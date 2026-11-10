// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/extensions/browser/switches.h"

#include "base/command_line.h"

namespace plainbrowser::extensions {

namespace {

// ungoogled-chromium's add-flag-to-configure-extension-downloading.patch.
constexpr char kMimeHandlingSwitch[] = "extension-mime-request-handling";
constexpr char kAlwaysPromptForInstall[] = "always-prompt-for-install";

}  // namespace

void ApplyDefaultSwitches() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(kMimeHandlingSwitch)) {
    cl->AppendSwitchASCII(kMimeHandlingSwitch, kAlwaysPromptForInstall);
  }
}

}  // namespace plainbrowser::extensions
