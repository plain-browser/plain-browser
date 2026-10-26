// GET /api/latest-release → newest published GitHub release (prereleases included), reduced to the
// fields the site needs, cached at the edge for CACHE_SECONDS. No logs, no client data forwarded.

type Env = { REPO: string; CACHE_SECONDS: string; GITHUB_TOKEN?: string };
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

export default {
  async fetch(req: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    const url = new URL(req.url);
    if (url.pathname !== "/api/latest-release") return new Response("not found", { status: 404 });
    if (req.method !== "GET" && req.method !== "HEAD") return new Response("method not allowed", { status: 405 });

    const ttl = Math.max(60, Number(env.CACHE_SECONDS) || 600);
    const cacheKey = new Request(`${url.origin}/api/latest-release`, { method: "GET" });
    const cache = (caches as unknown as { default: Cache }).default;
    const hit = await cache.match(cacheKey);
    if (hit) return hit;

    const headers: Record<string, string> = { accept: "application/vnd.github+json", "user-agent": "plain-browser.app site-api" };
    if (env.GITHUB_TOKEN) headers.authorization = `Bearer ${env.GITHUB_TOKEN}`;
    const gh = await fetch(`https://api.github.com/repos/${env.REPO}/releases?per_page=10`, { headers });
    if (!gh.ok) return new Response(JSON.stringify({ error: `github ${gh.status}` }), { status: 502, headers: JSON_HEADERS });
    const list = (await gh.json()) as GhRelease[];
    const r = list.find((x) => !x.draft);
    if (!r) return new Response(JSON.stringify({ error: "no release" }), { status: 404, headers: JSON_HEADERS });

    const res = new Response(JSON.stringify(pick(r)), {
      headers: { ...JSON_HEADERS, "cache-control": `public, max-age=${ttl}` },
    });
    ctx.waitUntil(cache.put(cacheKey, res.clone()));
    return res;
  },
};
