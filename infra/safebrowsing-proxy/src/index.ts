// Safe Browsing v4 privacy proxy for Cloudflare Workers.
// Forwards the browser's requests to safebrowsing.googleapis.com with the operator's API key and without
// anything that identifies the client: no IP, no cookies, no User-Agent, no Referer, no CF-* headers.
// Nothing is logged or stored. No dependencies.

export interface Env {
  GOOGLE_SB_KEY: string;
  // Optional Rate Limiting binding (wrangler.toml [[unsafe.bindings]] type = "ratelimit").
  RATE_LIMITER?: { limit(options: { key: string }): Promise<{ success: boolean }> };
  // Optional. When set, requests must carry `X-Client: <value>/<version>`, e.g. "PlainBrowser/0.1.0".
  REQUIRED_CLIENT?: string;
}

const UPSTREAM = "https://safebrowsing.googleapis.com";
const ALLOWED_PATHS = new Set([
  "/v4/threatListUpdates:fetch",
  "/v4/fullHashes:find",
  "/v4/threatMatches:find",
]);
const NEUTRAL_USER_AGENT = "Mozilla/5.0 (compatible; SafeBrowsingProxy)";
// Largest body any v4 request needs is a few hundred KB (threatMatches with many entries).
const MAX_BODY_BYTES = 1024 * 1024;
// The version part must not be unique per user: major.minor.patch only.
const CLIENT_HEADER = /^([A-Za-z0-9._-]{1,40})\/(\d{1,4}(\.\d{1,4}){0,3})$/;

function text(status: number, body: string): Response {
  return new Response(body, {
    status,
    headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" },
  });
}

function clientAllowed(request: Request, env: Env): boolean {
  if (!env.REQUIRED_CLIENT) return true;
  const m = CLIENT_HEADER.exec(request.headers.get("X-Client") ?? "");
  return m !== null && m[1] === env.REQUIRED_CLIENT;
}

export async function handle(request: Request, env: Env): Promise<Response> {
  const url = new URL(request.url);

  if (!ALLOWED_PATHS.has(url.pathname)) return text(404, "Not found");
  if (request.method !== "GET" && request.method !== "POST") {
    return new Response("Method not allowed", {
      status: 405,
      headers: { Allow: "GET, POST", "Cache-Control": "no-store" },
    });
  }
  if (!env.GOOGLE_SB_KEY) return text(500, "Proxy is not configured");
  if (!clientAllowed(request, env)) return text(403, "Forbidden");

  // The limiter key is the client IP. It is only used inside Cloudflare's limiter and is never forwarded.
  if (env.RATE_LIMITER) {
    const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
    const { success } = await env.RATE_LIMITER.limit({ key: ip });
    if (!success) {
      return new Response("Too many requests", {
        status: 429,
        headers: { "Retry-After": "60", "Cache-Control": "no-store" },
      });
    }
  }

  let method = request.method;
  let body: ArrayBuffer | undefined;
  let bodyContentType = request.headers.get("Content-Type");
  if (request.method === "POST") {
    const declared = Number(request.headers.get("Content-Length") ?? "0");
    if (declared > MAX_BODY_BYTES) return text(413, "Payload too large");
    body = await request.arrayBuffer();
    if (body.byteLength > MAX_BODY_BYTES) return text(413, "Payload too large");
  }

  // Keep the client's query ($ct, alt, ...) but replace any key it sent with ours.
  const upstream = new URL(UPSTREAM + url.pathname);
  for (const [k, v] of url.searchParams) {
    if (k.toLowerCase() !== "key" && k !== "$req") upstream.searchParams.append(k, v);
  }
  upstream.searchParams.set("key", env.GOOGLE_SB_KEY);

  // Chromium sends the v4 request as GET with the protobuf in `$req` (base64) plus
  // "X-HTTP-Method-Override: POST". Google answers that envelope with 403 "ThreatType is not
  // accessible" unless the override header is there, and the header is not on our allow-list.
  // So the proxy unwraps it: the decoded `$req` becomes the body of an ordinary POST, which is
  // the same request Google accepts from any client.
  const reqParam = url.searchParams.get("$req");
  if (request.method === "GET" && reqParam !== null) {
    let decoded: Uint8Array;
    try {
      decoded = Uint8Array.from(atob(reqParam.replace(/-/g, "+").replace(/_/g, "/")), (c) => c.charCodeAt(0));
    } catch {
      return text(400, "Bad $req");
    }
    if (decoded.byteLength > MAX_BODY_BYTES) return text(413, "Payload too large");
    method = "POST";
    body = decoded.buffer as ArrayBuffer;
    bodyContentType = "application/x-protobuf";
  }

  // Allow-list, not block-list: only these headers are ever sent.
  const headers = new Headers({ "User-Agent": NEUTRAL_USER_AGENT });
  if (bodyContentType) headers.set("Content-Type", bodyContentType);

  let res: Response;
  try {
    res = await fetch(upstream.toString(), {
      method,
      headers,
      body,
      redirect: "manual",
      cf: { cacheTtl: 0, cacheEverything: false },
    } as RequestInit);
  } catch {
    return text(502, "Upstream unavailable");
  }

  const out = new Headers({ "Cache-Control": "no-store" });
  const ct = res.headers.get("Content-Type");
  if (ct) out.set("Content-Type", ct);
  return new Response(res.body, { status: res.status, headers: out });
}

export default {
  fetch: handle,
};
