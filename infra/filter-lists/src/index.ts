// Filter list mirror for Cloudflare Workers (lists.plain-browser.app).
// A cron trigger fetches EasyList, EasyPrivacy and EasyList Polish from their publishers, checks that each is a
// plausible Adblock Plus list and stores it in KV with a manifest of SHA-256 hashes and sizes. The manifest is
// signed with an Ed25519 key (secret SIGNING_KEY); the browser (//plainbrowser/adblock FilterListUpdater) checks
// the signature with the public key compiled into it, reads the manifest once a day and downloads only lists
// whose hash changed. Browsers never contact the publishers, and the Worker neither logs nor stores anything about them.
// No dependencies.

// The part of the KV binding this Worker uses (wrangler.toml [[kv_namespaces]] binding = "LISTS").
export interface Kv {
  get(key: string): Promise<string | null>;
  put(key: string, value: string): Promise<void>;
}

export interface Env {
  LISTS: Kv;
  // Optional Rate Limiting binding (wrangler.toml [[unsafe.bindings]] type = "ratelimit").
  RATE_LIMITER?: { limit(options: { key: string }): Promise<{ success: boolean }> };
  // Optional. When set, requests must carry `X-Client: <value>/<version>`, e.g. "PlainBrowser/0.1.2".
  REQUIRED_CLIENT?: string;
  // Ed25519 private key, base64 of its PKCS#8 DER (scripts/keygen.mjs). Without it nothing is published.
  SIGNING_KEY?: string;
}

// The names are the browser's (filter_lists::kUpdatableLists); the URLs are the publishers' (lists/NOTICE.md).
export const SOURCES: Record<string, string> = {
  easylist: "https://easylist.to/easylist/easylist.txt",
  easyprivacy: "https://easylist.to/easylist/easyprivacy.txt",
  easylistpolish: "https://easylist-downloads.adblockplus.org/easylistpolish.txt",
};

// Bounds a real list is well inside (EasyList ~2 MB and ~70k rules, EasyList Polish ~1 MB); the browser accepts
// at most 16 MiB. A response outside them (an error page, a truncated file) is not published.
export const MIN_BYTES = 10 * 1024;
export const MAX_BYTES = 16 * 1024 * 1024;
export const MIN_RULES = 1000;
// A new version that drops more than this share of the published one's rules is held back as suspicious.
export const MAX_SHRINK = 0.5;

const USER_AGENT = "PlainBrowserListMirror/1 (+https://plain-browser.app)";
const CLIENT_HEADER = /^([A-Za-z0-9._-]{1,40})\/(\d{1,4}(\.\d{1,4}){0,3})$/;
const LIST_PATH = /^\/v1\/lists\/([a-z]+)\.txt$/;
// KV key of the published manifest text and its signature, kept together so a reader never gets a manifest
// with the signature of another one.
const SIGNED_KEY = "signed-manifest";

export interface Entry {
  sha256: string;
  size: number;
  rules: number;
  updated: string; // ISO time the Worker published this version
  source: string;
}
export interface Manifest {
  format: 1;
  generated: string;
  license: string;
  lists: Record<string, Entry>;
}

function text(status: number, body: string, extra: Record<string, string> = {}): Response {
  return new Response(body, {
    status,
    headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store", ...extra },
  });
}

function clientAllowed(request: Request, env: Env): boolean {
  if (!env.REQUIRED_CLIENT) return true;
  const m = CLIENT_HEADER.exec(request.headers.get("X-Client") ?? "");
  return m !== null && m[1] === env.REQUIRED_CLIENT;
}

export async function sha256Hex(data: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(data));
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, "0")).join("");
}

// Rule lines: not empty, not a comment ("!"), not the "[Adblock Plus ...]" header.
export function countRules(list: string): number {
  let n = 0;
  for (const line of list.split("\n")) {
    const t = line.trim();
    if (t && !t.startsWith("!") && !t.startsWith("[")) n++;
  }
  return n;
}

// Why `list` must not be published, or null when it is fine.
export function rejectReason(list: string, previous?: Entry): string | null {
  const bytes = new TextEncoder().encode(list).byteLength;
  if (bytes < MIN_BYTES || bytes > MAX_BYTES) return `size ${bytes}`;
  if (!list.replace(/^﻿/, "").startsWith("[Adblock Plus")) return "no [Adblock Plus] header";
  const rules = countRules(list);
  if (rules < MIN_RULES) return `only ${rules} rules`;
  if (previous && rules < previous.rules * (1 - MAX_SHRINK)) return `shrank from ${previous.rules} to ${rules} rules`;
  return null;
}

interface Signed {
  manifest: string; // the exact bytes that were signed and are served
  signature: string; // base64 Ed25519 signature of `manifest`
}

async function readSigned(env: Env): Promise<{ signed: Signed; manifest: Manifest } | null> {
  const raw = await env.LISTS.get(SIGNED_KEY);
  if (!raw) return null;
  try {
    const signed = JSON.parse(raw) as Signed;
    const manifest = JSON.parse(signed.manifest) as Manifest;
    return manifest.format === 1 && manifest.lists ? { signed, manifest } : null;
  } catch {
    return null;
  }
}

function fromBase64(b64: string): Uint8Array {
  return Uint8Array.from(atob(b64.trim()), (c) => c.charCodeAt(0));
}

function toBase64(bytes: ArrayBuffer): string {
  return btoa(String.fromCharCode(...new Uint8Array(bytes)));
}

export async function sign(text: string, pkcs8Base64: string): Promise<string> {
  const key = await crypto.subtle.importKey("pkcs8", fromBase64(pkcs8Base64), { name: "Ed25519" }, false, ["sign"]);
  return toBase64(await crypto.subtle.sign({ name: "Ed25519" }, key, new TextEncoder().encode(text)));
}

// Cron: fetch every source, publish the ones that changed and passed the checks. The list is written before the
// manifest that names it, so a reader that sees the new hash finds the new file (KV may still serve the old one
// for a minute; the browser then sees a hash mismatch and tries again on its next check).
export async function refresh(env: Env, now = new Date()): Promise<Record<string, string>> {
  if (!env.SIGNING_KEY) return { refused: "SIGNING_KEY is not set (scripts/keygen.mjs)" };
  const published = await readSigned(env);
  const manifest: Manifest = published?.manifest ?? {
    format: 1,
    generated: "",
    license: "GPLv3 or CC BY-SA 3.0, https://easylist.to/pages/licence.html",
    lists: {},
  };
  const report: Record<string, string> = {};
  let changed = false;
  for (const [name, url] of Object.entries(SOURCES)) {
    let body: string;
    try {
      const res = await fetch(url, { headers: { "User-Agent": USER_AGENT }, redirect: "follow" });
      if (res.status !== 200) {
        report[name] = `kept: HTTP ${res.status}`;
        continue;
      }
      body = await res.text();
    } catch {
      report[name] = "kept: fetch failed";
      continue;
    }
    const previous = manifest.lists[name];
    const reason = rejectReason(body, previous);
    if (reason) {
      report[name] = `kept: ${reason}`;
      continue;
    }
    const sha256 = await sha256Hex(body);
    if (previous?.sha256 === sha256) {
      report[name] = "unchanged";
      continue;
    }
    await env.LISTS.put(`list:${name}`, body);
    manifest.lists[name] = {
      sha256,
      size: new TextEncoder().encode(body).byteLength,
      rules: countRules(body),
      updated: now.toISOString(),
      source: url,
    };
    report[name] = "published";
    changed = true;
  }
  // Signed again when nothing was published yet under this key (first run, or a new key).
  if (changed) manifest.generated = now.toISOString();
  const text = JSON.stringify(manifest);
  const signature = await sign(text, env.SIGNING_KEY);
  if (changed || !published || published.signed.signature !== signature) {
    await env.LISTS.put(SIGNED_KEY, JSON.stringify({ manifest: text, signature } satisfies Signed));
  }
  return report;
}

export async function handle(request: Request, env: Env): Promise<Response> {
  const url = new URL(request.url);
  const listMatch = LIST_PATH.exec(url.pathname);
  const isManifest = url.pathname === "/v1/manifest.json";
  const isSignature = url.pathname === "/v1/manifest.json.sig";
  if (!isManifest && !isSignature && !(listMatch && listMatch[1] in SOURCES)) {
    return text(404, "Not found");
  }
  if (request.method !== "GET" && request.method !== "HEAD") {
    return text(405, "Method not allowed", { Allow: "GET, HEAD" });
  }
  if (!clientAllowed(request, env)) return text(403, "Forbidden");

  // The limiter key is the client IP. It is only used inside Cloudflare's limiter and is never stored.
  if (env.RATE_LIMITER) {
    const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
    const { success } = await env.RATE_LIMITER.limit({ key: ip });
    if (!success) return text(429, "Too many requests", { "Retry-After": "60" });
  }

  const published = await readSigned(env);
  if (!published) return text(503, "Not ready", { "Retry-After": "3600" });
  const { signed, manifest } = published;

  if (isManifest || isSignature) {
    // The browser checks once a day; a few minutes of caching keeps the edge from reading KV for every client.
    // Manifest and signature come from one KV value; an edge cache may still pair an old one with a new one for
    // up to five minutes, which the browser sees as a bad signature and retries an hour later.
    return new Response(request.method === "HEAD" ? null : isManifest ? signed.manifest : signed.signature, {
      status: 200,
      headers: {
        "Content-Type": isManifest ? "application/json" : "text/plain; charset=utf-8",
        "Cache-Control": "public, max-age=300",
      },
    });
  }
  const name = listMatch[1];
  const entry = manifest.lists[name];
  const body = entry ? await env.LISTS.get(`list:${name}`) : null;
  if (!entry || body === null) return text(404, "Not found");
  return new Response(request.method === "HEAD" ? null : body, {
    status: 200,
    headers: {
      "Content-Type": "text/plain; charset=utf-8",
      // Lists are fetched only after the manifest named their hash; the ETag lets caches tell versions apart.
      "Cache-Control": "public, max-age=300",
      ETag: `"${entry.sha256}"`,
    },
  });
}

export default {
  fetch: handle,
  async scheduled(_event: unknown, env: Env, ctx: { waitUntil(p: Promise<unknown>): void }) {
    ctx.waitUntil(refresh(env));
  },
};
