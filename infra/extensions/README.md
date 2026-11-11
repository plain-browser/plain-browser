# Extension repository (Cloudflare Worker + R2)

Serves the extensions Plain Browser offers to install, at `ext.plain-browser.app`: uBlock Origin, NoScript and
AdGuard, built from their upstream GitHub releases. The browser installs one from its CRX URL; Chromium's
extension updater then keeps it current through the `update_url` that every served manifest carries, so updates come
from here and not from the Chrome Web Store or the authors' servers.

How it works:
- `pins.json` names the one release of each extension that is built: its tag, the manifest version, the sha256 of
  the upstream zip and the permissions it asks for (API and host permissions, required and `optional:`, and
  `content_script:` match patterns).
- A weekly GitHub Actions run (`.github/workflows/extensions.yml`, job `propose`) runs `scripts/propose.mjs`: for each
  extension in `SOURCES` (`scripts/extensions.mjs`) it takes the latest non-pre-release GitHub release, checks the
  zip as the build would, and moves the pin. If anything changed it puts the new `pins.json` on the branch
  `extensions/update` and opens a pull request that links each release and source diff and calls out new
  permissions (and an asset that changed under the same tag). That job has neither the signing keys nor the
  Cloudflare token. **Nothing reaches users until someone merges the pull request.**
- A push to `main` that changes `pins.json` (or a manual run) starts the job `build`: `scripts/build.mjs` downloads
  the pinned release, refuses a zip whose sha256, manifest version or permissions differ from the pin, checks the
  manifest (name, version in Chromium's format, `manifest_version` 2 or 3, not older than the published version),
  and repacks it with one change: `"update_url": "https://ext.plain-browser.app/v1/updates.xml"`
  in `manifest.json` (moved to the root if the zip has it one directory down; an upstream `key` is dropped, the CRX
  key decides the ID). Zip entries with absolute paths, `..` or backslashes are refused.
- The zip is signed as a CRX3 (`scripts/crx3.mjs`, RSA-2048, SHA-256) with that extension's own key, and checked
  again before it is written. The same release and key give the same bytes.
- `scripts/publish.mjs` uploads `dist/` to the R2 bucket: versioned CRX files first, then `<slug>.crx`,
  `catalog.json`, and `updates.xml` last, so `updates.xml` never names a file that is not there yet.
- An extension whose download or checks fail keeps its published entry (the run says why, and warns on the run
  page); the others go ahead. The build reads the live `catalog.json` first and stops if a key gives a different
  extension ID than the published one.

What the Worker sees: the request path and query (Chromium's updater sends the installed IDs and versions in
`?x=...`; the Worker ignores them and serves one file to everyone) and, through Cloudflare, the client IP. It logs
nothing (`observability` off) and stores nothing about clients; R2 holds only the files. No `X-Client` header is
required, since Chromium's updater cannot send one.

## Endpoints

| Path | Response |
|---|---|
| `GET /v1/updates.xml` | gupdate (protocol 2.0) response, one `<app>` per extension with `codebase`, `version`, `hash_sha256` and `size`; `application/xml`, `Cache-Control: public, max-age=300` |
| `GET /v1/catalog.json` | `{"ublock": {"id", "name", "version", "manifest_version", "sha256", "size", "source_url", "license"}, ...}` |
| `GET /v1/crx/<slug>.crx` | the latest CRX, `application/x-chrome-extension`, `ETag` |
| `GET /v1/crx/<slug>-<version>.crx` | that version's CRX (what `updates.xml` points at) |

`<slug>` is `ublock`, `noscript` or `adguard`. `HEAD` works too; any other path is 404 (also before the
first publish), other methods 405, more than 60 requests a minute from one IP 429.

## Keys and extension IDs

Each extension has its own RSA key; the extension ID is derived from it (first 16 bytes of SHA-256 of the public
key, `a`-`p` for each hex digit). The browser has the IDs compiled in.

**Losing a key, or replacing it, gives the extension a new ID.** Installed copies keep the old ID and never get
another update; users have to remove and reinstall the extension. There is no rotation: keep each key safe for good.

```bash
mkdir -m 700 ~/plainbrowser-extension-keys
node scripts/keygen.mjs ublock ~/plainbrowser-extension-keys/ublock.pem   # prints the extension ID
gh secret set EXT_KEY_UBLOCK < ~/plainbrowser-extension-keys/ublock.pem
```

The same for `noscript` and `adguard`. Keep the PEM files offline (a password manager, say) and never
commit them (`.gitignore` covers `*.pem` here). The build takes a key from `EXT_KEY_<SLUG>` (PEM text) or
`--keys-dir DIR` (`DIR/<slug>.pem`) and refuses to start when one is missing.

## Deploy

```bash
cd infra/extensions
npx wrangler login
npx wrangler r2 bucket create plainbrowser-extensions
npx wrangler deploy
node scripts/build.mjs --keys-dir ~/plainbrowser-extension-keys --previous none   # first build only
node scripts/publish.mjs            # --dry-run prints the wrangler commands
```

`ext.plain-browser.app` is a custom domain of the `plain-browser.app` zone; `wrangler deploy` creates the record.
After that the workflow builds and publishes whenever `pins.json` changes on `main`; it needs the `EXT_KEY_*`
secrets and `CLOUDFLARE_API_TOKEN` (R2 write; set the variable `CLOUDFLARE_ACCOUNT_ID` too if the token reaches more
than one account). Without the token it builds and does not publish. Run it by hand from the Actions tab; tick
"first build" while nothing is published yet, or "propose" to look for new upstream releases now.

For the weekly run to open its pull request, turn on Settings → Actions → General → "Allow GitHub Actions to create
and approve pull requests". With it off the run still pushes `extensions/update` and opens an issue with the link
that opens the pull request. To move a pin by hand: `node scripts/propose.mjs [--only ublock]`, look at the diff of
`pins.json`, commit.

Check:

```bash
curl -s https://ext.plain-browser.app/v1/updates.xml
curl -s https://ext.plain-browser.app/v1/catalog.json
curl -sI https://ext.plain-browser.app/v1/crx/ublock.crx
```

## Test

Unit tests (Node 22.18+, no dependencies; R2 is mocked, no network):

```bash
npm test
```

## Licenses

All four extensions are GPLv3 and are redistributed under it, with one change to each: the `update_url` line in
`manifest.json` (and the upstream `key`, if any, removed). Everything else is the upstream release zip, file for
file. The source is upstream, at the release each build names in `catalog.json` (`source_url`):

- uBlock Origin: https://github.com/gorhill/uBlock
- NoScript: https://github.com/hackademix/noscript
- AdGuard: https://github.com/AdguardTeam/AdguardBrowserExtension

The scripts and the Worker here are MPL-2.0 like the rest of Plain Browser.
