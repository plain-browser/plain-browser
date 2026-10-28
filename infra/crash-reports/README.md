# Crash report collector (Cloudflare Worker)

Receives crash reports from Plain Browser at `crash.plain-browser.app/report` and keeps them in R2 for 90 days.
The browser uploads only when the user turned "Send crash reports to Plain Browser" on (Settings > Privacy and
security, off by default); the browser side is `//plainbrowser/crash` and `patches/95-crash`, see
`docs/CRASH_REPORTS.md`.

How it works:
- Crashpad (the browser's crash handler) sends one `POST /report` per crash, at most one an hour per install:
  `multipart/form-data`, usually `Content-Encoding: gzip`, with the minidump in `upload_file_minidump` and the
  process annotations as form fields (`prod`, `ver`, `channel`, `plat`, `ptype` and Chromium's crash keys).
- The Worker accepts a report only if `prod` is our product name, `ver` a version number and the file starts with
  the minidump signature `MDMP`; at most 20 MiB as sent and 32 MiB unpacked.
- Fields that could identify an install or a person are dropped before anything is stored: `guid` (a client ID;
  Chromium sets it only with metrics on, which Plain Browser does not have), `num-switches` and `switch-N`
  (command-line switches can hold a path under `C:\Users\<name>`), and any field whose name contains `url`,
  `path` or `dir`.
- Stored in R2: `reports/<yyyy-mm-dd>/<id>.dmp` (the minidump as sent) and `reports/<yyyy-mm-dd>/<id>.json`
  (`{format, id, received: <day>, size, fields}`). The answer is the report ID, which the browser shows on
  `chrome://crashes`, so a user can quote it in a bug report.

What the Worker sees: the report and, through Cloudflare, the client IP, used only by the rate limiter (5 reports a
minute per IP). It logs nothing (`observability` off) and stores no IP, cookie or time finer than the day. A
minidump can contain parts of the crashed process's memory (for a renderer: the page that was open); that is why
uploading is opt-in and the reports are deleted after 90 days.

## Endpoints

| Path | Response |
|---|---|
| `POST /report` | 200 and the report ID; 400 for a body that is not a report of ours, 413 too large, 415 an encoding other than gzip, 429 over the rate limit |

Any other path is 404, other methods 405.

## Deploy

```bash
cd infra/crash-reports
npx wrangler login
npx wrangler r2 bucket create plainbrowser-crash-reports
npx wrangler r2 bucket lifecycle add plainbrowser-crash-reports delete-after-90-days --expire-days 90
npx wrangler deploy
```

`crash.plain-browser.app` is a custom domain of the `plain-browser.app` zone; `wrangler deploy` creates the record.
Until the Worker is deployed, browsers with crash reports on keep their reports locally and Crashpad retries later.

## Reading reports

Download a report (`npx wrangler r2 object get plainbrowser-crash-reports/reports/<day>/<id>.dmp --file x.dmp`)
and symbolize it with `minidump-stackwalk` (rust-minidump) against the PDBs of the same build; see
`docs/CRASH_REPORTS.md`, "Symbols".

## Tests

`npm test` (Node 22.18+ runs the TypeScript directly; no dependencies).
