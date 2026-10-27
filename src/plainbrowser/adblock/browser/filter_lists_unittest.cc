// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/filter_lists.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"

namespace plainbrowser::adblock::filter_lists {
namespace {

constexpr char kList[] = "[Adblock Plus 2.0]\n! Title: test\n||ads.test^\n";

ListEntry EntryFor(const std::string& text) {
  return {base::HexEncodeLower(crypto::hash::Sha256(text)),
          static_cast<int64_t>(text.size())};
}

std::string ManifestJson(const std::string& lists) {
  return R"({"format": 1, "generated": "2026-10-27T12:00:00Z", "lists": {)" +
         lists + "}}";
}

TEST(FilterListsTest, ParsesTheWorkersManifest) {
  const ListEntry e = EntryFor(kList);
  std::optional<Manifest> m = ParseManifest(ManifestJson(
      R"("easylist": {"sha256": ")" + e.sha256 + R"(", "size": )" +
      base::NumberToString(e.size) +
      R"(, "updated": "x", "source": "https://easylist.to/"})"));
  ASSERT_TRUE(m);
  ASSERT_EQ(1u, m->size());
  EXPECT_EQ(e, m->at("easylist"));
}

TEST(FilterListsTest, DropsUnknownListsAndBadEntries) {
  const std::string good = EntryFor(kList).sha256;
  std::optional<Manifest> m = ParseManifest(ManifestJson(
      R"("other": {"sha256": ")" + good + R"(", "size": 10},)"
      R"("plainbrowser-removeparam": {"sha256": ")" + good +
      R"(", "size": 10},)"
      R"("easylist": {"sha256": "ABC", "size": 10},)"
      R"("easyprivacy": {"sha256": ")" + good + R"(", "size": 0},)"
      R"("easylistpolish": {"sha256": ")" + good + R"(", "size": 1.5})"));
  ASSERT_TRUE(m);
  EXPECT_TRUE(m->empty());
}

TEST(FilterListsTest, RejectsWhatIsNotAFormat1Manifest) {
  EXPECT_FALSE(ParseManifest("not json"));
  EXPECT_FALSE(ParseManifest(R"({"format": 2, "lists": {}})"));
  EXPECT_FALSE(ParseManifest(R"({"format": 1})"));
  EXPECT_FALSE(
      ParseManifest(std::string(kMaxManifestBytes + 1, ' ') + "{}"));
  EXPECT_TRUE(ParseManifest(R"({"format": 1, "lists": {}})"));
}

TEST(FilterListsTest, SerializedManifestParsesBack) {
  Manifest m = {{"easylist", EntryFor(kList)},
                {"easyprivacy", EntryFor(std::string(kList) + "x")}};
  EXPECT_EQ(m, ParseManifest(SerializeManifest(m)));
}

TEST(FilterListsTest, ListMatchesChecksSizeHashAndHeader) {
  const ListEntry e = EntryFor(kList);
  EXPECT_TRUE(ListMatches(e, kList));
  EXPECT_FALSE(ListMatches(e, std::string(kList) + "x"));
  ListEntry wrong_hash = e;
  wrong_hash.sha256[0] = wrong_hash.sha256[0] == '0' ? '1' : '0';
  EXPECT_FALSE(ListMatches(wrong_hash, kList));
  // Right hash, but not an Adblock Plus list (an HTML error page, say).
  const std::string html = "<html>502</html>";
  EXPECT_FALSE(ListMatches(EntryFor(html), html));
  const std::string bom = std::string("\xEF\xBB\xBF") + kList;
  EXPECT_TRUE(ListMatches(EntryFor(bom), bom));
}

TEST(FilterListsTest, ManifestSignatureIsEd25519OverTheExactBytes) {
  uint8_t public_key[ED25519_PUBLIC_KEY_LEN];
  uint8_t private_key[ED25519_PRIVATE_KEY_LEN];
  ED25519_keypair(public_key, private_key);
  uint8_t other_public[ED25519_PUBLIC_KEY_LEN];
  uint8_t other_private[ED25519_PRIVATE_KEY_LEN];
  ED25519_keypair(other_public, other_private);

  const std::string manifest = ManifestJson("");
  uint8_t signature[ED25519_SIGNATURE_LEN];
  ASSERT_EQ(1, ED25519_sign(signature,
                            reinterpret_cast<const uint8_t*>(manifest.data()),
                            manifest.size(), private_key));
  const std::string sig = base::Base64Encode(signature);
  const std::string key = base::Base64Encode(public_key);
  const std::string other = base::Base64Encode(other_public);

  const std::string_view right[] = {"", key};
  EXPECT_TRUE(VerifyManifestSignature(manifest, sig, right));
  EXPECT_TRUE(VerifyManifestSignature(manifest, sig + "\n", right));
  // Any change to the bytes, another key, or a broken signature fails.
  EXPECT_FALSE(VerifyManifestSignature(manifest + " ", sig, right));
  const std::string_view wrong[] = {other};
  EXPECT_FALSE(VerifyManifestSignature(manifest, sig, wrong));
  EXPECT_FALSE(VerifyManifestSignature(manifest, "not base64!", right));
  EXPECT_FALSE(VerifyManifestSignature(manifest, base::Base64Encode("short"),
                                       right));
  // A key that is not compiled in (the production keys) is refused.
  EXPECT_FALSE(VerifyManifestSignature(manifest, sig, kListSigningKeys));
}

TEST(FilterListsTest, Urls) {
  const GURL server(kDefaultServerUrl);
  EXPECT_EQ(GURL("https://lists.plain-browser.app/v1/manifest.json"),
            ManifestUrl(server));
  EXPECT_EQ(GURL("https://lists.plain-browser.app/v1/lists/easylist.txt"),
            ListUrl(server, "easylist"));
  EXPECT_EQ(GURL("https://lists.plain-browser.app/v1/manifest.json.sig"),
            ManifestSignatureUrl(server));
}

TEST(FilterListsTest, ChangedListsAreTheNewAndTheDifferentOnes) {
  const ListEntry a = EntryFor(kList);
  const ListEntry b = EntryFor(std::string(kList) + "b");
  const Manifest stored = {{"easylist", a}, {"easyprivacy", a}};
  const Manifest wanted = {
      {"easylist", a}, {"easyprivacy", b}, {"easylistpolish", a}};
  EXPECT_EQ((std::vector<std::string>{"easyprivacy", "easylistpolish"}),
            ChangedLists(stored, wanted));
  EXPECT_TRUE(ChangedLists(wanted, wanted).empty());
  // A list the server stopped serving is not "changed": the stored one stays.
  EXPECT_TRUE(ChangedLists(wanted, Manifest{{"easylist", a}}).empty());
}

TEST(FilterListsTest, StoreRoundTripAndVerification) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  const base::FilePath dir = temp.GetPath().Append(kStoreDirName);
  EXPECT_TRUE(ReadStore(dir).lists.empty());  // No directory yet.

  const std::string privacy = std::string(kList) + "||track.test^\n";
  const Manifest manifest = {{"easylist", EntryFor(kList)},
                             {"easyprivacy", EntryFor(privacy)}};
  ASSERT_TRUE(WriteStore(dir, manifest,
                         {{"easylist", kList}, {"easyprivacy", privacy}}));
  Store store = ReadStore(dir);
  EXPECT_EQ(manifest, store.manifest);
  ASSERT_EQ(2u, store.lists.size());

  // A damaged file is left out; the other list is still used.
  ASSERT_TRUE(base::WriteFile(dir.AppendASCII("easyprivacy.txt"), "garbage"));
  store = ReadStore(dir);
  EXPECT_EQ((Manifest{{"easylist", EntryFor(kList)}}), store.manifest);
  ASSERT_EQ(1u, store.lists.size());
  EXPECT_EQ("easylist", store.lists[0].first);
  EXPECT_EQ(kList, store.lists[0].second);

  // An update writes only the changed list next to the kept one.
  const Manifest next = {{"easylist", EntryFor(kList)},
                         {"easyprivacy", EntryFor(privacy + "x")}};
  ASSERT_TRUE(WriteStore(dir, next, {{"easyprivacy", privacy + "x"}}));
  EXPECT_EQ(next, ReadStore(dir).manifest);

  EXPECT_TRUE(DeleteStore(dir));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(ReadStore(dir).lists.empty());
}

}  // namespace
}  // namespace plainbrowser::adblock::filter_lists
