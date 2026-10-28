// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_SERVICE_H_
#define PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "plainbrowser/adblock/common/adblock_engine.mojom.h"
#include "plainbrowser/adblock/engine/ad_block_engine.h"

class GURL;

namespace plainbrowser::adblock {

// mojom::AdblockEngine in the adblock utility process: registered in
// chrome/utility/services.cc (patches/30-adblock/0008), launched by
// AdBlockService with content::ServiceProcessHost. The receiver lives on the
// process's main thread; every query runs on the engine's sequence and its
// answer comes back here. One instance per process; the browser closes the
// pipe to end it.
class AdBlockEngineService : public mojom::AdblockEngine {
 public:
  explicit AdBlockEngineService(
      mojo::PendingReceiver<mojom::AdblockEngine> receiver);
  ~AdBlockEngineService() override;

  AdBlockEngineService(const AdBlockEngineService&) = delete;
  AdBlockEngineService& operator=(const AdBlockEngineService&) = delete;

  // mojom::AdblockEngine:
  void UseLists(std::vector<mojom::NamedListPtr> lists,
                UseListsCallback callback) override;
  void ShouldBlock(const GURL& url,
                   const GURL& source_url,
                   const std::string& request_type,
                   ShouldBlockCallback callback) override;
  void CosmeticResources(const GURL& url,
                         CosmeticResourcesCallback callback) override;
  void Scriptlets(const GURL& url, ScriptletsCallback callback) override;
  void RedirectUrl(const GURL& url,
                   const GURL& source_url,
                   RedirectUrlCallback callback) override;
  void RedirectHosts(RedirectHostsCallback callback) override;
  void GenericCosmeticCss(const GURL& url,
                          const std::vector<std::string>& classes,
                          const std::vector<std::string>& ids,
                          GenericCosmeticCssCallback callback) override;
  void RewrittenUrl(const GURL& url,
                    const GURL& source_url,
                    const std::string& request_type,
                    RewrittenUrlCallback callback) override;

 private:
  // Leaked on purpose: queries and list builds still in flight hold it
  // unretained, and the process exits right after the service is destroyed
  // (the browser closed the pipe).
  const raw_ptr<AdBlockEngine> engine_;
  mojo::Receiver<mojom::AdblockEngine> receiver_;
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_SERVICE_H_
