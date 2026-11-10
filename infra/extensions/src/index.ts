// Extension repository for Cloudflare Workers (ext.plain-browser.app).
// Serves the curated extensions that Plain Browser installs, from an R2 bucket that scripts/publish.mjs fills:
// the signed CRX files, catalog.json and updates.xml, the gupdate response Chromium's extension updater reads
// through the update_url in each extension's manifest. The Worker only reads; it neither logs nor stores anything
// about clients. No dependencies.

// The part of the R2 binding this Worker uses (wrangler.toml [[r2_buckets]] binding = "EXTENSIONS").
export interface R2Object {
  size: number;
  httpEtag: string;
}
export interface R2ObjectBody extends R2Object {
  body: ReadableStream | string | ArrayBuffer;
}
export interface R2 {
  get(key: string): Promise<R2ObjectBody | null>;
  head(key: string): Promise<R2Object | null>;
}

export interface Env {
  EXTENSIONS: R2;
  // Optional Rate Limiting binding (wrangler.toml [[unsafe.bindings]] type = "ratelimit").
  RATE_LIMITER?: { limit(options: { key: string }): Promise<{ success: boolean }> };
}

// The slugs of scripts/extensions.mjs SOURCES (a test checks they match).
export const SLUGS = ["ublock", "umatrix", "noscript", "adguard"];

// /v1/crx/<slug>.crx (latest) or /v1/crx/<slug>-<version>.crx; versions as Chromium writes them.
const CRX_PATH = /^\/v1\/crx\/([a-z]{1,32})(?:-(\d{1,5}(?:\.\d{1,5}){0,3}))?\.crx$/;

interface Route {
  key: string;
  type: string;
  cache: string;
}

function route(pathname: string): Route | null {
  // The updater asks often and each answer is small; five minutes at the edge spares R2 most reads.
  if (pathname === "/v1/updates.xml") return { key: "updates.xml", type: "application/xml", cache: "public, max-age=300" };
  if (pathname === "/v1/catalog.json") return { key: "catalog.json", type: "application/json", cache: "public, max-age=300" };
  const m = CRX_PATH.exec(pathname);
  if (!m || !SLUGS.includes(m[1])) return null;
  return {
    key: `crx/${m[1]}${m[2] ? `-${m[2]}` : ""}.crx`,
    type: "application/x-chrome-extension",
    // A versioned file changes only if the build itself changes; the latest copy changes with each release.
    cache: m[2] ? "public, max-age=3600" : "public, max-age=300",
  };
}

function text(status: number, body: string, extra: Record<string, string> = {}): Response {
  return new Response(body, {
    status,
    headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store", ...extra },
  });
}

export async function handle(request: Request, env: Env): Promise<Response> {
  const url = new URL(request.url);
  // The query string (Chromium's updater sends ?response=updatecheck&x=...) is ignored: everyone gets the same file.
  const r = route(url.pathname);
  if (!r) return text(404, "Not found");
  if (request.method !== "GET" && request.method !== "HEAD") {
    return text(405, "Method not allowed", { Allow: "GET, HEAD" });
  }

  // The limiter key is the client IP. It is only used inside Cloudflare's limiter and is never stored.
  if (env.RATE_LIMITER) {
    const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
    const { success } = await env.RATE_LIMITER.limit({ key: ip });
    if (!success) return text(429, "Too many requests", { "Retry-After": "60" });
  }

  const head = request.method === "HEAD";
  const obj = head ? await env.EXTENSIONS.head(r.key) : await env.EXTENSIONS.get(r.key);
  if (!obj) return text(404, "Not found");
  const headers = {
    "Content-Type": r.type,
    "Cache-Control": r.cache,
    ETag: obj.httpEtag,
    "X-Content-Type-Options": "nosniff",
  };
  const inm = request.headers.get("If-None-Match");
  if (inm && inm.split(",").some((t) => t.trim() === obj.httpEtag)) return new Response(null, { status: 304, headers });
  // A GET body from R2 carries its length; a HEAD states it.
  if (head) return new Response(null, { status: 200, headers: { ...headers, "Content-Length": String(obj.size) } });
  return new Response((obj as R2ObjectBody).body, { status: 200, headers });
}

export default { fetch: handle };
