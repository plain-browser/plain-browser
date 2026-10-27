// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_BROWSER_BLOCKED_REQUESTS_COUNTER_H_
#define PLAINBROWSER_SHIELDS_BROWSER_BLOCKED_REQUESTS_COUNTER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace plainbrowser::shields {

// Number of requests the adblock filter blocked on the page currently shown in
// a tab. Reset when the primary page changes. Shown in the toolbar bubble.
class BlockedRequestsCounter
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BlockedRequestsCounter> {
 public:
  ~BlockedRequestsCounter() override;

  // Any thread. Finds the tab that owns the frame `frame_token` in renderer
  // process `process_id` and counts one blocked request for it. Called by the
  // adblock filter on the engine sequence; the work happens on the UI thread.
  static void CountBlocked(int process_id,
                           const blink::LocalFrameToken& frame_token);

  // 0 when nothing was blocked or no counter exists yet.
  static int CountFor(content::WebContents* web_contents);

  int count() const { return count_; }
  void Increment() { count_++; }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  friend class content::WebContentsUserData<BlockedRequestsCounter>;
  explicit BlockedRequestsCounter(content::WebContents* web_contents);

  int count_ = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_BROWSER_BLOCKED_REQUESTS_COUNTER_H_
