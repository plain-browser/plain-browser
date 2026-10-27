// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/renderer/fingerprint_exceptions.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "plainbrowser/fingerprint/common/switches.h"
#include "plainbrowser/shields/renderer/shields_config_impl.h"

namespace plainbrowser::fingerprint {

namespace {

// Built-in compatibility list: sites that need a stable device fingerprint to
// work. Typical example: online banking and payment services, which fingerprint
// the device for fraud detection and strong customer authentication; a
// fingerprint that changes every session triggers extra verification or blocks
// the login. Polish market first. Keep sorted; eTLD+1 only, lower case.
constexpr std::array<std::string_view, 36> kBuiltInExceptions = {
    "aliorbank.pl",      "bankmillennium.pl", "blik.com",
    "bnpparibas.pl",     "bosbank.pl",        "bps.pl",
    "citibank.pl",       "citibankonline.pl", "credit-agricole.pl",
    "envelobank.pl",     "getinbank.pl",      "ing.pl",
    "ingbank.pl",        "inteligo.pl",       "ipko.pl",
    "mbank.pl",          "millenet.pl",       "nestbank.pl",
    "paypal.com",        "payu.pl",           "pekao.com.pl",
    "pekao24.pl",        "pkobp.pl",          "plusbank.pl",
    "pocztowy.pl",       "przelewy24.pl",     "revolut.com",
    "santander.pl",      "santanderconsumer.pl", "sgb.pl",
    "skok.pl",           "tpay.com",          "velobank.pl",
    "wise.com",          "zen.com",           "zenbank.com",
};

const std::vector<std::string>& SwitchExceptions() {
  static const base::NoDestructor<std::vector<std::string>> list(
      ParseExceptionList(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kFingerprintExceptions)));
  return *list;
}

}  // namespace

std::vector<std::string> ParseExceptionList(std::string_view value) {
  std::vector<std::string> out;
  for (std::string_view item :
       base::SplitStringPiece(value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    out.push_back(base::ToLowerASCII(item));
  }
  return out;
}

bool IsExceptedSite(std::string_view site,
                    base::span<const std::string> user_exceptions) {
  if (site.empty()) {
    return false;
  }
  const std::string lower = base::ToLowerASCII(site);
  if (std::find(kBuiltInExceptions.begin(), kBuiltInExceptions.end(), lower) !=
      kBuiltInExceptions.end()) {
    return true;
  }
  return std::find(user_exceptions.begin(), user_exceptions.end(), lower) !=
         user_exceptions.end();
}

bool IsExceptedSite(std::string_view site) {
  // --fingerprint-exceptions is read once per process; the list pushed over
  // mojom::ShieldsConfig follows pref changes while the process runs (the
  // bubble's reload reuses the renderer).
  return IsExceptedSite(site, SwitchExceptions()) ||
         shields::IsExceptedSite(base::ToLowerASCII(site));
}

}  // namespace plainbrowser::fingerprint
