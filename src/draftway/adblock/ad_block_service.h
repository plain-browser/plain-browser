// Copyright Draftway. MPL-2.0.
#ifndef DRAFTWAY_ADBLOCK_AD_BLOCK_SERVICE_H_
#define DRAFTWAY_ADBLOCK_AD_BLOCK_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "rust/cxx.h"

namespace draftway::adblock {
struct BlockResult;
class AdblockEngine;  // Rust type, see rs/src/lib.rs

// Owns the adblock-rust engine. All engine access happens on |task_runner_|.
class AdBlockService {
 public:
  AdBlockService();
  ~AdBlockService();
  AdBlockService(const AdBlockService&) = delete;
  AdBlockService& operator=(const AdBlockService&) = delete;

  // Replaces the engine from filter list text (EasyList syntax).
  void LoadRules(const std::string& rules_text);
  // Replaces the engine from a pre-serialized DAT blob shipped as a component.
  bool LoadSerialized(const std::vector<uint8_t>& data);

  // Must be called on the engine sequence.
  bool ShouldBlock(const std::string& url,
                   const std::string& source_url,
                   const std::string& request_type,
                   std::string* redirect_out) const;
  std::string CosmeticCss(const std::string& url) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  rust::Box<AdblockEngine> engine_;
};

}  // namespace draftway::adblock
#endif  // DRAFTWAY_ADBLOCK_AD_BLOCK_SERVICE_H_
