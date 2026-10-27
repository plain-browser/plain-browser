// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/blocked_requests_counter.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace plainbrowser::shields {

namespace {

void CountBlockedOnUI(int process_id, const blink::LocalFrameToken& token) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromFrameToken(
      content::GlobalRenderFrameHostToken(process_id, token));
  if (!rfh) {
    return;  // Frame already gone.
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  BlockedRequestsCounter::CreateForWebContents(web_contents);
  BlockedRequestsCounter::FromWebContents(web_contents)->Increment();
}

}  // namespace

BlockedRequestsCounter::BlockedRequestsCounter(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BlockedRequestsCounter>(*web_contents) {}

BlockedRequestsCounter::~BlockedRequestsCounter() = default;

// static
void BlockedRequestsCounter::CountBlocked(
    int process_id,
    const blink::LocalFrameToken& frame_token) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CountBlockedOnUI, process_id, frame_token));
}

void BlockedRequestsCounter::Increment() {
  count_++;
  callbacks_.Notify(count_);
}

base::CallbackListSubscription BlockedRequestsCounter::AddCountChangedCallback(
    base::RepeatingCallback<void(int)> callback) {
  return callbacks_.Add(std::move(callback));
}

// static
int BlockedRequestsCounter::CountFor(content::WebContents* web_contents) {
  if (!web_contents) {
    return 0;
  }
  const BlockedRequestsCounter* counter = FromWebContents(web_contents);
  return counter ? counter->count_ : 0;
}

void BlockedRequestsCounter::PrimaryPageChanged(content::Page& page) {
  count_ = 0;
  callbacks_.Notify(count_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BlockedRequestsCounter);

}  // namespace plainbrowser::shields
