// Crash report collector for Cloudflare Workers (crash.plain-browser.app).
// Crashpad in the browser uploads a report only when the user turned "Send crash reports" on (//plainbrowser/crash,
// off by default): a multipart/form-data POST, usually gzip-compressed, with the minidump in upload_file_minidump
// and the process annotations (prod, ver, channel, plat, ptype, crash keys) as form fields. The Worker checks it,
// drops the fields that could identify an install or a person, and stores the minidump and the remaining fields in
// R2. It answers with the report ID, which Crashpad shows on chrome://crashes.
// It logs nothing and stores no IP address, cookie or time finer than the day. No dependencies.

// The part of the R2 binding this Worker uses (wrangler.toml [[r2_buckets]] binding = "REPORTS").
export interface Bucket {
  put(key: string, value: ArrayBuffer | string, options?: { httpMetadata?: { contentType?: string } }): Promise<unknown>;
}

export interface Env {
  REPORTS: Bucket;
  // Optional Rate Limiting binding (wrangler.toml [[unsafe.bindings]] type = "ratelimit").
  RATE_LIMITER?: { limit(options: { key: string }): Promise<{ success: boolean }> };
}

// A Chromium minidump is a few hundred KiB (stable channel) up to a few MiB (full-memory dumps on other channels).
export const MAX_BODY_BYTES = 20 * 1024 * 1024; // as sent, usually gzip
export const MAX_DUMP_BYTES = 32 * 1024 * 1024; // after decompression
export const MAX_FIELDS = 400;
export const MAX_VALUE_BYTES = 4096;

// The product annotation of our builds (install_static, the version resource's product name).
const PRODUCT = /^Plain[ _]?Browser$/;
const VERSION = /^\d{1,4}(\.\d{1,4}){1,3}$/;
const KEY = /^[A-Za-z0-9._-]{1,64}$/;
// Never stored: a client ID ("guid", Chromium sets it only with metrics on, which Plain Browser does not have),
// command-line switches (they can hold a user-data-dir under C:\Users\<name>) and anything named like a URL or path.
const DROPPED = /^(guid|metrics_client_id|num-switches|switch-\d+)$|url|path|dir/i;
const MINIDUMP_FIELD = "upload_file_minidump";
const MDMP = [0x4d, 0x44, 0x4d, 0x50]; // "MDMP", the minidump signature

function text(status: number, body: string, extra: Record<string, string> = {}): Response {
  return new Response(body, {
    status,
    headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store", ...extra },
  });
}

async function readLimited(stream: ReadableStream<Uint8Array>, limit: number): Promise<Uint8Array | null> {
  const reader = stream.getReader();
  const chunks: Uint8Array[] = [];
  let size = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    size += value.byteLength;
    if (size > limit) {
      await reader.cancel();
      return null;
    }
    chunks.push(value);
  }
  const out = new Uint8Array(size);
  let offset = 0;
  for (const c of chunks) {
    out.set(c, offset);
    offset += c.byteLength;
  }
  return out;
}

// The form fields worth keeping, or why the report is refused.
export function cleanFields(form: FormData): { fields: Record<string, string> } | { error: string } {
  const fields: Record<string, string> = {};
  let count = 0;
  for (const [key, value] of form.entries()) {
    if (key === MINIDUMP_FIELD || typeof value !== "string") continue;
    if (++count > MAX_FIELDS) return { error: "too many fields" };
    if (!KEY.test(key) || DROPPED.test(key)) continue;
    fields[key] = value.length > MAX_VALUE_BYTES ? value.slice(0, MAX_VALUE_BYTES) : value;
  }
  if (!PRODUCT.test(fields.prod ?? "")) return { error: "unknown product" };
  if (!VERSION.test(fields.ver ?? "")) return { error: "bad version" };
  return { fields };
}

export async function handle(request: Request, env: Env, now = new Date()): Promise<Response> {
  const url = new URL(request.url);
  if (url.pathname !== "/report") return text(404, "Not found");
  if (request.method !== "POST") return text(405, "Method not allowed", { Allow: "POST" });

  // The limiter key is the client IP. It is only used inside Cloudflare's limiter and is never stored.
  if (env.RATE_LIMITER) {
    const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
    const { success } = await env.RATE_LIMITER.limit({ key: ip });
    if (!success) return text(429, "Too many requests", { "Retry-After": "3600" });
  }

  const type = request.headers.get("Content-Type") ?? "";
  if (!type.startsWith("multipart/form-data") || !request.body) return text(400, "Expected multipart/form-data");
  const declared = Number(request.headers.get("Content-Length") ?? "0");
  if (declared > MAX_BODY_BYTES) return text(413, "Too large");

  let body: ReadableStream<Uint8Array> = request.body;
  const encoding = (request.headers.get("Content-Encoding") ?? "").toLowerCase();
  if (encoding === "gzip") {
    body = body.pipeThrough(new DecompressionStream("gzip"));
  } else if (encoding && encoding !== "identity") {
    return text(415, "Unsupported encoding");
  }
  let raw: Uint8Array | null;
  try {
    raw = await readLimited(body, MAX_DUMP_BYTES + MAX_BODY_BYTES);
  } catch {
    return text(400, "Bad body");
  }
  if (!raw) return text(413, "Too large");

  let form: FormData;
  try {
    form = await new Response(raw, { headers: { "Content-Type": type } }).formData();
  } catch {
    return text(400, "Bad form");
  }
  const dump = form.get(MINIDUMP_FIELD);
  if (!dump || typeof dump === "string") return text(400, "No minidump");
  if (dump.size > MAX_DUMP_BYTES) return text(413, "Too large");
  const bytes = await dump.arrayBuffer();
  const head = new Uint8Array(bytes, 0, Math.min(4, bytes.byteLength));
  if (head.length < 4 || MDMP.some((b, i) => head[i] !== b)) return text(400, "Not a minidump");

  const cleaned = cleanFields(form);
  if ("error" in cleaned) return text(400, cleaned.error);

  const id = crypto.randomUUID();
  const day = now.toISOString().slice(0, 10);
  const meta = { format: 1, id, received: day, size: bytes.byteLength, fields: cleaned.fields };
  // The dump first: a JSON without its dump would point at nothing.
  await env.REPORTS.put(`reports/${day}/${id}.dmp`, bytes, { httpMetadata: { contentType: "application/octet-stream" } });
  await env.REPORTS.put(`reports/${day}/${id}.json`, JSON.stringify(meta), { httpMetadata: { contentType: "application/json" } });
  // Crashpad stores the response body as the report's server ID.
  return text(200, id);
}

export default { fetch: handle };
