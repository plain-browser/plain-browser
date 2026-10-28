// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_CRASH_BROWSER_CRASH_CONSENT_H_
#define PLAINBROWSER_CRASH_BROWSER_CRASH_CONSENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace plainbrowser::crash {

// Keeps Crashpad's upload consent equal to the "Send crash reports" pref:
// at start and on every change it writes the pref into the registry value
// chrome_elf reads at the next start (crash_constants.h) and tells the running
// Crashpad. Chromium ties crash uploads to metrics consent; Plain Browser has
// no metrics, so patches/95-crash/0001 points both places at this instead.
class CrashConsent {
 public:
  // Production values: StartCrashConsent() below.
  using SetUploadConsent = base::RepeatingCallback<void(bool)>;
  CrashConsent(PrefService* local_state,
               std::wstring registry_path,
               SetUploadConsent set_upload_consent);
  CrashConsent(const CrashConsent&) = delete;
  CrashConsent& operator=(const CrashConsent&) = delete;
  ~CrashConsent();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Writes |enabled| as the registry value under HKCU\|registry_path|. Blocks.
  static bool WriteRegistry(const std::wstring& registry_path, bool enabled);

 private:
  void Apply();

  raw_ptr<PrefService> local_state_;
  const std::wstring registry_path_;
  const SetUploadConsent set_upload_consent_;
  PrefChangeRegistrar registrar_;
  base::WeakPtrFactory<CrashConsent> weak_factory_{this};
};

// Starts the browser's CrashConsent (for the life of the process) with the
// install's registry path and chrome_elf's SetUploadConsent. Windows only.
void StartCrashConsent(PrefService* local_state);

}  // namespace plainbrowser::crash

#endif  // PLAINBROWSER_CRASH_BROWSER_CRASH_CONSENT_H_
