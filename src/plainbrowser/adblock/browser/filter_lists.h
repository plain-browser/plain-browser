// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LISTS_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LISTS_H_

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"

class GURL;

// Downloaded filter lists: the manifest served by lists.plain-browser.app
// (infra/filter-lists) and their copy in the profile directory. Pure logic and
// blocking file IO, no network: FilterListUpdater does the fetching.
namespace plainbrowser::adblock::filter_lists {

// Lists the updater may replace, by the names AdBlockService::UseLists() and
// the Rust bridge use. plainbrowser-removeparam is ours and ships with the
// browser only.
inline constexpr std::array<std::string_view, 3> kUpdatableLists = {
    "easylist", "easyprivacy", "easylistpolish"};

// Where the manifest and the lists are served from (the Worker). Tests point
// the browser elsewhere with --plainbrowser-filter-lists-url.
inline constexpr char kDefaultServerUrl[] = "https://lists.plain-browser.app/v1/";

inline constexpr size_t kMaxManifestBytes = 64 * 1024;
inline constexpr size_t kMaxSignatureBytes = 1024;

// Ed25519 public keys (base64 of the 32 raw bytes) whose signature over the
// manifest is accepted; the Worker signs with the private key
// (infra/filter-lists/scripts/keygen.mjs). Empty entries are skipped, so with
// no key set every update is refused and the bundled lists stay in use. Two
// slots allow rotating to a new key one release before the Worker switches.
inline constexpr std::array<std::string_view, 2> kListSigningKeys = {
    "+ZuojnXniM+QYfewXOxhyv1RgwqkM81plLyg8wMYud0=",  // 2026-10-27
    "",  // Next key, for rotation.
};

// EasyList is about 2 MB; anything much bigger is not a list we serve.
inline constexpr size_t kMaxListBytes = 16 * 1024 * 1024;

// Directory in the user data directory that holds manifest.json and
// <name>.txt for every downloaded list.
inline constexpr base::FilePath::CharType kStoreDirName[] =
    FILE_PATH_LITERAL("PlainBrowser Filter Lists");

struct ListEntry {
  std::string sha256;  // Lower-case hex, 64 characters.
  int64_t size = 0;    // Bytes.
  bool operator==(const ListEntry&) const = default;
};
// By list name; only names in kUpdatableLists.
using Manifest = std::map<std::string, ListEntry>;

// The Worker's manifest:
//   {"format": 1, "lists": {"easylist": {"sha256": "<hex>", "size": 123,
//    ...}, ...}}
// Unknown lists and extra fields are ignored; an entry with a bad hash or a
// size outside 1..kMaxListBytes is dropped. nullopt when the JSON is not a
// format-1 manifest.
std::optional<Manifest> ParseManifest(std::string_view json);
std::string SerializeManifest(const Manifest& manifest);

// True when `text` is the list `entry` describes (size and SHA-256) and looks
// like an Adblock Plus list (first line "[Adblock Plus ...]").
bool ListMatches(const ListEntry& entry, std::string_view text);

// True when `signature_base64` (base64 of 64 bytes) is a valid Ed25519
// signature of the exact bytes of `manifest` by one of `public_keys` (base64 of
// 32 bytes; empty or malformed entries are skipped).
bool VerifyManifestSignature(std::string_view manifest,
                             std::string_view signature_base64,
                             base::span<const std::string_view> public_keys);

// <server>/lists/<name>.txt, <server>/manifest.json and its signature
// <server>/manifest.json.sig.
GURL ListUrl(const GURL& server, std::string_view name);
GURL ManifestUrl(const GURL& server);
GURL ManifestSignatureUrl(const GURL& server);

// The lists names in `wanted` whose entry differs from `stored`'s (new or
// changed), in kUpdatableLists order.
std::vector<std::string> ChangedLists(const Manifest& stored,
                                      const Manifest& wanted);

// The verified content of a store directory.
struct Store {
  Manifest manifest;  // Only entries whose file is present and matches.
  std::vector<std::pair<std::string, std::string>> lists;  // Name, text.
};

// Blocking. Reads `dir`; files that are missing or do not match the stored
// manifest are left out. Empty when the directory does not exist.
Store ReadStore(const base::FilePath& dir);

// Blocking. Writes the lists in `lists` (each must match its entry in
// `manifest`) and then `manifest` as the new manifest, each file atomically.
// Entries of `manifest` without a list in `lists` must already be on disk.
// Returns false when a write failed; the store then still reads back
// consistently (a file that does not match its entry is ignored).
bool WriteStore(const base::FilePath& dir,
                const Manifest& manifest,
                const std::vector<std::pair<std::string, std::string>>& lists);

// Blocking. Removes the store directory.
bool DeleteStore(const base::FilePath& dir);

}  // namespace plainbrowser::adblock::filter_lists

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_FILTER_LISTS_H_
