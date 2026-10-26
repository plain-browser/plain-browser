// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/browser/renderer_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "plainbrowser/fingerprint/common/features.h"
#include "plainbrowser/fingerprint/common/switches.h"

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

void AppendRendererCommandLineSwitches(base::CommandLine* command_line) {
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
