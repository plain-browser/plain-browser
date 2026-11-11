// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_EXTENSIONS_BROWSER_CATALOG_H_
#define PLAINBROWSER_EXTENSIONS_BROWSER_CATALOG_H_

#include <array>
#include <string_view>

class GURL;

// The curated extension repository: extensions Plain Browser hosts itself at
// ext.plain-browser.app (infra/extensions), repacked from their upstream
// releases with one change, an update_url in manifest.json pointing back at
// it, and signed with one key per extension. The extension ID follows from the
// key, so it is compiled in here and the installer refuses a CRX signed by any
// other key. Chromium's extension updater keeps them current through that
// update_url. The Chrome Web Store IDs are for moving a copy installed from
// there to ours (settings included).
namespace plainbrowser::extensions {

struct CuratedExtension {
  // Our name for it; also the file name on the server (<slug>.crx).
  std::string_view slug;
  // The ID of our copy (from our signing key).
  std::string_view id;
  // The ID of the Chrome Web Store copy, for migration and for noticing
  // another ad blocker; empty when there is none.
  std::string_view store_id;
  std::string_view name;
  // Grit message id of a one-line description (chrome/grit/generated_resources.h).
  int description_id;
  std::string_view homepage;
  int manifest_version;
  // Blocks ads by itself: the built-in blocker then does the same work twice.
  bool ad_blocker;
  // Archived upstream: no new versions will come.
  bool unmaintained;
};

// In the order the page lists them.
const std::array<CuratedExtension, 3>& Catalog();

const CuratedExtension* FindBySlug(std::string_view slug);

// Where our copy of `entry` is downloaded from.
GURL CrxUrl(const CuratedExtension& entry);

// The update manifest every CRX names as its update_url.
inline constexpr char kUpdateUrl[] =
    "https://ext.plain-browser.app/v1/updates.xml";

}  // namespace plainbrowser::extensions

#endif  // PLAINBROWSER_EXTENSIONS_BROWSER_CATALOG_H_
