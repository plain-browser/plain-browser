#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Checks what lists.plain-browser.app serves the way the browser does: the manifest's Ed25519 signature against the
// public key compiled into the browser (filter_lists.h, kListSigningKeys), the freshness fields (sequence, expires) and
// the lists it names (size and SHA-256 of each file). Exit status 1 when anything is wrong.
//
//   node scripts/verify-live.mjs [--server https://lists.plain-browser.app/v1/] [--key <base64 raw public key>]
import { createHash, createPublicKey, verify } from "node:crypto";
import { readFileSync } from "node:fs";
import { parseArgs } from "node:util";

const { values: args } = parseArgs({ options: { server: { type: "string", default: "https://lists.plain-browser.app/v1/" }, key: { type: "string" } } });
const header = readFileSync(new URL("../../../src/plainbrowser/adblock/browser/filter_lists.h", import.meta.url), "utf8");
const keys = args.key ? [args.key] : [...header.matchAll(/^\s*"([A-Za-z0-9+/]{43}=)",/gm)].map((m) => m[1]);
if (keys.length === 0) throw new Error("no public key found in filter_lists.h; pass --key");

const get = async (path) => {
  const res = await fetch(new URL(path, args.server), { headers: { "X-Client": "PlainBrowser/0.2.2" } });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${path}`);
  return res;
};
const SPKI_ED25519 = Buffer.from("302a300506032b6570032100", "hex");
let bad = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) bad++; };

const manifestText = await (await get("manifest.json")).text();
const signature = Buffer.from((await (await get("manifest.json.sig")).text()).trim(), "base64");
const signed = keys.some((k) => verify(null, Buffer.from(manifestText), createPublicKey({ key: Buffer.concat([SPKI_ED25519, Buffer.from(k, "base64")]), format: "der", type: "spki" }), signature));
check(signed, "the signature verifies against a key compiled into the browser");

const m = JSON.parse(manifestText);
const now = Math.floor(Date.now() / 1000);
check(m.format === 1, "format 1");
check(Number.isInteger(m.sequence) && m.sequence >= 1, `sequence ${m.sequence} (integer >= 1)`);
check(Number.isInteger(m.expires) && m.expires > now, `expires ${m.expires} is in the future (${Number.isInteger(m.expires) ? ((m.expires - now) / 86400).toFixed(1) : "?"} days)`);
for (const [name, e] of Object.entries(m.lists)) {
  const body = Buffer.from(await (await get(`lists/${name}.txt`)).arrayBuffer());
  check(body.length === e.size && createHash("sha256").update(body).digest("hex") === e.sha256, `${name}: ${body.length} bytes, size and SHA-256 match the manifest`);
}
console.log(`lists: ${Object.keys(m.lists).join(", ")}; generated ${m.generated}`);
process.exit(bad ? 1 : 0);
