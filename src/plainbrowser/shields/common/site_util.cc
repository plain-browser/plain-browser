// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/common/site_util.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace plainbrowser::shields {

std::string SiteFor(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || !url.has_host()) {
    return std::string();
  }
  std::string site = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // GURL hosts are canonical (lower case), so no case folding is needed.
  return site.empty() ? std::string(url.host()) : site;
}

}  // namespace plainbrowser::shields
