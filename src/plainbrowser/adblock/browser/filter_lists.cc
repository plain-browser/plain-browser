// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/filter_lists.h"

#include <algorithm>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"

namespace plainbrowser::adblock::filter_lists {

namespace {

constexpr char kManifestFile[] = "manifest.json";
constexpr int kFormat = 1;

bool Updatable(std::string_view name) {
  return std::find(kUpdatableLists.begin(), kUpdatableLists.end(), name) !=
         kUpdatableLists.end();
}

bool IsSha256Hex(std::string_view s) {
  return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
           return base::IsAsciiDigit(c) || (c >= 'a' && c <= 'f');
         });
}

base::FilePath ListPath(const base::FilePath& dir, std::string_view name) {
  return dir.AppendASCII(std::string(name) + ".txt");
}

}  // namespace

std::optional<Manifest> ParseManifest(std::string_view json) {
  if (json.size() > kMaxManifestBytes) {
    return std::nullopt;
  }
  std::optional<base::DictValue> root =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!root || root->FindInt("format") != kFormat) {
    return std::nullopt;
  }
  const base::DictValue* lists = root->FindDict("lists");
  if (!lists) {
    return std::nullopt;
  }
  Manifest manifest;
  for (const auto [name, value] : *lists) {
    const base::DictValue* entry = value.GetIfDict();
    if (!Updatable(name) || !entry) {
      continue;
    }
    const std::string* sha256 = entry->FindString("sha256");
    // JSON numbers are doubles; list sizes are far below 2^53.
    const std::optional<double> size = entry->FindDouble("size");
    if (!sha256 || !IsSha256Hex(*sha256) || !size || *size < 1 ||
        *size > kMaxListBytes || *size != static_cast<int64_t>(*size)) {
      continue;
    }
    manifest[name] = ListEntry{*sha256, static_cast<int64_t>(*size)};
  }
  return manifest;
}

std::string SerializeManifest(const Manifest& manifest) {
  base::DictValue lists;
  for (const auto& [name, entry] : manifest) {
    lists.Set(name, base::DictValue()
                        .Set("sha256", entry.sha256)
                        .Set("size", static_cast<double>(entry.size)));
  }
  return base::WriteJson(base::DictValue()
                             .Set("format", kFormat)
                             .Set("lists", std::move(lists)))
      .value_or(std::string());
}

bool ListMatches(const ListEntry& entry, std::string_view text) {
  if (static_cast<int64_t>(text.size()) != entry.size) {
    return false;
  }
  if (base::HexEncodeLower(crypto::hash::Sha256(text)) != entry.sha256) {
    return false;
  }
  // A UTF-8 BOM may precede the header.
  if (text.starts_with("\xEF\xBB\xBF")) {
    text.remove_prefix(3);
  }
  return text.starts_with("[Adblock Plus");
}

GURL ListUrl(const GURL& server, std::string_view name) {
  return server.Resolve("lists/" + std::string(name) + ".txt");
}

GURL ManifestUrl(const GURL& server) {
  return server.Resolve("manifest.json");
}

GURL ManifestSignatureUrl(const GURL& server) {
  return server.Resolve("manifest.json.sig");
}

bool VerifyManifestSignature(std::string_view manifest,
                             std::string_view signature_base64,
                             base::span<const std::string_view> public_keys) {
  std::optional<std::vector<uint8_t>> signature = base::Base64Decode(
      base::TrimWhitespaceASCII(signature_base64, base::TRIM_ALL));
  if (!signature || signature->size() != ED25519_SIGNATURE_LEN) {
    return false;
  }
  for (std::string_view key_base64 : public_keys) {
    std::optional<std::vector<uint8_t>> key = base::Base64Decode(key_base64);
    if (!key || key->size() != ED25519_PUBLIC_KEY_LEN) {
      continue;
    }
    if (ED25519_verify(reinterpret_cast<const uint8_t*>(manifest.data()),
                       manifest.size(), signature->data(), key->data()) == 1) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> ChangedLists(const Manifest& stored,
                                      const Manifest& wanted) {
  std::vector<std::string> changed;
  for (std::string_view name : kUpdatableLists) {
    auto want = wanted.find(std::string(name));
    if (want == wanted.end()) {
      continue;
    }
    auto have = stored.find(std::string(name));
    if (have == stored.end() || !(have->second == want->second)) {
      changed.emplace_back(name);
    }
  }
  return changed;
}

Store ReadStore(const base::FilePath& dir) {
  Store store;
  std::string json;
  if (!base::ReadFileToStringWithMaxSize(dir.AppendASCII(kManifestFile), &json,
                                         kMaxManifestBytes)) {
    return store;
  }
  std::optional<Manifest> manifest = ParseManifest(json);
  if (!manifest) {
    return store;
  }
  for (const auto& [name, entry] : *manifest) {
    std::string text;
    if (base::ReadFileToStringWithMaxSize(ListPath(dir, name), &text,
                                          kMaxListBytes) &&
        ListMatches(entry, text)) {
      store.manifest[name] = entry;
      store.lists.emplace_back(name, std::move(text));
    }
  }
  return store;
}

bool WriteStore(const base::FilePath& dir,
                const Manifest& manifest,
                const std::vector<std::pair<std::string, std::string>>& lists) {
  if (!base::CreateDirectory(dir)) {
    return false;
  }
  for (const auto& [name, text] : lists) {
    if (!Updatable(name) ||
        !base::ImportantFileWriter::WriteFileAtomically(ListPath(dir, name),
                                                        text)) {
      return false;
    }
  }
  return base::ImportantFileWriter::WriteFileAtomically(
      dir.AppendASCII(kManifestFile), SerializeManifest(manifest));
}

bool DeleteStore(const base::FilePath& dir) {
  return base::DeletePathRecursively(dir);
}

}  // namespace plainbrowser::adblock::filter_lists
