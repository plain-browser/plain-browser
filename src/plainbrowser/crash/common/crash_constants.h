// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_CRASH_COMMON_CRASH_CONSTANTS_H_
#define PLAINBROWSER_CRASH_COMMON_CRASH_CONSTANTS_H_

// Header only and without includes: chrome_elf's crash reporter client
// (chrome/app/chrome_crash_reporter_client_win.cc, patches/95-crash/0001) reads
// these before anything else of the browser is loaded.

namespace plainbrowser::crash {

// Where Crashpad uploads reports (infra/crash-reports, a Cloudflare Worker that
// keeps no logs).
inline constexpr char kUploadUrl[] = "https://crash.plain-browser.app/report";

// DWORD under HKCU\<install_static::GetRegistryPath()>: 1 when the user turned
// crash reports on. The source of truth is the Local State pref
// (pref_names.h); CrashConsent mirrors it here because chrome_elf decides on
// uploads at start-up, before Local State is read. Missing or 0: no uploads.
inline constexpr wchar_t kConsentRegValue[] = L"PlainBrowserCrashReports";

}  // namespace plainbrowser::crash

#endif  // PLAINBROWSER_CRASH_COMMON_CRASH_CONSTANTS_H_
