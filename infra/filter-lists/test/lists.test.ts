// Run: node --test test/   (Node 22.18+ runs TypeScript directly)
import { test } from "node:test";
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { countRules, handle, MIN_RULES, refresh, rejectReason, SOURCES, type Env, type Manifest } from "../src/index.ts";

class FakeKv {
  data = new Map<string, string>();
  puts: string[] = [];
  async get(key: string) {
    return this.data.get(key) ?? null;
  }
  async put(key: string, value: string) {
    this.puts.push(key);
    this.data.set(key, value);
  }
}

// A plausible list: header, comments and `rules` rules, with a marker so versions differ.
function list(marker: string, rules = 2000): string {
  const lines = ["[Adblock Plus 2.0]", `! Title: ${marker}`, "! Expires: 4 days"];
  for (let i = 0; i < rules; i++) lines.push(`||ads-${marker}-${i}.example^`);
  return lines.join("\n") + "\n";
}

function mockPublishers(bodies: Record<string, string | number>) {
  const fetched: string[] = [];
  globalThis.fetch = (async (url: string) => {
    fetched.push(url);
    const name = Object.keys(SOURCES).find((n) => SOURCES[n] === url)!;
    const b = bodies[name];
    if (typeof b === "number") return new Response("error", { status: b });
    return new Response(b, { status: 200, headers: { "Set-Cookie": "x=y" } });
  }) as typeof fetch;
  return fetched;
}

const sha = (s: string) => createHash("sha256").update(s).digest("hex");
const manifestOf = (kv: FakeKv) => JSON.parse(kv.data.get("manifest")!) as Manifest;

test("refresh publishes valid lists with hash, size and rule count; list before manifest", async () => {
  const kv = new FakeKv();
  const bodies = { easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") };
  mockPublishers(bodies);
  const report = await refresh({ LISTS: kv }, new Date("2026-10-27T12:00:00Z"));
  assert.deepEqual(report, { easylist: "published", easyprivacy: "published", easylistpolish: "published" });
  const m = manifestOf(kv);
  assert.equal(m.format, 1);
  assert.equal(m.generated, "2026-10-27T12:00:00.000Z");
  assert.equal(m.lists.easylist.sha256, sha(bodies.easylist));
  assert.equal(m.lists.easylist.size, Buffer.byteLength(bodies.easylist));
  assert.equal(m.lists.easylist.rules, 2000);
  assert.equal(kv.data.get("list:easylistpolish"), bodies.easylistpolish);
  assert.equal(kv.puts.at(-1), "manifest");
});

test("refresh keeps the published version when a source fails or looks wrong", async () => {
  const kv = new FakeKv();
  mockPublishers({ easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") });
  await refresh({ LISTS: kv });
  const before = manifestOf(kv);
  kv.puts = [];
  mockPublishers({
    easylist: 503,
    easyprivacy: "<html>Cloudflare error</html>".padEnd(20000, " "),
    easylistpolish: list("pl2", 900), // shrank by more than half
  });
  const report = await refresh({ LISTS: kv });
  assert.equal(report.easylist, "kept: HTTP 503");
  assert.equal(report.easyprivacy, "kept: no [Adblock Plus] header");
  assert.match(report.easylistpolish, /^kept: (only|shrank)/);
  assert.deepEqual(kv.puts, []); // nothing written, manifest untouched
  assert.deepEqual(manifestOf(kv), before);
});

test("refresh writes only what changed", async () => {
  const kv = new FakeKv();
  mockPublishers({ easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") });
  await refresh({ LISTS: kv });
  kv.puts = [];
  mockPublishers({ easylist: list("a"), easyprivacy: list("p-new"), easylistpolish: list("pl") });
  const report = await refresh({ LISTS: kv });
  assert.deepEqual(report, { easylist: "unchanged", easyprivacy: "published", easylistpolish: "unchanged" });
  assert.deepEqual(kv.puts, ["list:easyprivacy", "manifest"]);
});

test("rejectReason bounds", () => {
  assert.match(rejectReason("[Adblock Plus 2.0]\n||a^\n")!, /^size/);
  assert.equal(rejectReason(list("ok")), null);
  assert.match(rejectReason(list("few", MIN_RULES - 1))!, /only/);
  assert.equal(countRules("[Adblock Plus]\n! c\n\n||a^\n##.b\n"), 2);
  assert.equal(rejectReason("﻿" + list("bom")), null);
});

const env = (kv: FakeKv, extra: Partial<Env> = {}): Env => ({ LISTS: kv, REQUIRED_CLIENT: "PlainBrowser", ...extra });
const get = (path: string, headers: Record<string, string> = { "X-Client": "PlainBrowser/0.1.2" }, method = "GET") =>
  new Request("https://lists.example" + path, { method, headers });

test("serves the manifest and the lists to the browser", async () => {
  const kv = new FakeKv();
  const bodies = { easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") };
  mockPublishers(bodies);
  await refresh({ LISTS: kv });

  const res = await handle(get("/v1/manifest.json"), env(kv));
  assert.equal(res.status, 200);
  assert.equal(res.headers.get("Content-Type"), "application/json");
  const m = (await res.json()) as Manifest;
  assert.equal(m.lists.easyprivacy.sha256, sha(bodies.easyprivacy));

  const l = await handle(get("/v1/lists/easyprivacy.txt"), env(kv));
  assert.equal(l.status, 200);
  const text = await l.text();
  assert.equal(sha(text), m.lists.easyprivacy.sha256); // what the browser verifies
  assert.equal(l.headers.get("ETag"), `"${m.lists.easyprivacy.sha256}"`);
  assert.equal(l.headers.get("Set-Cookie"), null);

  const head = await handle(get("/v1/manifest.json", undefined, "HEAD"), env(kv));
  assert.equal(head.status, 200);
  assert.equal(await head.text(), "");
});

test("refuses other paths, methods and clients", async () => {
  const kv = new FakeKv();
  mockPublishers({ easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") });
  await refresh({ LISTS: kv });
  assert.equal((await handle(get("/v1/lists/other.txt"), env(kv))).status, 404);
  assert.equal((await handle(get("/v1/lists/../manifest"), env(kv))).status, 404);
  assert.equal((await handle(get("/"), env(kv))).status, 404);
  assert.equal((await handle(get("/v1/manifest.json", undefined, "POST"), env(kv))).status, 405);
  assert.equal((await handle(get("/v1/manifest.json", {}), env(kv))).status, 403);
  assert.equal((await handle(get("/v1/manifest.json", { "X-Client": "Other/1.0" }), env(kv))).status, 403);
  // Without REQUIRED_CLIENT any client may read.
  assert.equal((await handle(get("/v1/manifest.json", {}), { LISTS: kv })).status, 200);
});

test("503 before the first refresh, 429 from the rate limiter", async () => {
  const kv = new FakeKv();
  const empty = await handle(get("/v1/manifest.json"), env(kv));
  assert.equal(empty.status, 503);
  assert.equal(empty.headers.get("Cache-Control"), "no-store");
  mockPublishers({ easylist: list("a"), easyprivacy: list("p"), easylistpolish: list("pl") });
  await refresh({ LISTS: kv });
  const limited = await handle(
    get("/v1/manifest.json"),
    env(kv, { RATE_LIMITER: { limit: async () => ({ success: false }) } }),
  );
  assert.equal(limited.status, 429);
});
