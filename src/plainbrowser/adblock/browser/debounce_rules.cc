// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/debounce_rules.h"

#include <string>

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/third_party/mozilla/url_parse.h"

namespace plainbrowser::adblock {

// Feature name "PlainBrowserDebounce" (derived from the identifier).
BASE_FEATURE(kPlainBrowserDebounce, base::FEATURE_ENABLED_BY_DEFAULT);

namespace debounce {

namespace {

// Redirectors whose only job is to record the click before sending the user
// on. Keep to services where the target is in the URL itself; a redirector
// that decides the target server side (t.co, bit.ly) cannot be skipped.
// Own list, MPL-2.0; docs/ADBLOCK.md, "Debouncing".
constexpr Rule kRules[] = {
    // Google search results and Gmail links (google.com also covers www.).
    {"google.com", "/url", "q|url"},
    {"google.pl", "/url", "q|url"},
    {"google.de", "/url", "q|url"},
    {"google.co.uk", "/url", "q|url"},
    {"google.fr", "/url", "q|url"},
    {"google.es", "/url", "q|url"},
    {"google.it", "/url", "q|url"},
    {"google.nl", "/url", "q|url"},
    {"google.cz", "/url", "q|url"},
    // Google Ads click tracking.
    {"adclick.g.doubleclick.net", "/pcs/click", "adurl"},
    // YouTube video descriptions and comments.
    {"www.youtube.com", "/redirect", "q"},
    // Facebook, Messenger, Instagram outbound links.
    {"l.facebook.com", "/l.php", "u"},
    {"lm.facebook.com", "/l.php", "u"},
    {"l.messenger.com", "/l.php", "u"},
    {"l.instagram.com", "/", "u"},
    // Microsoft: Outlook Safe Links (eur01.safelinks..., nam12.safelinks...)
    // and Bing results ("a1" + base64url of the target).
    {"safelinks.protection.outlook.com", "/", "url"},
    {"www.bing.com", "/ck/a", "u", Encoding::kBase64Url, "a1"},
    // LinkedIn, Reddit, Tumblr, VK, Steam, Slack, SoundCloud.
    {"www.linkedin.com", "/safety/go", "url"},
    {"out.reddit.com", "/", "url"},
    {"t.umblr.com", "/redirect", "z"},
    {"vk.com", "/away.php", "to"},
    {"away.vk.com", "/away.php", "to"},
    {"steamcommunity.com", "/linkfilter/", "u|url"},
    {"slack-redir.net", "/link", "url"},
    {"exit.sc", "/", "url"},
    // Affiliate networks: the target shop is in the link.
    {"click.linksynergy.com", "/deeplink", "murl"},
    {"www.awin1.com", "/cread.php", "ued"},
    {"go.skimresources.com", "/", "url"},
};

bool HostMatches(std::string_view host, std::string_view rule_host) {
  if (host == rule_host) {
    return true;
  }
  return host.size() > rule_host.size() &&
         base::EndsWith(host, rule_host) &&
         host[host.size() - rule_host.size() - 1] == '.';
}

bool PathMatches(std::string_view path, std::string_view rule_path) {
  if (rule_path.ends_with('/')) {
    return base::StartsWith(path, rule_path);
  }
  return path == rule_path;
}

// The unescaped value of the first `name` parameter in `url`'s query.
std::optional<std::string> QueryValue(const GURL& url, std::string_view name) {
  std::string_view spec = url.spec();
  url::Component query = url.parsed_for_possibly_invalid_spec().query;
  url::Component key, value;
  while (url::ExtractQueryKeyValue(spec, &query, &key, &value)) {
    if (spec.substr(key.begin, key.len) == name) {
      return base::UnescapeBinaryURLComponent(
          spec.substr(value.begin, value.len));
    }
  }
  return std::nullopt;
}

std::optional<std::string> Decode(const Rule& rule, std::string value) {
  if (rule.encoding == Encoding::kPlain) {
    return value;
  }
  if (!base::StartsWith(value, rule.value_prefix)) {
    return std::nullopt;
  }
  std::string decoded;
  if (!base::Base64UrlDecode(
          std::string_view(value).substr(rule.value_prefix.size()),
          base::Base64UrlDecodePolicy::IGNORE_PADDING, &decoded)) {
    return std::nullopt;
  }
  return decoded;
}

}  // namespace

base::span<const Rule> BuiltInRules() {
  return kRules;
}

std::optional<GURL> TargetFor(const GURL& url, base::span<const Rule> rules) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_query()) {
    return std::nullopt;
  }
  for (const Rule& rule : rules) {
    if (!HostMatches(url.host(), rule.host) ||
        !PathMatches(url.path(), rule.path)) {
      continue;
    }
    for (std::string_view name : base::SplitStringPiece(
             rule.params, "|", base::TRIM_WHITESPACE,
             base::SPLIT_WANT_NONEMPTY)) {
      std::optional<std::string> value = QueryValue(url, name);
      if (!value) {
        continue;
      }
      std::optional<std::string> decoded = Decode(rule, std::move(*value));
      if (!decoded) {
        continue;
      }
      GURL target(*decoded);
      if (target.is_valid() && target.SchemeIsHTTPOrHTTPS() &&
          target != url) {
        return target;
      }
    }
    return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace debounce
}  // namespace plainbrowser::adblock
