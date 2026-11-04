// GET /api/latest-release → newest published GitHub release (prereleases included), reduced to the
// fields the site needs, cached at the edge for CACHE_SECONDS.
// GET /api/store-version → { version } the Microsoft Store serves now, from the public product catalog,
// cached for STORE_CACHE_SECONDS. No logs, no client data forwarded.

type Env = { REPO: string; CACHE_SECONDS: string; STORE_ID: string; STORE_CACHE_SECONDS: string; GITHUB_TOKEN?: string };
type GhAsset = { name: string; browser_download_url: string };
type GhRelease = { tag_name: string; html_url: string; draft: boolean; prerelease: boolean; published_at: string | null; assets: GhAsset[] };

const JSON_HEADERS = { "content-type": "application/json; charset=utf-8", "x-content-type-options": "nosniff" };

function pick(r: GhRelease) {
  const find = (re: RegExp) => r.assets.find((a) => re.test(a.name))?.browser_download_url ?? null;
  return {
    version: r.tag_name.replace(/^v/, ""),
    tag: r.tag_name,
    url: r.html_url,
    publishedAt: r.published_at,
    prerelease: r.prerelease,
    installer: find(/_installer\.exe$/i),
    zip: find(/x64\.zip$/i),
    msix: find(/\.msix$/i),
    sums: find(/^SHA256SUMS/i),
  };
}

// Same parser as website/lib/store.ts: package names look like "PlainBrowser.PlainBrowser_0.1.5.0_x64__7m7psx7yqqmx8";
// the highest x64 one wins, and "0.1.5.0" is reported as "0.1.5".
function parseStoreVersion(text: string): string | null {
  const found = [...text.matchAll(/_(\d+)\.(\d+)\.(\d+)\.(\d+)_x64__/g)].map((m) => m.slice(1, 5).map(Number));
  if (!found.length) return null;
  found.sort((a, b) => a.reduce((d, _, i) => d || b[i] - a[i], 0));
  const [major, minor, patch, rev] = found[0];
  return rev ? `${major}.${minor}.${patch}.${rev}` : `${major}.${minor}.${patch}`;
}

async function storeVersion(env: Env): Promise<Response> {
  const id = encodeURIComponent(env.STORE_ID);
  const cat = await fetch(`https://displaycatalog.mp.microsoft.com/v7.0/products?bigIds=${id}&market=US&languages=en-us,neutral`, {
    headers: { accept: "application/json", "user-agent": "plain-browser.app site-api" },
  });
  if (!cat.ok) return new Response(JSON.stringify({ error: `catalog ${cat.status}` }), { status: 502, headers: JSON_HEADERS });
  const version = parseStoreVersion(await cat.text());
  if (!version) return new Response(JSON.stringify({ error: "no package" }), { status: 502, headers: JSON_HEADERS });
  return new Response(JSON.stringify({ version }), { headers: JSON_HEADERS });
}

async function latestRelease(env: Env): Promise<Response> {
  const headers: Record<string, string> = { accept: "application/vnd.github+json", "user-agent": "plain-browser.app site-api" };
  if (env.GITHUB_TOKEN) headers.authorization = `Bearer ${env.GITHUB_TOKEN}`;
  const gh = await fetch(`https://api.github.com/repos/${env.REPO}/releases?per_page=10`, { headers });
  if (!gh.ok) return new Response(JSON.stringify({ error: `github ${gh.status}` }), { status: 502, headers: JSON_HEADERS });
  const list = (await gh.json()) as GhRelease[];
  const r = list.find((x) => !x.draft);
  if (!r) return new Response(JSON.stringify({ error: "no release" }), { status: 404, headers: JSON_HEADERS });
  return new Response(JSON.stringify(pick(r)), { headers: JSON_HEADERS });
}

const ROUTES: Record<string, { handler: (env: Env) => Promise<Response>; ttl: (env: Env) => number }> = {
  "/api/latest-release": { handler: latestRelease, ttl: (env) => Number(env.CACHE_SECONDS) || 600 },
  "/api/store-version": { handler: storeVersion, ttl: (env) => Number(env.STORE_CACHE_SECONDS) || 900 },
};

export default {
  async fetch(req: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    const url = new URL(req.url);
    const route = ROUTES[url.pathname];
    if (!route) return new Response("not found", { status: 404 });
    if (req.method !== "GET" && req.method !== "HEAD") return new Response("method not allowed", { status: 405 });

    // Only successful answers are cached, so a failed upstream call is retried by the next visitor.
    const cacheKey = new Request(`${url.origin}${url.pathname}`, { method: "GET" });
    const cache = (caches as unknown as { default: Cache }).default;
    const hit = await cache.match(cacheKey);
    if (hit) return hit;

    const res = await route.handler(env);
    if (!res.ok) return res;
    const ttl = Math.max(60, route.ttl(env));
    const cached = new Response(res.body, { headers: { ...JSON_HEADERS, "cache-control": `public, max-age=${ttl}` } });
    ctx.waitUntil(cache.put(cacheKey, cached.clone()));
    return cached;
  },
};
