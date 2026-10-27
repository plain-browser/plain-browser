# Filter list mirror (Cloudflare Worker)

Serves current EasyList, EasyPrivacy and EasyList Polish to Plain Browser at `lists.plain-browser.app`, so the
built-in ad blocker gets list updates between browser releases without the browser contacting the list
publishers. The browser side is `//plainbrowser/adblock` (`FilterListUpdater`, `filter_lists`); see
`docs/ADBLOCK.md`, "Filter list updates".

How it works:
- A cron trigger (every 6 hours, `wrangler.toml`) fetches each list from its publisher (`SOURCES` in
  `src/index.ts`, the same URLs as `src/plainbrowser/adblock/lists/NOTICE.md`).
- A list is published only if it looks like a real one: 10 KiB to 16 MiB, an `[Adblock Plus ...]` header, at least
  1000 rules, and not less than half the rules of the version already published. Otherwise the published version
  stays (the cron report says why).
- Published lists go to KV (`list:<name>`), then the manifest with each list's SHA-256, size in bytes, rule count,
  publication time and source, signed with Ed25519 (secret `SIGNING_KEY`); manifest text and signature are one KV
  value (`signed-manifest`), so they always match. The list is written before the manifest that names it. Without
  `SIGNING_KEY` the cron publishes nothing.
- The browser reads the manifest and its signature once a day, checks the signature with the public key compiled
  into it (`kListSigningKeys` in `src/plainbrowser/adblock/browser/filter_lists.h`), downloads only lists whose hash
  differs from its stored copy, and checks size and SHA-256 before using one. So even a mirror that served
  something else (a compromised Cloudflare account aside: the key lives there) cannot push lists the browser
  takes. A mismatch (KV is eventually consistent, so a reader can briefly get
  the old file under a new hash) is skipped until the next check.

What the Worker sees: the request path, `X-Client: PlainBrowser/<major.minor.patch>` and, through Cloudflare, the
client IP. It logs nothing (`observability` off) and stores nothing about clients; KV holds only the lists. The
browser sends no cookies.

## Endpoints

| Path | Response |
|---|---|
| `GET /v1/manifest.json.sig` | base64 Ed25519 signature of the exact bytes of `manifest.json` |
| `GET /v1/manifest.json` | `{"format": 1, "generated": "...", "license": "...", "lists": {"easylist": {"sha256", "size", "rules", "updated", "source"}, ...}}`, `Cache-Control: public, max-age=300`; 503 before the first cron run |
| `GET /v1/lists/<name>.txt` | the list, `ETag: "<sha256>"`; `<name>` is `easylist`, `easyprivacy` or `easylistpolish` |

`HEAD` works too; any other path is 404, other methods 405, a missing or foreign `X-Client` 403 (a filter against
scanners, not authentication), more than 30 requests a minute from one IP 429.

## Deploy

```bash
cd infra/filter-lists
npx wrangler login
npx wrangler kv namespace create LISTS     # put the id in wrangler.toml, [[kv_namespaces]]
node scripts/keygen.mjs ~/plainbrowser-lists-signing.key
npx wrangler secret put SIGNING_KEY < ~/plainbrowser-lists-signing.key
npx wrangler deploy
```

`keygen.mjs` prints the public key: put it into `kListSigningKeys` in
`src/plainbrowser/adblock/browser/filter_lists.h` (first slot) before building the release that should accept
updates; until then the browser refuses every manifest and keeps its bundled lists. Keep the private key file
offline (it is only needed to set the secret again) and never commit it. Rotating: put the new public key into the
second slot, release, then `wrangler secret put SIGNING_KEY` with the new private key; the next cron run re-signs
the manifest.

`lists.plain-browser.app` is a custom domain of the `plain-browser.app` zone; `wrangler deploy` creates the record.
The manifest exists after the first cron run. To publish right away, trigger the cron once from the dashboard
(Workers → plainbrowser-filter-lists → Settings → Triggers) or locally with `npx wrangler dev --test-scheduled`
and `curl "http://localhost:8787/__scheduled?cron=17+*/6+*+*+*"` (that writes to the KV preview namespace only).

Check:

```bash
curl -s -H "X-Client: PlainBrowser/0.1.2" https://lists.plain-browser.app/v1/manifest.json
curl -s -H "X-Client: PlainBrowser/0.1.2" https://lists.plain-browser.app/v1/manifest.json.sig
curl -sI -H "X-Client: PlainBrowser/0.1.2" https://lists.plain-browser.app/v1/lists/easylist.txt
```

## Test

Unit tests (Node 22.18+, no dependencies; publishers and KV are mocked):

```bash
npm test
```

## License of the lists

The lists are redistributed unmodified under CC BY-SA 3.0 (also available under GPLv3), as in the browser itself;
the manifest's `license` field and `source` per list name the terms and the origin.
