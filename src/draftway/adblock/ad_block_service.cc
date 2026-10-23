// Copyright Draftway. MPL-2.0.
#include "draftway/adblock/ad_block_service.h"

#include "base/task/thread_pool.h"
#include "draftway/adblock/rs/src/lib.rs.h"

namespace draftway::adblock {

AdBlockService::AdBlockService()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      engine_(new_engine("")) {}

AdBlockService::~AdBlockService() = default;

void AdBlockService::LoadRules(const std::string& rules_text) {
  engine_ = new_engine(rust::Str(rules_text));
}

bool AdBlockService::LoadSerialized(const std::vector<uint8_t>& data) {
  try {
    engine_ = deserialize_engine(rust::Slice<const uint8_t>(data.data(), data.size()));
    return true;
  } catch (const rust::Error&) {
    return false;
  }
}

bool AdBlockService::ShouldBlock(const std::string& url,
                                 const std::string& source_url,
                                 const std::string& request_type,
                                 std::string* redirect_out) const {
  BlockResult r = engine_->check_network_request(url, source_url, request_type);
  if (!r.redirect.empty() && redirect_out)
    *redirect_out = std::string(r.redirect);
  return r.important || (r.matched && !r.exception);
}

std::string AdBlockService::CosmeticCss(const std::string& url) const {
  return std::string(engine_->cosmetic_css(url));
}

}  // namespace draftway::adblock
