// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_
#define PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace plainbrowser::fingerprint {

// Called from ChromeContentBrowserClient::AppendExtraCommandLineSwitches for
// renderer processes (patches/80-fingerprint). Passes the session key (one
// random value per browser run, the same for every renderer) and the exception
// list to the renderer: the --fingerprint-exceptions switch of the browser
// plus the per-site exceptions of the profile the process belongs to
// (`child_process_id` is the renderer's RenderProcessHost id). Later changes
// of the pref reach running renderers over mojom::ShieldsConfig.
void AppendRendererCommandLineSwitches(base::CommandLine* command_line,
                                       int child_process_id);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_BROWSER_RENDERER_SWITCHES_H_
