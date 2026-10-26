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

namespace plainbrowser::fingerprint {

namespace {

// Built-in exceptions: online banking and payment sites (Polish market first).
// Banks fingerprint the device as part of fraud detection and strong customer
// authentication; a fingerprint that changes every session triggers extra
// verification or blocks the login. Keep sorted; eTLD+1 only, lower case.
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
  static const base::NoDestructor<std::vector<std::string>> list([] {
    std::vector<std::string> out;
    const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
    for (std::string_view item : base::SplitStringPiece(
             cmd.GetSwitchValueASCII(switches::kFingerprintExceptions), ",",
             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      out.push_back(base::ToLowerASCII(item));
    }
    return out;
  }());
  return *list;
}

}  // namespace

bool IsExceptedSite(std::string_view site) {
  if (site.empty()) {
    return false;
  }
  const std::string lower = base::ToLowerASCII(site);
  if (std::find(kBuiltInExceptions.begin(), kBuiltInExceptions.end(), lower) !=
      kBuiltInExceptions.end()) {
    return true;
  }
  const std::vector<std::string>& user = SwitchExceptions();
  return std::find(user.begin(), user.end(), lower) != user.end();
}

}  // namespace plainbrowser::fingerprint
