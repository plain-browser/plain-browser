# Safe Browsing proxy (Cloudflare Worker)

Lets Plain Browser use Google Safe Browsing (v4, Update API with a local hash database) without Google seeing
user IP addresses. The browser talks to this Worker; the Worker talks to `safebrowsing.googleapis.com` with your
API key and with everything that identifies the client removed.

What Google sees: Cloudflare's IP, a fixed `User-Agent`, `Content-Type`, your key, and the request body
(hash prefixes). What it does not see: client IP, cookies, `Referer`, `Accept-Language`, any `CF-*` header.

Limits you should know about:
- The proxy operator (you) can see client IPs in Cloudflare. The Worker neither logs nor stores them
  (`observability` is off, no KV/D1), but Cloudflare processes the connection as with any site.
- The request body is passed through unchanged. Hash prefixes are what Safe Browsing requires; do not enable
  reporting or Enhanced Protection, they send URLs.
- Only v4 is proxied, as designed. Current Chromium also has a v5 prefix (`kSbV5UrlPrefix`); see "Chromium side".

## Endpoints

`GET`/`POST` on `/v4/threatListUpdates:fetch`, `/v4/fullHashes:find`, `/v4/threatMatches:find`. Any other path is
404. The query string is forwarded (Chromium sends `$req` and `$ct`), except `key`, which is always replaced by
the server's key.

## Deploy

```bash
cd infra/safebrowsing-proxy
npm i -g wrangler            # or use npx wrangler
wrangler login
```

1. `wrangler.toml` routes the Worker at `sb.plain-browser.app` (the zone `plain-browser.app` must be on
   Cloudflare) and requires `X-Client: PlainBrowser/<version>`; set a unique `namespace_id` for the rate limiter.
2. Create a Google API key with only the **Safe Browsing API** enabled (Google Cloud console → APIs & Services).
   Restrict it to that API. Applying IP restrictions to Cloudflare's ranges is not practical, so rely on the
   rate limiter and quota alerts.
3. Store the key and deploy:

```bash
wrangler secret put GOOGLE_SB_KEY
wrangler deploy
```

Rate limiting: `wrangler.toml` allows 60 requests per 60 s per client IP. A browser polls every 30 minutes or
more. `REQUIRED_CLIENT = "PlainBrowser"` in `[vars]` accepts only `X-Client: PlainBrowser/<major.minor.patch>`,
which is what the browser sends. This is a filter, not authentication: anyone can send that header.

## Test

Unit tests (Node 22.18+, no dependencies; the upstream is mocked):

```bash
npm test
```

Live test against your deployment. The request body is a minimal `threatListUpdates:fetch`:

```bash
curl -sS -X POST "https://sb.plain-browser.app/v4/threatListUpdates:fetch" \
  -H "Content-Type: application/json" -H "X-Client: PlainBrowser/0.1.0" \
  -d '{"client":{"clientId":"plainbrowser","clientVersion":"0.1.0"},
       "listUpdateRequests":[{"threatType":"MALWARE","platformType":"WINDOWS",
       "threatEntryType":"URL","state":"","constraints":{"supportedCompressions":["RAW"]}}]}' \
  -i | head -20
```

Expect `HTTP/2 200`, `cache-control: no-store` and JSON with `listUpdateResponses`. A 404 for any other path:
`curl -i https://sb.plain-browser.app/v4/other`, and a 403 without the `X-Client` header.

## Chromium side

Done in `patches/70-safebrowsing/0001-proxy-and-client-header.patch`, on
`components/safe_browsing/core/browser/db/sb_protocol_manager_util.cc` (Chromium 156 renamed the old
`v4_protocol_manager_util.cc`; the functions kept their names):

1. `GetRequestUrlAndHeaders` builds v4 URLs on `https://sb.plain-browser.app/v4` and `GetSbV5UrlPrefix()` returns
   `https://sb.plain-browser.app/v5`. The upstream constants with the Google hostname are left in place (ungoogled's
   domain substitution rewrites that line, so a hunk on it would apply in one build flow and fail in the other);
   nothing reads them any more.
2. Every v4 request gets `X-Client: PlainBrowser/<major.minor.patch>` from `PLAINBROWSER_VERSION`
   (`src/plainbrowser/common/version.h`, generated from `versions.toml` by `scripts/sync-version.py`). No install
   id, user id or Chromium build number is sent.
3. The Worker does not serve v5, so v5 lookups get a 404 at the proxy and never reach Google. The real-time URL
   lookup and report URLs still carry domain-substituted hostnames (dead), as in ungoogled-chromium.
4. Enhanced Protection and extended reporting stay disabled and unselectable (they upload URLs and page data).
   Standard protection (local hash list) is the only mode that fits this design.

Still open: ungoogled-chromium builds with `safe_browsing_mode=0` and patches the feature out
(`fix-building-without-safebrowsing.patch`, `safe_browsing-disable-*.patch`). The patch above applies cleanly on
that tree but has no effect until Safe Browsing is compiled back in (`safe_browsing_mode=1` in the flags override
in `scripts/build.ps1`, and reverting those ungoogled patches). That is a separate, unestimated step.
