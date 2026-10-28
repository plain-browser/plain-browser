// Copyright Plain Browser contributors. MPL-2.0.
// StartCrashConsent: the production wiring of CrashConsent, kept apart so the
// unit tests link the class without chrome_elf and install_static.
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "plainbrowser/crash/browser/crash_consent.h"

namespace plainbrowser::crash {

void StartCrashConsent(PrefService* local_state) {
  static base::NoDestructor<CrashConsent> consent(
      local_state, install_static::GetRegistryPath(),
      base::BindRepeating(&SetUploadConsent_ExportThunk));
}

}  // namespace plainbrowser::crash
