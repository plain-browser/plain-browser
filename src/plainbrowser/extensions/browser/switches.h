// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_EXTENSIONS_BROWSER_SWITCHES_H_
#define PLAINBROWSER_EXTENSIONS_BROWSER_SWITCHES_H_

namespace plainbrowser::extensions {

// Called once in the browser process after start-up
// (ChromeBrowserMainParts::PostBrowserStart, patches/60-ui/0014), before any
// download can reach the extension installer.
//
// Without the Chrome Web Store there is no "Add to Chrome" button, so a .crx
// file is how an extension gets in. ungoogled-chromium's chrome://flags entry
// "Handling of extension MIME type requests" (switch
// --extension-mime-request-handling) decides what opening one does; by
// default Chromium refuses it ("cannot be added from this website"). Plain
// Browser makes "Always prompt for install" the default: a .crx from any site
// shows the usual install dialog with the extension's permissions, and nothing
// is installed without the user's click. Choosing another value in
// chrome://flags still wins (about_flags puts the switch on the command line
// before this runs).
void ApplyDefaultSwitches();

}  // namespace plainbrowser::extensions

#endif  // PLAINBROWSER_EXTENSIONS_BROWSER_SWITCHES_H_
