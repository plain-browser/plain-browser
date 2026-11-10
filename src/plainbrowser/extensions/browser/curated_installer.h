// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_EXTENSIONS_BROWSER_CURATED_INSTALLER_H_
#define PLAINBROWSER_EXTENSIONS_BROWSER_CURATED_INSTALLER_H_

#include <string>
#include <string_view>

class Profile;

namespace content {
class WebContents;
}

namespace plainbrowser::extensions {

struct CuratedExtension;

// Installs our copy of a curated extension (catalog.h): downloads its CRX from
// ext.plain-browser.app, then hands it to Chromium's CrxInstaller with the
// standard install dialog (the user sees the permissions and confirms), the
// expected ID pinned (a CRX signed by another key is refused) and off-store
// installation allowed the way chrome://extensions allows a dropped file.
// The installed copy is an ordinary user-installed extension; Chromium's
// updater keeps it current through the update_url in its manifest.
//
// With `migrate`, the Chrome Web Store copy (entry.store_id) is replaced:
// it is disabled, its local settings (chrome.storage.local, the "Local
// Extension Settings/<id>" LevelDB, which holds e.g. uBlock Origin's filter
// selection and the user's own rules) are copied to our copy's ID before it
// is installed, and it is uninstalled once ours is in. Best effort: a store
// that cannot be copied (files in use) leaves ours with default settings.
//
// UI thread. Each install owns itself until it finishes; one at a time per
// profile and extension.
class CuratedInstaller {
 public:
  static void Start(Profile* profile,
                    content::WebContents* web_contents,
                    const CuratedExtension& entry,
                    bool migrate);

  // For the page (recommended_extensions_ui.cc).
  static bool IsInstalling(Profile* profile, std::string_view slug);
  // The error of the last failed install of `slug` in `profile`, or empty
  // (also after the user cancelled the dialog).
  static std::u16string LastError(Profile* profile, std::string_view slug);
};

}  // namespace plainbrowser::extensions

#endif  // PLAINBROWSER_EXTENSIONS_BROWSER_CURATED_INSTALLER_H_
