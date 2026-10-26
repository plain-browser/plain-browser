# plain-browser.app

The project website, a static Next.js export served from a Google Cloud Storage bucket behind Cloudflare.

- English at `/`, Polish at `/pl/`. Dictionaries in `lib/i18n.ts`.
- The privacy policy and security policy are rendered at build time from `../docs/PRIVACY*.md` and `../SECURITY.md`,
  and the version shown on the Download page comes from `../versions.toml`. Edit those files, not the site.
- No analytics, no cookies, no third-party fonts or scripts, so the site keeps the promise the browser makes.

## Develop

```bash
npm install
npm run dev
```

`npm run build` writes the static site to `out/`.

## One-time setup

1. **Domain on Cloudflare.** The zone `plain-browser.app` must use Cloudflare DNS (the Safe Browsing proxy
   in `infra/safebrowsing-proxy` needs that too).
2. **Verify the domain with Google**, because a bucket named after a domain can only be created by a verified
   owner: Google Search Console → add property `plain-browser.app` → DNS TXT record (add it in Cloudflare).
3. **Create the bucket** with the exact domain name and make it a public website:
   ```bash
   gcloud storage buckets create gs://plain-browser.app --location=EU --uniform-bucket-level-access
   gcloud storage buckets add-iam-policy-binding gs://plain-browser.app --member=allUsers --role=roles/objectViewer
   gcloud storage buckets update gs://plain-browser.app --web-main-page-suffix=index.html --web-error-page=404.html
   ```
4. **DNS.** In Cloudflare add a proxied (orange cloud) record:
   `CNAME plain-browser.app → c.storage.googleapis.com`. For `www` add another proxied CNAME with the same target
   and a redirect rule (`www.plain-browser.app/*` → `https://plain-browser.app/$1`, 301), or skip `www` entirely.
5. **TLS.** GCS serves custom domains over plain HTTP only, so set SSL/TLS mode to **Flexible** for this zone
   (Full would try HTTPS to the bucket and fail) and turn on **Always Use HTTPS**. The browser-to-Cloudflare
   leg is HTTPS; the Cloudflare-to-GCS leg stays inside Google's and Cloudflare's networks over HTTP.
   If that is not acceptable later, put a Google HTTPS load balancer in front of the bucket and switch to Full (strict).
6. **Caching.** Cloudflare caches HTML only with a rule. Add a Cache Rule: hostname equals `plain-browser.app` →
   Eligible for cache, Edge TTL "respect origin". The deploy script sets `max-age=300` on HTML and one year on
   hashed `_next/static` files. After a deploy, purge everything in Cloudflare if you want the new HTML up at once.
7. **Headers.** Add a Response Header Transform rule so the static host still sends sane security headers:
   `X-Content-Type-Options: nosniff`, `Referrer-Policy: strict-origin-when-cross-origin`,
   `Content-Security-Policy: default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; frame-ancestors 'none'`.
   (`'unsafe-inline'` is needed for the Next.js hydration bootstrap and inline styles.)
8. **Cloudflare settings worth switching on:** HTTP/3, Brotli, Early Hints. Leave Rocket Loader, Email
   Obfuscation, Web Analytics and the Insights beacon **off**; the site promises no third-party scripts.

## Deploy

Locally, with `gcloud auth login` done once:

```bash
npm run deploy
```

From GitHub Actions: `.github/workflows/website.yml` runs on every push that touches `website/`, `docs/`,
`SECURITY.md` or `versions.toml`, using Workload Identity Federation. It needs two repository variables:
`GCP_WORKLOAD_IDENTITY_PROVIDER` and `GCP_SERVICE_ACCOUNT` (a service account with
`roles/storage.objectAdmin` on the bucket). Optional `CLOUDFLARE_ZONE_ID` + secret `CLOUDFLARE_API_TOKEN`
(Zone → Cache Purge) purge the cache after upload.
