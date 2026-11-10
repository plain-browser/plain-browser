// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/win/msix_default_browser.h"

#include <windows.h>

#include <appmodel.h>
#include <shellapi.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/shlwapi.h"

namespace plainbrowser::win {

namespace {

// The App User Model ID of the app Windows opens `scheme` with, or empty.
// ASSOCSTR_APPID names the app behind the user's choice, packaged or not, so
// it matches the package app's ID exactly when that app is the default.
std::wstring DefaultAppForScheme(const wchar_t* scheme) {
  wchar_t buffer[APPLICATION_USER_MODEL_ID_MAX_LENGTH];
  DWORD size = APPLICATION_USER_MODEL_ID_MAX_LENGTH;
  if (FAILED(::AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_APPID, scheme,
                                 nullptr, buffer, &size))) {
    return std::wstring();
  }
  return std::wstring(buffer);
}

}  // namespace

std::wstring PackageAppUserModelId() {
  wchar_t id[APPLICATION_USER_MODEL_ID_MAX_LENGTH];
  UINT32 length = APPLICATION_USER_MODEL_ID_MAX_LENGTH;
  return ::GetCurrentApplicationUserModelId(&length, id) == ERROR_SUCCESS
             ? std::wstring(id)
             : std::wstring();
}

std::optional<bool> IsPackageDefaultBrowser() {
  const std::wstring app_id = PackageAppUserModelId();
  if (app_id.empty()) {
    return std::nullopt;
  }
  for (const wchar_t* scheme : {L"http", L"https"}) {
    // App User Model IDs compare case-insensitively.
    if (!base::EqualsCaseInsensitiveASCII(DefaultAppForScheme(scheme),
                                          app_id)) {
      return false;
    }
  }
  return true;
}

bool ShowPackageDefaultAppsSettings() {
  const std::wstring app_id = PackageAppUserModelId();
  if (app_id.empty()) {
    return false;
  }
  // A package family name and an app id contain only letters, digits, '.',
  // '_', '-' and the '!' between them: nothing to escape in a query value.
  const std::wstring uri = L"ms-settings:defaultapps?registeredAUMID=" + app_id;
  // ShellExecute returns a value above 32 on success.
  if (reinterpret_cast<intptr_t>(::ShellExecuteW(nullptr, L"open", uri.c_str(),
                                                 nullptr, nullptr,
                                                 SW_SHOWNORMAL)) > 32) {
    return true;
  }
  // Older Windows 10 builds may not know the parameter: open the page itself.
  LOG(WARNING) << "Plain Browser: could not open the Default apps page for "
                  "the package app; opening Default apps";
  return reinterpret_cast<intptr_t>(::ShellExecuteW(
             nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr,
             SW_SHOWNORMAL)) > 32;
}

}  // namespace plainbrowser::win
