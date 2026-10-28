// Run: node --test test/   (Node 22.18+ runs TypeScript directly)
import { test } from "node:test";
import assert from "node:assert/strict";
import { gzipSync } from "node:zlib";
import { handle, MAX_BODY_BYTES, type Env } from "../src/index.ts";

class FakeBucket {
  data = new Map<string, ArrayBuffer | string>();
  async put(key: string, value: ArrayBuffer | string) {
    this.data.set(key, value);
  }
}

function env(limited = false): Env & { REPORTS: FakeBucket } {
  return {
    REPORTS: new FakeBucket(),
    RATE_LIMITER: { limit: async () => ({ success: !limited }) },
  };
}

const MINIDUMP = new Uint8Array([0x4d, 0x44, 0x4d, 0x50, 0x93, 0xa7, 0, 0, 1, 2, 3, 4]);

// A report as Crashpad builds it: annotations as fields, the dump as a file part.
function form(fields: Record<string, string>, dump: Uint8Array | null = MINIDUMP): FormData {
  const f = new FormData();
  for (const [k, v] of Object.entries(fields)) f.append(k, v);
  if (dump) f.append("upload_file_minidump", new Blob([dump]), "3f1c.dmp");
  return f;
}

const GOOD = { prod: "Plain_Browser", ver: "156.0.8078.47", channel: "", plat: "Win64", ptype: "renderer" };

// The multipart bytes and their Content-Type, optionally gzip-compressed like Crashpad's upload.
async function post(f: FormData, gzip = false, extra: Record<string, string> = {}): Promise<Request> {
  const r = new Request("https://crash.plain-browser.app/report", { method: "POST", body: f });
  const type = r.headers.get("Content-Type")!;
  let body = new Uint8Array(await r.arrayBuffer());
  const headers: Record<string, string> = { "Content-Type": type, ...extra };
  if (gzip) {
    body = new Uint8Array(gzipSync(body));
    headers["Content-Encoding"] = "gzip";
  }
  return new Request("https://crash.plain-browser.app/report", { method: "POST", body, headers });
}

const NOW = new Date("2026-10-28T10:31:07Z");

test("a gzip report is stored as dump and JSON under the day, and the ID comes back", async () => {
  const e = env();
  const res = await handle(await post(form(GOOD), true), e, NOW);
  assert.equal(res.status, 200);
  const id = await res.text();
  assert.match(id, /^[0-9a-f-]{36}$/);
  const dump = e.REPORTS.data.get(`reports/2026-10-28/${id}.dmp`) as ArrayBuffer;
  assert.deepEqual(new Uint8Array(dump), MINIDUMP);
  const meta = JSON.parse(e.REPORTS.data.get(`reports/2026-10-28/${id}.json`) as string);
  assert.deepEqual(meta, { format: 1, id, received: "2026-10-28", size: MINIDUMP.byteLength, fields: GOOD });
});

test("an uncompressed report is accepted too", async () => {
  const res = await handle(await post(form(GOOD)), env(), NOW);
  assert.equal(res.status, 200);
});

test("identifying fields are dropped, crash keys kept", async () => {
  const e = env();
  const fields = {
    ...GOOD,
    guid: "0123456789abcdef",
    "num-switches": "2",
    "switch-1": "--user-data-dir=C:\\Users\\jan\\AppData",
    "active-url": "https://bank.example/login",
    "user-data-dir": "C:\\Users\\jan",
    "crash-key-gpu": "ANGLE (Intel)",
  };
  const id = await (await handle(await post(form(fields), true), e, NOW)).text();
  const meta = JSON.parse(e.REPORTS.data.get(`reports/2026-10-28/${id}.json`) as string);
  assert.deepEqual(Object.keys(meta.fields).sort(), [...Object.keys(GOOD), "crash-key-gpu"].sort());
});

test("nothing is stored for a refused report", async () => {
  const cases: [string, Request, number][] = [
    ["foreign product", await post(form({ ...GOOD, prod: "Chrome" })), 400],
    ["no version", await post(form({ prod: "Plain_Browser" })), 400],
    ["no dump", await post(form(GOOD, null)), 400],
    ["not a minidump", await post(form(GOOD, new TextEncoder().encode("<html>"))), 400],
    ["brotli", await post(form(GOOD), false, { "Content-Encoding": "br" }), 415],
    ["declared too large", await post(form(GOOD), false, { "Content-Length": String(MAX_BODY_BYTES + 1) }), 413],
  ];
  for (const [what, req, status] of cases) {
    const e = env();
    const res = await handle(req, e, NOW);
    assert.equal(res.status, status, what);
    assert.equal(e.REPORTS.data.size, 0, what);
  }
});

test("only POST /report; rate limit", async () => {
  assert.equal((await handle(new Request("https://crash.plain-browser.app/"), env())).status, 404);
  assert.equal((await handle(new Request("https://crash.plain-browser.app/report"), env())).status, 405);
  const plain = new Request("https://crash.plain-browser.app/report", { method: "POST", body: "x" });
  assert.equal((await handle(plain, env())).status, 400);
  const e = env(true);
  assert.equal((await handle(await post(form(GOOD)), e)).status, 429);
  assert.equal(e.REPORTS.data.size, 0);
});
