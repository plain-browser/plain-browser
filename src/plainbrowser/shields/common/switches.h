// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_COMMON_SWITCHES_H_
#define PLAINBROWSER_SHIELDS_COMMON_SWITCHES_H_

namespace plainbrowser::shields::switches {

// Browser -> renderer: the profile's per-site exception list (comma-separated
// eTLD+1 sites) at the time the renderer process was created. Only the initial
// value of the renderer's ShieldsConfig list, so a document created before the
// first mojom::ShieldsConfig push already sees it; every push replaces it.
// Never read from the user's command line.
inline constexpr char kShieldsExceptions[] = "plainbrowser-shields-exceptions";

}  // namespace plainbrowser::shields::switches

#endif  // PLAINBROWSER_SHIELDS_COMMON_SWITCHES_H_
