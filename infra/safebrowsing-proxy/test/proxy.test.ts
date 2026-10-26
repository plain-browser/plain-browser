// Run: node --test test/   (Node 22.18+ runs TypeScript directly)
import { test } from "node:test";
import assert from "node:assert/strict";
import { handle, type Env } from "../src/index.ts";

type Captured = { url: string; init: RequestInit };

function mockUpstream(status = 200, body = '{"listUpdateResponses":[{"threatType":"MALWARE"}]}') {
  const calls: Captured[] = [];
  globalThis.fetch = (async (url: string, init: RequestInit) => {
    calls.push({ url, init });
    return new Response(body, {
      status,
      headers: { "Content-Type": "application/json", "Set-Cookie": "NID=leak", "X-Google": "1" },
    });
  }) as typeof fetch;
  return calls;
}

const env: Env = { GOOGLE_SB_KEY: "SERVER_KEY" };
const dirty = {
  "CF-Connecting-IP": "203.0.113.7",
  "X-Forwarded-For": "203.0.113.7",
  "X-Real-IP": "203.0.113.7",
  "User-Agent": "Chrome/156 Plain",
  Cookie: "a=b",
  Referer: "https://example.org/",
  "Accept-Language": "pl-PL",
  "CF-Ray": "abc",
  "Content-Type": "application/json",
};

test("threatListUpdates:fetch returns 200 with the update list", async () => {
  mockUpstream();
  const res = await handle(
    new Request("https://sb.example.com/v4/threatListUpdates:fetch", {
      method: "POST",
      headers: dirty,
      body: JSON.stringify({ client: { clientId: "x", clientVersion: "1" }, listUpdateRequests: [] }),
    }),
    env,
  );
  assert.equal(res.status, 200);
  assert.ok((await res.json()).listUpdateResponses.length > 0);
  assert.equal(res.headers.get("Cache-Control"), "no-store");
  assert.equal(res.headers.get("Set-Cookie"), null);
  assert.equal(res.headers.get("Content-Type"), "application/json");
});

test("only Content-Type and a neutral User-Agent reach Google; client key is replaced", async () => {
  const calls = mockUpstream();
  await handle(
    new Request("https://sb.example.com/v4/fullHashes:find?key=CLIENT_KEY&%24req=AAA&%24ct=application%2Fx-protobuf", {
      method: "GET",
      headers: dirty,
    }),
    env,
  );
  const sent = new Headers(calls[0].init.headers);
  assert.deepEqual([...sent.keys()].sort(), ["content-type", "user-agent"]);
  assert.ok(!sent.get("user-agent")!.includes("Chrome"));
  const u = new URL(calls[0].url);
  assert.equal(u.host, "safebrowsing.googleapis.com");
  assert.deepEqual(u.searchParams.getAll("key"), ["SERVER_KEY"]);
  assert.equal(u.searchParams.get("$ct"), "application/x-protobuf");
  assert.ok(!calls[0].url.includes("203.0.113.7") && !calls[0].url.includes("CLIENT_KEY"));
});

test("Chromium's GET + $req envelope is unwrapped into a protobuf POST", async () => {
  const calls = mockUpstream();
  const req = btoa("\x0a\x05hello"); // any bytes; Google parses the protobuf
  const res = await handle(
    new Request(`https://sb.example.com/v4/threatListUpdates:fetch?%24req=${encodeURIComponent(req)}&%24ct=application%2Fx-protobuf&key=dummytoken`, {
      method: "GET",
      headers: { "X-HTTP-Method-Override": "POST", "User-Agent": "HeadlessChrome/156" },
    }),
    env,
  );
  assert.equal(res.status, 200);
  assert.equal(calls[0].init.method, "POST");
  const u = new URL(calls[0].url);
  assert.equal(u.searchParams.get("$req"), null);
  assert.equal(u.searchParams.get("$ct"), "application/x-protobuf");
  assert.deepEqual(u.searchParams.getAll("key"), ["SERVER_KEY"]);
  const sent = new Headers(calls[0].init.headers);
  assert.equal(sent.get("content-type"), "application/x-protobuf");
  assert.equal(sent.get("x-http-method-override"), null);
  assert.equal(new TextDecoder().decode(calls[0].init.body as ArrayBuffer), "\x0a\x05hello");
});

test("a GET with an undecodable $req is rejected without contacting Google", async () => {
  const calls = mockUpstream();
  const res = await handle(new Request("https://sb.example.com/v4/threatListUpdates:fetch?%24req=%25%25%25", { method: "GET" }), env);
  assert.equal(res.status, 400);
  assert.equal(calls.length, 0);
});

test("other paths and methods are rejected without contacting Google", async () => {
  const calls = mockUpstream();
  assert.equal((await handle(new Request("https://sb.example.com/v4/other"), env)).status, 404);
  assert.equal((await handle(new Request("https://sb.example.com/"), env)).status, 404);
  assert.equal(
    (await handle(new Request("https://sb.example.com/v4/threatMatches:find", { method: "DELETE" }), env)).status,
    405,
  );
  assert.equal(calls.length, 0);
});

test("rate limiter blocks with 429", async () => {
  const calls = mockUpstream();
  const limited: Env = { ...env, RATE_LIMITER: { limit: async () => ({ success: false }) } };
  const res = await handle(new Request("https://sb.example.com/v4/threatMatches:find", { method: "POST", body: "{}" }), limited);
  assert.equal(res.status, 429);
  assert.equal(calls.length, 0);
});

test("X-Client filter", async () => {
  mockUpstream();
  const strict: Env = { ...env, REQUIRED_CLIENT: "PlainBrowser" };
  const mk = (h: Record<string, string>) =>
    new Request("https://sb.example.com/v4/threatMatches:find", { method: "POST", body: "{}", headers: h });
  assert.equal((await handle(mk({}), strict)).status, 403);
  assert.equal((await handle(mk({ "X-Client": "Other/1.0" }), strict)).status, 403);
  assert.equal((await handle(mk({ "X-Client": "PlainBrowser/0.1.0" }), strict)).status, 200);
});

test("upstream status is passed through; failures become 502", async () => {
  mockUpstream(400, '{"error":{"code":400}}');
  const bad = await handle(new Request("https://sb.example.com/v4/threatMatches:find", { method: "POST", body: "{}" }), env);
  assert.equal(bad.status, 400);
  globalThis.fetch = (async () => { throw new Error("down"); }) as typeof fetch;
  const down = await handle(new Request("https://sb.example.com/v4/threatMatches:find", { method: "POST", body: "{}" }), env);
  assert.equal(down.status, 502);
});
