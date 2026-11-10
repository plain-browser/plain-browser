// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/extensions/browser/curated_installer.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install_prompt_data.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "plainbrowser/common/version.h"
#include "plainbrowser/extensions/browser/catalog.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_ui_types.h"

namespace plainbrowser::extensions {

namespace {

// AdGuard's CRX is about 20 MB; leave room for growth, but not for anything.
constexpr int64_t kMaxCrxBytes = 64 * 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("plainbrowser_curated_extensions", R"(
        semantics {
          sender: "Plain Browser recommended extensions"
          description:
            "Downloads an extension (uBlock Origin, AdGuard, NoScript or "
            "uMatrix) that Plain Browser hosts at ext.plain-browser.app, when "
            "the user clicks Install on chrome://recommended-extensions."
          trigger: "The user clicks Install or Switch on that page."
          data:
            "None. The request carries no cookies, only the browser's "
            "version in an X-Client header."
          destination: OTHER
          destination_other: "ext.plain-browser.app, a Cloudflare Worker"
        }
        policy {
          cookies_allowed: NO
          setting: "Only runs when the user installs an extension there."
          policy_exception_justification: "No enterprise policy."
        })");

using Key = std::pair<const void*, std::string>;

struct Status {
  bool installing = false;
  std::u16string error;
};

std::map<Key, Status>& Statuses() {
  static base::NoDestructor<std::map<Key, Status>> statuses;
  return *statuses;
}

// Blocking. Copies the chrome.storage.local database of `from` to `to` unless
// `to` already has one. LOCK is LevelDB's lock file, held while the database
// is open; the copy gets its own.
bool CopyLocalSettings(const base::FilePath& profile_dir,
                       const std::string& from,
                       const std::string& to) {
  const base::FilePath settings =
      profile_dir.Append(::extensions::kLocalExtensionSettingsDirectoryName);
  const base::FilePath source = settings.AppendASCII(from);
  const base::FilePath target = settings.AppendASCII(to);
  if (!base::DirectoryExists(source) || base::PathExists(target) ||
      !base::CreateDirectory(target)) {
    return false;
  }
  bool ok = true;
  base::FileEnumerator files(source, /*recursive=*/false,
                             base::FileEnumerator::FILES);
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    if (file.BaseName().value() == FILE_PATH_LITERAL("LOCK")) {
      continue;
    }
    ok &= base::CopyFile(file, target.Append(file.BaseName()));
  }
  if (!ok) {
    // A partial copy is worse than none: the extension starts clean instead.
    base::DeletePathRecursively(target);
  }
  return ok;
}

class Install {
 public:
  Install(Profile* profile,
          content::WebContents* web_contents,
          const CuratedExtension& entry,
          bool migrate)
      : profile_(profile->GetWeakPtr()),
        web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
        entry_(entry),
        migrate_(migrate && !entry.store_id.empty()) {}

  void Run() {
    SetStatus({.installing = true});
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = CrxUrl(entry_);
    request->method = "GET";
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    request->headers.SetHeader(
        "X-Client", std::string("PlainBrowser/") + PLAINBROWSER_VERSION);
    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               kTrafficAnnotation);
    loader_->DownloadToTempFile(
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        base::BindOnce(&Install::OnDownloaded, weak_factory_.GetWeakPtr()),
        kMaxCrxBytes);
  }

 private:
  void OnDownloaded(base::FilePath crx) {
    const network::mojom::URLResponseHead* head = loader_->ResponseInfo();
    const bool ok = !crx.empty() && head && head->headers &&
                    head->headers->response_code() == 200;
    loader_.reset();
    if (!ok) {
      if (!crx.empty()) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::BindOnce(base::IgnoreResult(&base::DeleteFile), crx));
      }
      Finish(l10n_util::GetStringUTF16(
          IDS_PLAINBROWSER_EXTENSIONS_DOWNLOAD_FAILED));
      return;
    }
    if (!profile_) {
      Finish(std::u16string());
      return;
    }
    const std::string store_id(entry_.store_id);
    if (!migrate_ || !::extensions::ExtensionRegistry::Get(profile_.get())
                          ->GetInstalledExtension(store_id)) {
      migrate_ = false;
      InstallCrx(crx);
      return;
    }
    // Stop the store copy first so it does not write while its settings are
    // copied.
    ::extensions::ExtensionRegistrar::Get(profile_.get())
        ->DisableExtension(store_id,
                           {::extensions::disable_reason::DISABLE_USER_ACTION});
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&CopyLocalSettings, profile_->GetPath(), store_id,
                       std::string(entry_.id)),
        base::BindOnce(
            [](base::WeakPtr<Install> self, base::FilePath crx, bool copied) {
              if (!self) {
                return;
              }
              LOG_IF(WARNING, !copied)
                  << "Plain Browser: could not copy the settings of the "
                     "Chrome Web Store copy of "
                  << self->entry_.name;
              self->InstallCrx(crx);
            },
            weak_factory_.GetWeakPtr(), crx));
  }

  void InstallCrx(const base::FilePath& crx) {
    if (!profile_) {
      Finish(std::u16string());
      return;
    }
    auto prompt_data = std::make_unique<::extensions::InstallPromptData>(
        ::extensions::InstallPromptData::UNSET_PROMPT_TYPE);
    std::unique_ptr<ExtensionInstallPrompt> prompt =
        web_contents_ ? std::make_unique<ExtensionInstallPrompt>(
                            web_contents_.get(), std::move(prompt_data))
                      : std::make_unique<ExtensionInstallPrompt>(
                            profile_.get(), gfx::NativeWindow(),
                            std::move(prompt_data));
    scoped_refptr<::extensions::CrxInstaller> installer =
        ::extensions::CrxInstaller::Create(profile_.get(), std::move(prompt));
    // Our key or nothing: CrxInstaller compares the ID the CRX's key gives
    // with this one.
    installer->set_expected_id(std::string(entry_.id));
    installer->set_install_source(
        ::extensions::mojom::ManifestLocation::kInternal);
    installer->set_off_store_install_allow_reason(
        ::extensions::CrxInstaller::OffStoreInstallAllowedFromSettingsPage);
    installer->set_error_on_unsupported_requirements(true);
    installer->set_delete_source(true);
    installer->AddInstallerCallback(base::BindOnce(
        &Install::OnInstalled, weak_factory_.GetWeakPtr()));
    installer->InstallCrx(crx);
  }

  void OnInstalled(const std::optional<::extensions::CrxInstallError>& error) {
    if (error) {
      // A cancelled dialog is not an error to show.
      const bool cancelled =
          error->detail() ==
              ::extensions::CrxInstallErrorDetail::USER_CANCELED ||
          error->detail() == ::extensions::CrxInstallErrorDetail::USER_ABORTED;
      if (migrate_ && profile_) {
        // Ours did not go in: give the user the store copy back.
        ::extensions::ExtensionRegistrar::Get(profile_.get())
            ->EnableExtension(std::string(entry_.store_id));
      }
      Finish(cancelled ? std::u16string() : error->message());
      return;
    }
    if (migrate_ && profile_) {
      ::extensions::ExtensionRegistrar::Get(profile_.get())
          ->UninstallExtension(
              std::string(entry_.store_id),
              ::extensions::UNINSTALL_REASON_USER_INITIATED, nullptr);
    }
    Finish(std::u16string());
  }

  void SetStatus(Status status) {
    Statuses()[{profile_.get(), std::string(entry_.slug)}] = std::move(status);
  }

  // Ends the install and deletes this.
  void Finish(std::u16string error) {
    if (profile_) {
      SetStatus({.installing = false, .error = std::move(error)});
    }
    delete this;
  }

  base::WeakPtr<Profile> profile_;
  base::WeakPtr<content::WebContents> web_contents_;
  const CuratedExtension& entry_;
  bool migrate_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::WeakPtrFactory<Install> weak_factory_{this};
};

}  // namespace

// static
void CuratedInstaller::Start(Profile* profile,
                             content::WebContents* web_contents,
                             const CuratedExtension& entry,
                             bool migrate) {
  if (!profile || IsInstalling(profile, entry.slug)) {
    return;
  }
  // Owns itself until Finish().
  (new Install(profile, web_contents, entry, migrate))->Run();
}

// static
bool CuratedInstaller::IsInstalling(Profile* profile, std::string_view slug) {
  auto it = Statuses().find({profile, std::string(slug)});
  return it != Statuses().end() && it->second.installing;
}

// static
std::u16string CuratedInstaller::LastError(Profile* profile,
                                           std::string_view slug) {
  auto it = Statuses().find({profile, std::string(slug)});
  return it != Statuses().end() ? it->second.error : std::u16string();
}

}  // namespace plainbrowser::extensions
