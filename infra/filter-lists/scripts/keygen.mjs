#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Makes the Ed25519 key pair that signs the filter list manifest.
//
//   node scripts/keygen.mjs <private-key-file>
//
// Writes the private key (base64 of its PKCS#8 DER, what the Worker's SIGNING_KEY secret takes) to
// <private-key-file>, readable only by you, and prints the public key for the browser. Then:
//   1. npx wrangler secret put SIGNING_KEY < <private-key-file>
//   2. put the printed public key into kListSigningKeys in src/plainbrowser/adblock/browser/filter_lists.h
//   3. keep <private-key-file> offline (a password manager, say): it is needed only to re-upload the secret.
//      Do not commit it. Losing it means a new key and a browser release with the new public key.
import { generateKeyPairSync } from "node:crypto";
import { existsSync, writeFileSync } from "node:fs";

const out = process.argv[2];
if (!out) {
  console.error("usage: node scripts/keygen.mjs <private-key-file>");
  process.exit(2);
}
if (existsSync(out)) {
  console.error(`${out} exists; not overwriting a key`);
  process.exit(1);
}
const { publicKey, privateKey } = generateKeyPairSync("ed25519");
const pkcs8 = privateKey.export({ type: "pkcs8", format: "der" }).toString("base64");
// SPKI DER of an Ed25519 key is a fixed 12-byte header followed by the 32 raw key bytes.
const raw = publicKey.export({ type: "spki", format: "der" }).subarray(12).toString("base64");
writeFileSync(out, pkcs8 + "\n", { mode: 0o600, flag: "wx" });
console.log(`private key written to ${out} (mode 600)`);
console.log(`public key for kListSigningKeys: ${raw}`);
console.log(`next: npx wrangler secret put SIGNING_KEY < ${out}`);
