// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/filter_list_updater.h"

#include <algorithm>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/common/pref_names.h"
#include "plainbrowser/common/version.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace plainbrowser::adblock {

namespace {

// Test hook: another server for the manifest and the lists (an http:// URL is
// accepted only here), and a first check a few seconds after start-up.
constexpr char kServerSwitch[] = "plainbrowser-filter-lists-url";
// Test hook, honoured only together with kServerSwitch: one more accepted
// signing key (base64 of the raw Ed25519 public key).
constexpr char kKeySwitch[] = "plainbrowser-filter-lists-key";

constexpr base::TimeDelta kFirstCheckDelay = base::Minutes(2);
constexpr base::TimeDelta kTestFirstCheckDelay = base::Seconds(3);
constexpr base::TimeDelta kCheckInterval = base::Hours(24);
constexpr base::TimeDelta kRetryDelay = base::Hours(1);

FilterListUpdater* g_updater = nullptr;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("plainbrowser_filter_lists", R"(
        semantics {
          sender: "Plain Browser filter list updater"
          description:
            "Downloads updates of the ad and tracker filter lists (EasyList, "
            "EasyPrivacy, EasyList Polish) from lists.plain-browser.app, a "
            "mirror run by the Plain Browser project."
          trigger:
            "Once a day while the browser runs, a few minutes after start-up."
          data:
            "None. The request carries no cookies and no user data, only the "
            "browser's major.minor.patch version in an X-Client header."
          destination: OTHER
          destination_other: "lists.plain-browser.app, a Cloudflare Worker"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Settings > Privacy and security > Update filter lists. With it "
            "off the browser uses the lists it was released with."
          policy_exception_justification: "No enterprise policy."
        })");

GURL ServerUrl() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kServerSwitch)) {
    GURL url(cl->GetSwitchValueASCII(kServerSwitch));
    if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
      return url;
    }
  }
  return GURL(filter_lists::kDefaultServerUrl);
}

}  // namespace

// static
void FilterListUpdater::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kListUpdatesEnabled, true);
  registry->RegisterTimePref(prefs::kListUpdatesLastCheck, base::Time());
}

// static
void FilterListUpdater::Start(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const base::FilePath& user_data_dir) {
  if (g_updater || !local_state || !factory || user_data_dir.empty()) {
    return;
  }
  // Lives until the process exits, like AdBlockService.
  g_updater =
      new FilterListUpdater(local_state, std::move(factory), user_data_dir);
}

FilterListUpdater::FilterListUpdater(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const base::FilePath& user_data_dir)
    : local_state_(local_state),
      factory_(std::move(factory)),
      store_dir_(user_data_dir.Append(filter_lists::kStoreDirName)),
      server_(ServerUrl()),
      file_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  registrar_.Init(local_state_);
  registrar_.Add(prefs::kListUpdatesEnabled,
                 base::BindRepeating(&FilterListUpdater::OnEnabledChanged,
                                     base::Unretained(this)));
  if (!Enabled()) {
    // Lists stored before the user turned updates off are not used; remove
    // them if a previous run could not.
    file_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&filter_lists::DeleteStore),
                       store_dir_));
    return;
  }
  LoadStore();
  const bool test_server =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kServerSwitch);
  const base::Time next =
      local_state_->GetTime(prefs::kListUpdatesLastCheck) + kCheckInterval;
  const base::TimeDelta min_delay =
      test_server ? kTestFirstCheckDelay : kFirstCheckDelay;
  ScheduleCheck(test_server ? min_delay
                            : std::clamp(next - base::Time::Now(), min_delay,
                                         kCheckInterval));
}

FilterListUpdater::~FilterListUpdater() = default;

bool FilterListUpdater::Enabled() const {
  return local_state_->GetBoolean(prefs::kListUpdatesEnabled);
}

void FilterListUpdater::OnEnabledChanged() {
  // Drop whatever was in flight; replies of the old state are ignored.
  weak_factory_.InvalidateWeakPtrs();
  timer_.Stop();
  loader_.reset();
  pending_.clear();
  fetched_.clear();
  if (Enabled()) {
    LoadStore();
    ScheduleCheck(kFirstCheckDelay);
    return;
  }
  stored_.clear();
  if (using_downloaded_) {
    using_downloaded_ = false;
    AdBlockService::GetInstance()->UseLists({});
  }
  file_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&filter_lists::DeleteStore),
                                store_dir_));
}

void FilterListUpdater::LoadStore() {
  file_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&filter_lists::ReadStore, store_dir_),
      base::BindOnce(&FilterListUpdater::OnStoreRead,
                     weak_factory_.GetWeakPtr()));
}

void FilterListUpdater::OnStoreRead(filter_lists::Store store) {
  stored_ = std::move(store.manifest);
  if (store.lists.empty() && !using_downloaded_) {
    return;  // Nothing downloaded yet: the compiled-in engine stays.
  }
  using_downloaded_ = !store.lists.empty();
  VLOG(1) << "Plain Browser filter lists: using " << store.lists.size()
          << " downloaded list(s)";
  AdBlockService::GetInstance()->UseLists(std::move(store.lists));
}

void FilterListUpdater::ScheduleCheck(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&FilterListUpdater::Check,
                              base::Unretained(this)));
}

void FilterListUpdater::Check() {
  if (!Enabled()) {
    return;
  }
  Fetch(filter_lists::ManifestUrl(server_), filter_lists::kMaxManifestBytes,
        base::BindOnce(&FilterListUpdater::OnManifest,
                       weak_factory_.GetWeakPtr()));
}

void FilterListUpdater::Fetch(
    const GURL& url,
    size_t max_bytes,
    base::OnceCallback<void(std::unique_ptr<std::string>)> done) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // The Worker sets its own cache headers; do not let a stale HTTP cache entry
  // hide an update for a day.
  request->load_flags = net::LOAD_BYPASS_CACHE;
  request->headers.SetHeader("X-Client",
                             std::string("PlainBrowser/") +
                                 PLAINBROWSER_VERSION);
  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             kTrafficAnnotation);
  loader_->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  loader_->DownloadToString(
      factory_.get(),
      base::BindOnce(
          [](FilterListUpdater* self,
             base::OnceCallback<void(std::unique_ptr<std::string>)> done,
             std::optional<std::string> body) {
            const network::mojom::URLResponseHead* head =
                self->loader_->ResponseInfo();
            const bool ok = body && head && head->headers &&
                            head->headers->response_code() == net::HTTP_OK;
            self->loader_.reset();
            std::move(done).Run(
                ok ? std::make_unique<std::string>(std::move(*body)) : nullptr);
          },
          base::Unretained(this), std::move(done)),
      max_bytes);
}

void FilterListUpdater::OnManifest(std::unique_ptr<std::string> body) {
  if (!body) {
    VLOG(1) << "Plain Browser filter lists: no manifest from "
            << server_.spec();
    FinishCheck(/*success=*/false);
    return;
  }
  Fetch(filter_lists::ManifestSignatureUrl(server_),
        filter_lists::kMaxSignatureBytes,
        base::BindOnce(&FilterListUpdater::OnSignature,
                       weak_factory_.GetWeakPtr(), std::move(*body)));
}

void FilterListUpdater::OnSignature(std::string manifest_json,
                                    std::unique_ptr<std::string> body) {
  std::vector<std::string_view> keys(filter_lists::kListSigningKeys.begin(),
                                     filter_lists::kListSigningKeys.end());
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  const std::string test_key = cl->HasSwitch(kServerSwitch)
                                   ? cl->GetSwitchValueASCII(kKeySwitch)
                                   : std::string();
  if (!test_key.empty()) {
    keys.push_back(test_key);
  }
  if (!body ||
      !filter_lists::VerifyManifestSignature(manifest_json, *body, keys)) {
    // Also the case while no key is compiled in: nothing is downloaded.
    LOG(WARNING) << "Plain Browser filter lists: manifest signature missing "
                    "or not valid; keeping the current lists";
    FinishCheck(/*success=*/false);
    return;
  }
  std::optional<filter_lists::Manifest> manifest =
      filter_lists::ParseManifest(manifest_json);
  if (!manifest) {
    VLOG(1) << "Plain Browser filter lists: no valid manifest from "
            << server_.spec();
    FinishCheck(/*success=*/false);
    return;
  }
  wanted_ = std::move(*manifest);
  pending_ = filter_lists::ChangedLists(stored_, wanted_);
  fetched_.clear();
  FetchNextList();
}

void FilterListUpdater::FetchNextList() {
  if (pending_.empty()) {
    if (fetched_.empty()) {
      FinishCheck(/*success=*/true);  // Nothing new.
      return;
    }
    // New manifest = the stored entries still served, plus the new ones.
    filter_lists::Manifest next;
    for (const auto& [name, text] : fetched_) {
      next[name] = wanted_[name];
    }
    for (const auto& [name, entry] : stored_) {
      if (!next.contains(name)) {
        next[name] = entry;
      }
    }
    file_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&filter_lists::WriteStore, store_dir_, std::move(next),
                       std::move(fetched_)),
        base::BindOnce(
            [](base::WeakPtr<FilterListUpdater> self, bool written) {
              if (!self) {
                return;
              }
              if (!written) {
                LOG(WARNING) << "Plain Browser filter lists: could not write "
                             << self->store_dir_;
              }
              // Load what is on disk now (verified again): one code path for
              // start-up and updates.
              self->LoadStore();
              self->FinishCheck(written);
            },
            weak_factory_.GetWeakPtr()));
    fetched_.clear();
    return;
  }
  std::string name = pending_.back();
  pending_.pop_back();
  Fetch(filter_lists::ListUrl(server_, name), filter_lists::kMaxListBytes,
        base::BindOnce(&FilterListUpdater::OnList, weak_factory_.GetWeakPtr(),
                       name));
}

void FilterListUpdater::OnList(std::string name,
                               std::unique_ptr<std::string> body) {
  // A list that does not match the manifest (a new version published between
  // the two requests, or a broken download) is skipped; the next check gets it.
  if (body && filter_lists::ListMatches(wanted_[name], *body)) {
    fetched_.emplace_back(std::move(name), std::move(*body));
  } else {
    VLOG(1) << "Plain Browser filter lists: " << name
            << " did not match the manifest";
  }
  FetchNextList();
}

void FilterListUpdater::FinishCheck(bool success) {
  local_state_->SetTime(prefs::kListUpdatesLastCheck, base::Time::Now());
  ScheduleCheck(success ? kCheckInterval : kRetryDelay);
}

}  // namespace plainbrowser::adblock
