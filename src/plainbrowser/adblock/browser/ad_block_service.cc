// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/ad_block_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

namespace {

AdBlockService* g_instance_for_testing = nullptr;

std::vector<mojom::NamedListPtr> ToMojo(const AdBlockService::ListTexts& lists) {
  std::vector<mojom::NamedListPtr> out;
  out.reserve(lists.size());
  for (const auto& [name, text] : lists) {
    out.push_back(mojom::NamedList::New(name, text));
  }
  return out;
}

mojom::CosmeticResultPtr NoCosmetics() {
  return mojom::CosmeticResult::New(std::string(), /*generic_hide=*/true,
                                    std::vector<std::string>(),
                                    std::vector<std::string>());
}

}  // namespace

// static
AdBlockService* AdBlockService::GetInstance() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
  static base::NoDestructor<AdBlockService> instance;
  return instance.get();
}

// static
std::unique_ptr<AdBlockService> AdBlockService::CreateForTesting(
    Launcher launcher) {
  std::unique_ptr<AdBlockService> service(new AdBlockService());
  service->SetLauncher(std::move(launcher));
  g_instance_for_testing = service.get();
  return service;
}

AdBlockService::AdBlockService()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AdBlockService::~AdBlockService() {
  if (g_instance_for_testing == this) {
    g_instance_for_testing = nullptr;
  }
}

void AdBlockService::SetLauncher(Launcher launcher) {
  // Unretained: the instance is never destroyed (tests run the task
  // environment until idle before destroying theirs).
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AdBlockService::SetLauncherOnSequence,
                                        base::Unretained(this),
                                        std::move(launcher)));
}

void AdBlockService::SetLauncherOnSequence(Launcher launcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!launcher_) {
    launcher_ = std::move(launcher);
  }
}

AdBlockService::Status::Status() = default;
AdBlockService::Status::Status(const Status&) = default;
AdBlockService::Status& AdBlockService::Status::operator=(const Status&) =
    default;
AdBlockService::Status::~Status() = default;

void AdBlockService::GetStatus(base::OnceCallback<void(Status)> callback) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AdBlockService::GetStatusOnSequence,
                                        base::Unretained(this),
                                        std::move(callback)));
}

void AdBlockService::GetStatusOnSequence(
    base::OnceCallback<void(Status)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status;
  status.engine_running = engine_.is_bound();
  status.engine_starts = engine_starts_;
  status.gave_up = !engine_.is_bound() && engine_starts_ >= kMaxEngineStarts;
  if (lists_) {
    for (const auto& [name, text] : *lists_) {
      status.downloaded_lists.push_back(name);
    }
  }
  std::move(callback).Run(std::move(status));
}

void AdBlockService::UseLists(ListTexts lists, base::OnceClosure done) {
  if (!done) {
    done = base::DoNothing();
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AdBlockService::UseListsOnSequence,
                     base::Unretained(this), std::move(lists),
                     std::move(done)));
}

void AdBlockService::UseListsOnSequence(ListTexts lists,
                                        base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lists.empty()) {
    lists_.reset();
  } else {
    lists_ = std::move(lists);
  }
  if (engine_.is_bound()) {
    engine_->UseLists(
        lists_ ? ToMojo(*lists_) : std::vector<mojom::NamedListPtr>(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  } else if (CanStartEngine()) {
    StartEngine(std::move(done));
  } else {
    std::move(done).Run();
  }
}

bool AdBlockService::CanStartEngine() const {
  return launcher_ && engine_starts_ < kMaxEngineStarts;
}

void AdBlockService::StartEngine(base::OnceClosure lists_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++engine_starts_;
  launcher_.Run(engine_.BindNewPipeAndPassReceiver());
  // Unretained: `engine_` is owned by this and never outlives it.
  engine_.set_disconnect_handler(
      base::BindOnce(&AdBlockService::OnEngineGone, base::Unretained(this)));
  // The process builds its engine from the compiled-in lists by itself; only
  // downloaded ones need sending.
  if (lists_) {
    engine_->UseLists(
        ToMojo(*lists_),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(lists_done)));
  } else {
    std::move(lists_done).Run();
  }
}

mojom::AdblockEngine* AdBlockService::Engine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_.is_bound()) {
    if (!CanStartEngine()) {
      return nullptr;
    }
    StartEngine(base::DoNothing());
  }
  return engine_.get();
}

void AdBlockService::OnEngineGone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pending queries are answered with their fail-open defaults here.
  engine_.reset();
  LOG(WARNING) << "Plain Browser adblock engine process went away (start "
               << engine_starts_ << " of " << kMaxEngineStarts << ")"
               << (lists_ ? "; dropping the downloaded filter lists" : "");
  lists_.reset();
}

void AdBlockService::ShouldBlock(const GURL& url,
                                 const GURL& source_url,
                                 const std::string& request_type,
                                 base::OnceCallback<void(bool)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(false);
    return;
  }
  engine->ShouldBlock(
      url, source_url, request_type,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void AdBlockService::CosmeticResources(
    const GURL& url,
    base::OnceCallback<void(mojom::CosmeticResultPtr)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(NoCosmetics());
    return;
  }
  engine->CosmeticResources(url, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                     std::move(callback), NoCosmetics()));
}

void AdBlockService::Scriptlets(
    const GURL& url,
    base::OnceCallback<void(const std::string&)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(std::string());
    return;
  }
  engine->Scriptlets(url, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                              std::move(callback), std::string()));
}

void AdBlockService::RedirectUrl(
    const GURL& url,
    const GURL& source_url,
    base::OnceCallback<void(const std::string&)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(std::string());
    return;
  }
  engine->RedirectUrl(url, source_url,
                      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                          std::move(callback), std::string()));
}

void AdBlockService::RedirectHosts(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run({});
    return;
  }
  engine->RedirectHosts(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<std::string>()));
}

void AdBlockService::GenericCosmeticCss(
    const GURL& url,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::string&)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(std::string());
    return;
  }
  engine->GenericCosmeticCss(url, classes, ids,
                             mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                                 std::move(callback), std::string()));
}

void AdBlockService::RewrittenUrl(
    const GURL& url,
    const GURL& source_url,
    const std::string& request_type,
    base::OnceCallback<void(const GURL&)> callback) {
  mojom::AdblockEngine* engine = Engine();
  if (!engine) {
    std::move(callback).Run(GURL());
    return;
  }
  engine->RewrittenUrl(
      url, source_url, request_type,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), GURL()));
}

}  // namespace plainbrowser::adblock
