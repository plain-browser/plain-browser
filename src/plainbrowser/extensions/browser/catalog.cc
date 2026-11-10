// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/extensions/browser/catalog.h"

#include <string>

#include "base/command_line.h"
#include "chrome/grit/generated_resources.h"
#include "url/gurl.h"

namespace plainbrowser::extensions {

namespace {

// Test hook: another server for the CRX files (scripts/test-extensions.ps1).
constexpr char kServerSwitch[] = "plainbrowser-extensions-url";
constexpr char kDefaultServer[] = "https://ext.plain-browser.app/v1/";

}  // namespace

const std::array<CuratedExtension, 4>& Catalog() {
  // IDs: infra/extensions/scripts/keygen.mjs, one RSA key per extension
  // (generated 2026-11-09). A new key means a new ID, and users would have to
  // install again: never rotate them casually.
  static const std::array<CuratedExtension, 4> kCatalog = {{
      {"ublock", "noejjobmnfhiaelnmphhjiidpaokblob",
       "cjpalhdlnbpafiamejdnhcphjbkeiagm", "uBlock Origin",
       IDS_PLAINBROWSER_EXTENSIONS_UBLOCK_DESC,
       "https://github.com/gorhill/uBlock", 2, /*ad_blocker=*/true,
       /*unmaintained=*/false},
      {"adguard", "alcnakdfglecaojndbbbianibfbhhohb",
       "bgnkhhnnamicmpeenaelnjfhikgbkllg", "AdGuard AdBlocker",
       IDS_PLAINBROWSER_EXTENSIONS_ADGUARD_DESC,
       "https://github.com/AdguardTeam/AdguardBrowserExtension", 2,
       /*ad_blocker=*/true, /*unmaintained=*/false},
      {"noscript", "ohmnffldfkgpeipdpknpchnalghalfdc",
       "doojmbjmlfjjnbmnoijecmcbfeoakpjm", "NoScript",
       IDS_PLAINBROWSER_EXTENSIONS_NOSCRIPT_DESC,
       "https://github.com/hackademix/noscript", 3, /*ad_blocker=*/false,
       /*unmaintained=*/false},
      {"umatrix", "knaimpmmofdakmoedpbpaneafhcgnfei",
       "ogfcmafjalglgifnmanfmnieipoejdcf", "uMatrix",
       IDS_PLAINBROWSER_EXTENSIONS_UMATRIX_DESC,
       "https://github.com/gorhill/uMatrix", 2, /*ad_blocker=*/false,
       /*unmaintained=*/true},
  }};
  return kCatalog;
}

const CuratedExtension* FindBySlug(std::string_view slug) {
  for (const CuratedExtension& entry : Catalog()) {
    if (entry.slug == slug) {
      return &entry;
    }
  }
  return nullptr;
}

GURL CrxUrl(const CuratedExtension& entry) {
  GURL server(kDefaultServer);
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kServerSwitch)) {
    GURL test_server(cl->GetSwitchValueASCII(kServerSwitch));
    if (test_server.is_valid() && test_server.SchemeIsHTTPOrHTTPS()) {
      server = test_server;
    }
  }
  return server.Resolve("crx/" + std::string(entry.slug) + ".crx");
}

}  // namespace plainbrowser::extensions
