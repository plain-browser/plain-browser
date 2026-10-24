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

1. Edit `wrangler.toml`: set `routes` to your hostname (for example `sb.mojadomena.pl`; the zone must be on
   Cloudflare) and a unique `namespace_id` for the rate limiter.
2. Create a Google API key with only the **Safe Browsing API** enabled (Google Cloud console → APIs & Services).
   Restrict it to that API. Applying IP restrictions to Cloudflare's ranges is not practical, so rely on the
   rate limiter and quota alerts.
3. Store the key and deploy:

```bash
wrangler secret put GOOGLE_SB_KEY
wrangler deploy
```

Rate limiting: `wrangler.toml` allows 60 requests per 60 s per client IP. A browser polls every 30 minutes or
more. Optional: set `REQUIRED_CLIENT = "PlainBrowser"` in `[vars]` to accept only `X-Client: PlainBrowser/<version>`.
This is a filter, not authentication: anyone can send that header.

## Test

Unit tests (Node 22.18+, no dependencies; the upstream is mocked):

```bash
npm test
```

Live test against your deployment. The request body is a minimal `threatListUpdates:fetch`:

```bash
curl -sS -X POST "https://sb.mojadomena.pl/v4/threatListUpdates:fetch" \
  -H "Content-Type: application/json" \
  -d '{"client":{"clientId":"plainbrowser","clientVersion":"0.1.0"},
       "listUpdateRequests":[{"threatType":"MALWARE","platformType":"WINDOWS",
       "threatEntryType":"URL","state":"","constraints":{"supportedCompressions":["RAW"]}}]}' \
  -i | head -20
```

Expect `HTTP/2 200`, `cache-control: no-store` and JSON with `listUpdateResponses`. A 404 for any other path:
`curl -i https://sb.mojadomena.pl/v4/other`.

## Chromium side

In the Chromium 156 tree the base URL is a constant in
`components/safe_browsing/core/browser/db/sb_protocol_manager_util.cc`:

```cpp
const char kSbV4UrlPrefix[] = "https://safebrowsing.googleapis.com/v4";   // ungoogled-chromium shows it domain-substituted
```

Requests are built in `SBProtocolManagerUtil::GetRequestUrlAndHeaders` as `<prefix>/<method>?$req=…&$ct=application/x-protobuf`
plus `&key=…`. A patch under `patches/` should:

1. Change `kSbV4UrlPrefix` to `"https://sb.mojadomena.pl/v4"` (the `/v4` stays; the Worker routes on it).
2. Check `kSbV5UrlPrefix` and the real-time URL lookup paths; anything that still points at Google bypasses the
   proxy. The Worker intentionally does not serve v5.
3. Send `X-Client: PlainBrowser/<major.minor.patch>` if `REQUIRED_CLIENT` is used.
4. Keep Enhanced Protection and extended reporting disabled and not selectable (they upload URLs and page data).
   Standard protection (local hash list) is the only mode that fits this design.

ungoogled-chromium disables Safe Browsing by default, so re-enabling it also means reverting its patches that
stub out the feature. That work is not done yet, and I have not verified which of those patches apply to 156.
