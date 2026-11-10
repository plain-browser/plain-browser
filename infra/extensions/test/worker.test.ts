// Run: node --test test/   (Node 22.18+ runs TypeScript directly)
import { test } from "node:test";
import assert from "node:assert/strict";
import { handle, type Env, type R2Object, type R2ObjectBody } from "../src/index.ts";

class FakeR2 {
  data = new Map<string, string>();
  reads: string[] = [];
  async get(key: string): Promise<R2ObjectBody | null> {
    this.reads.push(key);
    const body = this.data.get(key);
    return body === undefined ? null : { body, size: Buffer.byteLength(body), httpEtag: `"etag-${key}"` };
  }
  async head(key: string): Promise<R2Object | null> {
    this.reads.push(key);
    const body = this.data.get(key);
    return body === undefined ? null : { size: Buffer.byteLength(body), httpEtag: `"etag-${key}"` };
  }
}

function bucket(): FakeR2 {
  const r2 = new FakeR2();
  r2.data.set("updates.xml", "<gupdate/>");
  r2.data.set("catalog.json", '{"ublock":{}}');
  r2.data.set("crx/ublock.crx", "Cr24 latest");
  r2.data.set("crx/ublock-1.76.0.crx", "Cr24 versioned");
  r2.data.set("crx/adguard-5.5.3.3.crx", "Cr24 adguard");
  return r2;
}
const env = (r2: FakeR2, extra: Partial<Env> = {}): Env => ({ EXTENSIONS: r2, ...extra });
const req = (path: string, method = "GET", headers: Record<string, string> = {}) =>
  new Request("https://ext.example" + path, { method, headers });

test("serves updates.xml and catalog.json, no client header needed", async () => {
  const r2 = bucket();
  // What Chromium's extension updater sends: a GET with a query string and no X-Client.
  const res = await handle(req("/v1/updates.xml?response=updatecheck&x=id%3Dabc%26v%3D1.0"), env(r2));
  assert.equal(res.status, 200);
  assert.equal(res.headers.get("Content-Type"), "application/xml");
  assert.equal(res.headers.get("Cache-Control"), "public, max-age=300");
  assert.equal(await res.text(), "<gupdate/>");
  const cat = await handle(req("/v1/catalog.json"), env(r2));
  assert.equal(cat.headers.get("Content-Type"), "application/json");
  assert.equal(await cat.text(), '{"ublock":{}}');
});

test("serves CRX files, latest and versioned, with an ETag", async () => {
  const r2 = bucket();
  const latest = await handle(req("/v1/crx/ublock.crx"), env(r2));
  assert.equal(latest.status, 200);
  assert.equal(latest.headers.get("Content-Type"), "application/x-chrome-extension");
  assert.equal(latest.headers.get("ETag"), '"etag-crx/ublock.crx"');
  assert.equal(latest.headers.get("Cache-Control"), "public, max-age=300");
  assert.equal(await latest.text(), "Cr24 latest");
  const v = await handle(req("/v1/crx/ublock-1.76.0.crx"), env(r2));
  assert.equal(await v.text(), "Cr24 versioned");
  assert.equal(v.headers.get("Cache-Control"), "public, max-age=3600");
  assert.equal((await handle(req("/v1/crx/adguard-5.5.3.3.crx"), env(r2))).status, 200);
  // Known slug, version not published.
  assert.equal((await handle(req("/v1/crx/ublock-1.0.crx"), env(r2))).status, 404);
  assert.equal((await handle(req("/v1/crx/noscript.crx"), env(r2))).status, 404);

  const head = await handle(req("/v1/crx/ublock.crx", "HEAD"), env(r2));
  assert.equal(head.status, 200);
  assert.equal(head.headers.get("Content-Length"), String(Buffer.byteLength("Cr24 latest")));
  assert.equal(await head.text(), "");

  const cached = await handle(req("/v1/crx/ublock.crx", "GET", { "If-None-Match": '"etag-crx/ublock.crx"' }), env(r2));
  assert.equal(cached.status, 304);
  assert.equal(await cached.text(), "");
});

test("refuses other paths and methods without touching R2", async () => {
  const r2 = bucket();
  for (const path of [
    "/",
    "/v1/",
    "/v1/updates.xml/",
    "/v1/crx/other.crx",
    "/v1/crx/ublock-1.76.0.zip",
    "/v1/crx/ublock-v1.76.0.crx",
    "/v1/crx/ublock-1.2.3.4.5.crx",
    "/v1/crx/../catalog.json.crx",
    "/v1/crx/%2e%2e%2fupdates.crx",
    "/v1/crx/UBLOCK.crx",
    "/v2/updates.xml",
    "/updates.xml",
  ]) {
    assert.equal((await handle(req(path), env(r2))).status, 404, path);
  }
  const post = await handle(req("/v1/updates.xml", "POST"), env(r2));
  assert.equal(post.status, 405);
  assert.equal(post.headers.get("Allow"), "GET, HEAD");
  assert.equal((await handle(req("/v1/crx/ublock.crx", "PUT"), env(r2))).status, 405);
  assert.deepEqual(r2.reads, []);
});

test("404 before the first publish, 429 from the rate limiter", async () => {
  const empty = await handle(req("/v1/updates.xml"), env(new FakeR2()));
  assert.equal(empty.status, 404);
  assert.equal(empty.headers.get("Cache-Control"), "no-store");
  const keys: string[] = [];
  const limited = await handle(
    req("/v1/updates.xml", "GET", { "CF-Connecting-IP": "192.0.2.1" }),
    env(bucket(), { RATE_LIMITER: { limit: async ({ key }) => (keys.push(key), { success: false }) } }),
  );
  assert.equal(limited.status, 429);
  assert.equal(limited.headers.get("Retry-After"), "60");
  assert.deepEqual(keys, ["192.0.2.1"]);
});
