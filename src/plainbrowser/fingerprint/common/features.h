// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_COMMON_FEATURES_H_
#define PLAINBROWSER_FINGERPRINT_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace plainbrowser::fingerprint {

// Kill switch: --disable-features=PlainBrowserFingerprintProtection.
BASE_DECLARE_FEATURE(kPlainBrowserFingerprintProtection);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_COMMON_FEATURES_H_
