// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/browser/renderer_switches.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "plainbrowser/fingerprint/common/features.h"
#include "plainbrowser/fingerprint/common/switches.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/switches.h"

namespace plainbrowser::fingerprint {

namespace {

// 128 random bits, generated once per browser process. Not persisted: a new
// run gives every site a new fingerprint, which is the point.
const std::string& SessionKey() {
  static const base::NoDestructor<std::string> key(
      base::HexEncode(base::RandBytesAsVector(16)));
  return *key;
}

}  // namespace

void AppendRendererCommandLineSwitches(base::CommandLine* command_line,
                                       int child_process_id) {
  // The profile's per-site exceptions, as the initial value of the renderer's
  // ShieldsConfig list (a document may be created before the first push
  // arrives). Kept apart from --fingerprint-exceptions: that switch is fixed
  // for the life of the process, this list changes with the pref.
  content::RenderProcessHost* process =
      content::RenderProcessHost::FromID(child_process_id);
  if (process && process->GetBrowserContext()) {
    const std::vector<std::string> sites = shields::GetExceptions(
        user_prefs::UserPrefs::Get(process->GetBrowserContext()));
    if (!sites.empty()) {
      command_line->AppendSwitchASCII(shields::switches::kShieldsExceptions,
                                      base::JoinString(sites, ","));
    }
  }
  if (!base::FeatureList::IsEnabled(kPlainBrowserFingerprintProtection)) {
    return;
  }
  command_line->AppendSwitchASCII(switches::kFingerprintSessionKey,
                                  SessionKey());
  const base::CommandLine& browser = *base::CommandLine::ForCurrentProcess();
  if (browser.HasSwitch(switches::kFingerprintExceptions)) {
    command_line->AppendSwitchASCII(
        switches::kFingerprintExceptions,
        browser.GetSwitchValueASCII(switches::kFingerprintExceptions));
  }
}

}  // namespace plainbrowser::fingerprint
