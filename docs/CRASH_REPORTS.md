# Crash reports

Status 2026-10-28: built on the build VM. `plainbrowser_unittests` 80/80 (3 `CrashConsent` tests); a fresh profile
starts the Crashpad handler with `--url=https://crash.plain-browser.app/report` and writes
`HKCU\Software\PlainBrowser\PlainBrowserCrashReports = 0` (off by default). Worker unit tests pass
(`infra/crash-reports`, `npm test`). **Not done yet**: an upload end to end (toggle on, `chrome://crash`, a local
Worker), deploying the Worker, symbols.

## What the user sees
Settings > Privacy and security: "Send crash reports to Plain Browser", **off by default**, with what is sent and
where. On: after a crash the browser uploads the report to `crash.plain-browser.app` (at most one an hour) and
`chrome://crashes` lists it with its report ID. Off: reports are written and listed on `chrome://crashes` but stay
on the device.

## How it works
- **Crashpad back on.** ungoogled's `disable-crash-reporter.patch` turns Crashpad off on Windows; it is dropped
  in `patches/upstream-revert.list`. The crash handler runs as in Chromium (one `--type=crashpad-handler`
  process) and writes dumps under the user data directory (`Crashpad\`).
- **Consent.** Chromium uploads when "metrics and crash reporting" consent is on: the `usagestats` registry value,
  set together with UMA. Plain Browser has no metrics, so `patches/95-crash/0001` gives Crashpad its own consent:
  - Local State pref `plainbrowser.crash_reports.enabled` (default false), the settings toggle
    (`patches/60-ui/0012`);
  - `CrashConsent` (`//plainbrowser/crash/browser`) mirrors it at start and on every change into
    `HKCU\<install_static::GetRegistryPath()>\PlainBrowserCrashReports` (DWORD) and calls
    `SetUploadConsent_ExportThunk` for the running Crashpad;
  - `ChromeCrashReporterClient::GetCollectStatsConsent()` (chrome_elf, before Local State exists) reads that
    registry value; the `MetricsReportingEnabled` policy still wins;
  - `ChromeMetricsServicesManagerClient::UpdateRunningServices` no longer passes the metrics state to Crashpad
    (it would switch uploads off, metrics being always off).
- **URL.** `ChromeCrashReporterClient::GetUploadUrl()` returns `plainbrowser::crash::kUploadUrl`
  (`https://crash.plain-browser.app/report`).
- **Collector.** `infra/crash-reports`: a Cloudflare Worker that checks the report, drops identifying fields
  (`guid`, command-line switches, anything named like a URL, path or directory) and stores the minidump and the
  remaining fields in R2 for 90 days. No logs, no IP stored. Details in its README.

In the MSIX package the registry value and the dumps live in the package's private registry and storage, like the
profile; uninstalling removes them.

## Symbols
A minidump holds module names and offsets; turning them into function names needs the PDBs of the same build.
Not done yet:
- check the `symbol_level` of the release profile (`scripts/build.ps1`, `scripts/ci/prepare-tree.ps1`;
  `is_official_build=false` today) and keep `chrome.dll.pdb`, `chrome.exe.pdb`, `chrome_elf.dll.pdb` of every
  released build outside the package (they are large; a private R2 bucket or the release's CI artifact);
- symbolize with `minidump-stackwalk` from rust-minidump, which reads PDBs directly.

## Testing (after the next build)
- `plainbrowser_unittests --gtest_filter=CrashConsent*`: pref to registry and to the consent callback.
- `chrome://crash` with the toggle off: a report appears on `chrome://crashes` and nothing is uploaded; on, with the
  Worker running locally (`npx wrangler dev`) and the upload URL overridden with the environment variable
  `CHROME_CRASHPAD_SERVER_URL=http://127.0.0.1:8787/report` (Crashpad's test override,
  `components/crash/core/app/crashpad_win.cc`): the report reaches the Worker and its ID shows up on
  `chrome://crashes`.
