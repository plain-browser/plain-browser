// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_
#define PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace plainbrowser::fingerprint {

// Called from ChromeContentBrowserClient::AppendExtraCommandLineSwitches for
// renderer processes (patches/80-fingerprint). Passes the session key (one
// random value per browser run, the same for every renderer) and the user's
// exception list to the renderer.
void AppendRendererCommandLineSwitches(base::CommandLine* command_line);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_
