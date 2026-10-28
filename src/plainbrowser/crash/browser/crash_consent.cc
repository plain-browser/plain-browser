// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/crash/browser/crash_consent.h"

#include <windows.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "plainbrowser/crash/common/crash_constants.h"
#include "plainbrowser/crash/common/pref_names.h"

namespace plainbrowser::crash {

CrashConsent::CrashConsent(PrefService* local_state,
                           std::wstring registry_path,
                           SetUploadConsent set_upload_consent)
    : local_state_(local_state),
      registry_path_(std::move(registry_path)),
      set_upload_consent_(std::move(set_upload_consent)) {
  registrar_.Init(local_state_);
  registrar_.Add(prefs::kCrashReportsEnabled,
                 base::BindRepeating(&CrashConsent::Apply,
                                     weak_factory_.GetWeakPtr()));
  // Also at start: the registry may be stale (a profile copied from another
  // machine, a value written by an older build).
  Apply();
}

CrashConsent::~CrashConsent() = default;

// static
void CrashConsent::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCrashReportsEnabled, false);
}

// static
bool CrashConsent::WriteRegistry(const std::wstring& registry_path,
                                 bool enabled) {
  // KEY_WOW64_32KEY like install_static, which reads it with nt::WOW6432.
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER, registry_path.c_str(),
                 KEY_SET_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS) {
    return false;
  }
  return key.WriteValue(kConsentRegValue, enabled ? 1u : 0u) == ERROR_SUCCESS;
}

void CrashConsent::Apply() {
  const bool enabled = local_state_->GetBoolean(prefs::kCrashReportsEnabled);
  set_upload_consent_.Run(enabled);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                  base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&CrashConsent::WriteRegistry),
                     registry_path_, enabled));
}

}  // namespace plainbrowser::crash
