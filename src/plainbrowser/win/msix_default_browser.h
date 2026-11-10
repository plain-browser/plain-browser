// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_WIN_MSIX_DEFAULT_BROWSER_H_
#define PLAINBROWSER_WIN_MSIX_DEFAULT_BROWSER_H_

#include <optional>
#include <string>

// The default browser when Plain Browser runs from its MSIX package (Microsoft
// Store). Windows registers a packaged app's protocol handlers itself, under
// generated "AppX..." ProgIDs, and keeps the app's own registry writes in the
// package's private hive. So Chromium's checks, which look for the installer's
// ProgID (PlainBrowserHTML), always answer "not default", and its "Make
// default" button opens the Default apps page for an application name Windows
// does not know. These functions do it the packaged way: by the package app's
// App User Model ID (<PackageFamilyName>!PlainBrowser). Outside a package they
// do nothing and Chromium's own code runs (the portable build).
namespace plainbrowser::win {

// The App User Model ID of the package app the process belongs to, or empty
// when the process does not run from a package.
std::wstring PackageAppUserModelId();

// Whether the package app is the user's default for both http and https.
// nullopt outside a package, so the caller falls back to Chromium's check.
// Blocking (shell association queries); call where Chromium probes the
// default browser.
std::optional<bool> IsPackageDefaultBrowser();

// Opens Settings > Apps > Default apps on the package app's page
// (ms-settings:defaultapps?registeredAUMID=...). Returns false outside a
// package, so the caller falls back to Chromium's way.
bool ShowPackageDefaultAppsSettings();

}  // namespace plainbrowser::win

#endif  // PLAINBROWSER_WIN_MSIX_DEFAULT_BROWSER_H_
