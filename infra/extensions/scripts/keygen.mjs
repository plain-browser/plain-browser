#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Makes the RSA-2048 key that signs one extension's CRX. The extension ID is derived from it.
//
//   node scripts/keygen.mjs <slug> <out.pem>
//
// Writes the private key (PKCS#8 PEM) to <out.pem>, readable only by you, and prints the extension ID for the
// browser. Then put the PEM into the GitHub secret EXT_KEY_<SLUG> and keep the file offline. Do not commit it.
// Losing it means a new ID: users have to reinstall the extension (see README.md).
import { generateKeyPairSync } from "node:crypto";
import { existsSync, writeFileSync } from "node:fs";
import { extensionId } from "./crx3.mjs";
import { SOURCES } from "./extensions.mjs";

const [slug, out] = process.argv.slice(2);
if (!slug || !out) {
  console.error("usage: node scripts/keygen.mjs <slug> <out.pem>");
  process.exit(2);
}
if (!(slug in SOURCES)) {
  console.error(`unknown slug ${slug}; one of ${Object.keys(SOURCES).join(", ")}`);
  process.exit(2);
}
if (existsSync(out)) {
  console.error(`${out} exists; not overwriting a key`);
  process.exit(1);
}
const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048, publicExponent: 0x10001 });
writeFileSync(out, privateKey.export({ type: "pkcs8", format: "pem" }), { mode: 0o600, flag: "wx" });
console.log(`private key written to ${out} (mode 600)`);
console.log(`${slug} extension ID: ${extensionId(publicKey.export({ type: "spki", format: "der" }))}`);
console.log(`next: gh secret set EXT_KEY_${slug.toUpperCase()} < ${out}`);
