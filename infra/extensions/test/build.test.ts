// Run: node --test test/   (Node 22.18+ runs TypeScript directly)
import { test } from "node:test";
import assert from "node:assert/strict";
import { createHash, createPublicKey, generateKeyPairSync } from "node:crypto";
import { extensionId, packCrx3, verifyCrx3 } from "../scripts/crx3.mjs";
import { readZip, unsafeName, writeZip } from "../scripts/zip.mjs";
import {
  compareVersions,
  injectUpdateUrl,
  permissionsOf,
  repack,
  SOURCES,
  UPDATE_URL,
  updatesXml,
  validVersion,
  xmlAttr,
} from "../scripts/extensions.mjs";
import { SLUGS } from "../src/index.ts";
import { readPins } from "../scripts/upstream.mjs";

const { privateKey } = generateKeyPairSync("rsa", { modulusLength: 2048 });
const other = generateKeyPairSync("rsa", { modulusLength: 2048 }).privateKey;
const sha = (b: Buffer) => createHash("sha256").update(b).digest("hex");

const manifest = {
  manifest_version: 2,
  name: "__MSG_extName__",
  default_locale: "en",
  version: "1.76.0",
  key: "MIIBIjAN...",
  background: { page: "background.html" },
};
const files: Record<string, string | Buffer> = {
  "manifest.json": JSON.stringify(manifest),
  "_locales/en/messages.json": JSON.stringify({ extName: { message: "uBlock Origin" } }),
  "js/background.js": "console.log('x');\n".repeat(200),
  "img/icon.png": Buffer.from([0x89, 0x50, 0x4e, 0x47, 0, 1, 2, 3, 255]),
  "empty.txt": "",
  "ünïcode.txt": "ü",
};
const asEntries = (prefix = "") => [
  ...(prefix ? [{ name: prefix }] : []),
  ...Object.entries(files).map(([name, data]) => ({ name: prefix + name, data: Buffer.from(data), time: 0x6000, date: 0x5a21 })),
];

test("extension ID: Chromium's mapping of SHA-256 to a-p", () => {
  // crx_file::id_util::GenerateId("test") in Chromium's unit tests.
  assert.equal(extensionId(Buffer.from("test")), "jpignaibiiemhngfjkcpokkamffknabf");
  // The key of gorhill's own uBlock0_1.76.0.chromium.crx (packed by Chrome), whose crx_id is this ID.
  const ublockKey = Buffer.from(
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsgdkJHEX8xHAytYy3Rihqn5FoU/cbPKhoorkCCsgF8HR2y2OSWGM1Ojrmnr0ebgM9WA1pl1hr1Cm" +
      "OH7DjgQVKRhzBjK7/Zb6RVJNPVGEQvV9CdCUwOTKsu1qQGRbjm9Z/DkYxgu6B2sLo0ZpQ/IBsmBvs+FGR4CqrWra8GZPwn7n3FibeoxcArWiAx85N2Oy" +
      "iaef2Geytoog4hS+I5Fs3ymKkEeTYM3tzeC0U5nZ010LCnlQe0cQ3UDOro8VzLosuhaxAsrPFErIOfIUfvV3sNhQJrySqgii9Xv6RWT8TI3pHL1yjevKK" +
      "TxNb2VbPlTOi5MyzPowWV8hHJEOkwq2dQIDAQAB",
    "base64",
  );
  assert.equal(extensionId(ublockKey), "fkgkibajhfbepljeaefdnfnegdcjomkh");
});

test("CRX3 round trip: header, signature, ID", () => {
  const zip = writeZip(asEntries());
  const crx = packCrx3(zip, privateKey);
  assert.equal(crx.subarray(0, 4).toString(), "Cr24");
  assert.equal(crx.readUInt32LE(4), 3);
  const spki = createPublicKey(privateKey).export({ type: "spki", format: "der" });
  const r = verifyCrx3(crx);
  assert.equal(r.id, extensionId(spki));
  assert.deepEqual(r.publicKey, spki);
  assert.deepEqual(r.zip, zip);
  // RSASSA-PKCS1-v1_5 is deterministic: the same zip and key give the same CRX.
  assert.deepEqual(packCrx3(zip, privateKey), crx);
  // The PEM text works as well as a KeyObject.
  assert.deepEqual(packCrx3(zip, privateKey.export({ type: "pkcs8", format: "pem" }) as string), crx);
});

test("verifyCrx3 refuses tampering", () => {
  const zip = writeZip(asEntries());
  const crx = packCrx3(zip, privateKey);
  const flipped = Buffer.from(crx);
  flipped[flipped.length - 30] ^= 1; // in the zip
  assert.throws(() => verifyCrx3(flipped), /bad signature/);
  const swapped = Buffer.concat([crx.subarray(0, crx.length - zip.length), writeZip(asEntries().slice(1))]);
  assert.throws(() => verifyCrx3(swapped), /bad signature/);
  assert.throws(() => verifyCrx3(Buffer.from("PK\x03\x04")), /magic/);
  const v2 = Buffer.from(crx);
  v2.writeUInt32LE(2, 4);
  assert.throws(() => verifyCrx3(v2), /version 2/);
  // A header from one key with the signature of another: crx_id and signature no longer agree.
  const a = packCrx3(zip, privateKey);
  const b = packCrx3(zip, other);
  const hdrA = a.subarray(12, 12 + a.readUInt32LE(8));
  const hdrB = b.subarray(12, 12 + b.readUInt32LE(8));
  // Same layout, so swap only signed_header_data (the tail field, 16-byte crx_id).
  const mixed = Buffer.concat([a.subarray(0, 12), hdrA.subarray(0, hdrA.length - 18), hdrB.subarray(hdrB.length - 18), zip]);
  assert.throws(() => verifyCrx3(mixed), /bad signature|matches no key/);
});

test("zip: write then read keeps names, contents and times", () => {
  const entries = readZip(writeZip(asEntries()));
  assert.deepEqual(
    entries.map((e) => [e.name, e.data.toString("hex"), e.time, e.date]),
    Object.entries(files).map(([n, d]) => [n, Buffer.from(d).toString("hex"), 0x6000, 0x5a21]),
  );
});

test("zip: unsafe names are refused on read and write", () => {
  for (const name of ["/etc/passwd", "../x", "a/../../x", "C:/x", "a\\b", ""]) {
    assert.ok(unsafeName(name), name);
    assert.throws(() => writeZip([{ name, data: Buffer.from("x") }]), /refusing/);
  }
  assert.equal(unsafeName("a/..b/c..d"), null);
  // A zip written by someone else with a bad name (patched after writing, same length).
  const zip = writeZip([{ name: "aa/x", data: Buffer.from("x") }]);
  const evil = Buffer.from(zip.toString("latin1").replaceAll("aa/x", "../x"), "latin1");
  assert.throws(() => readZip(evil), /refusing.*'\.\.'/);
  const corrupt = Buffer.from(writeZip([{ name: "a.txt", data: Buffer.from("hello hello hello hello") }]));
  corrupt[30 + 5 + 2] ^= 0xff; // a byte of the compressed data
  assert.throws(() => readZip(corrupt));
});

test("repack: update_url in manifest.json, every other entry unchanged", () => {
  const { zip, manifest: m, name } = repack(writeZip(asEntries()));
  assert.equal(name, "uBlock Origin");
  assert.equal(m.version, "1.76.0");
  const out = readZip(zip);
  assert.deepEqual(out.map((e) => e.name), Object.keys(files));
  for (const e of out) {
    if (e.name === "manifest.json") continue;
    assert.equal(sha(e.data), sha(Buffer.from(files[e.name])), e.name);
  }
  const served = JSON.parse(out[0].data.toString());
  const { key: _key, ...rest } = manifest;
  assert.deepEqual(served, { ...rest, update_url: UPDATE_URL });
  assert.equal(served.key, undefined);
});

test("repack: manifest one directory down moves to the root", () => {
  const { zip } = repack(writeZip(asEntries("uBlock0.chromium/")));
  assert.deepEqual(readZip(zip).map((e) => e.name), Object.keys(files));
  // A file outside that directory is refused, as is a zip with no manifest at all.
  assert.throws(() => repack(writeZip([...asEntries("d/"), { name: "stray.js", data: Buffer.from("x") }])), /outside/);
  assert.throws(() => repack(writeZip([{ name: "a/b/manifest.json", data: Buffer.from("{}") }])), /no manifest/);
});

test("manifest checks", () => {
  const bad = (m: unknown) => () => repack(writeZip([{ name: "manifest.json", data: Buffer.from(JSON.stringify(m)) }]));
  assert.throws(bad({ name: "x", version: "1.0", manifest_version: 1 }), /manifest_version/);
  assert.throws(bad({ name: "x", version: "1.0beta", manifest_version: 3 }), /version/);
  assert.throws(bad({ version: "1.0", manifest_version: 3 }), /name/);
  assert.throws(() => repack(writeZip([{ name: "manifest.json", data: Buffer.from("{nope") }])), /not JSON/);
  // A BOM is fine; update_url replaces an upstream one.
  const { zip } = repack(
    writeZip([{ name: "manifest.json", data: Buffer.from('\uFEFF{"name":"n","version":"13.6.38","manifest_version":3,"update_url":"https://x/"}') }]),
  );
  assert.equal(JSON.parse(readZip(zip)[0].data.toString()).update_url, UPDATE_URL);
  assert.deepEqual(JSON.parse(injectUpdateUrl({ a: 1 }, "u").toString()), { a: 1, update_url: "u" });
});

test("versions", () => {
  for (const v of ["1", "1.76.0", "5.5.3.3", "65535.0"]) assert.ok(validVersion(v), v);
  for (const v of ["", "1.", "01.2", "1.2.3.4.5", "65536", "1.0b1", "v1.0"]) assert.ok(!validVersion(v), v);
  assert.equal(compareVersions("1.76.0", "1.75.9"), 1);
  assert.equal(compareVersions("1.4.4", "1.4.4.0"), 0);
  assert.equal(compareVersions("5.5.3.3", "5.5.10"), -1);
});

test("updates.xml: gupdate format, attributes escaped", () => {
  const xml = updatesXml(
    {
      ublock: { id: "abcdefghijklmnopabcdefghijklmnop", version: "1.76.0", sha256: "00ff", size: 123 },
      noscript: { id: "x'<&\"y", version: "13.6.38", sha256: "aa", size: 1 },
    },
    "https://ext.example/v1/",
  );
  assert.equal(
    xml,
    "<?xml version='1.0' encoding='UTF-8'?><gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>" +
      "<app appid='abcdefghijklmnopabcdefghijklmnop'><updatecheck codebase='https://ext.example/v1/crx/ublock-1.76.0.crx' " +
      "version='1.76.0' hash_sha256='00ff' size='123'/></app>" +
      "<app appid='x&apos;&lt;&amp;&quot;y'><updatecheck codebase='https://ext.example/v1/crx/noscript-13.6.38.crx' " +
      "version='13.6.38' hash_sha256='aa' size='1'/></app></gupdate>\n",
  );
  assert.equal(xmlAttr("a>b"), "a&gt;b");
});

test("the Worker serves exactly the extensions the build makes", () => {
  assert.deepEqual(SLUGS, Object.keys(SOURCES));
});

test("permissionsOf lists API, host and content-script permissions, sorted and without duplicates", () => {
  assert.deepEqual(
    permissionsOf({
      permissions: ["tabs", "<all_urls>", "tabs", 7],
      optional_permissions: ["downloads"],
      host_permissions: ["https://*/*"],
      optional_host_permissions: ["file:///*"],
      content_scripts: [{ matches: ["https://example.com/*"] }, { js: ["a.js"] }, null],
    }),
    ["<all_urls>", "content_script:https://example.com/*", "https://*/*", "optional:downloads", "optional:file:///*", "tabs"],
  );
  assert.deepEqual(permissionsOf({}), []);
});

test("pins.json pins every extension the build makes: tag, version, sha256 of the zip, permissions", () => {
  const pins = readPins();
  assert.deepEqual(Object.keys(pins), Object.keys(SOURCES));
  for (const [slug, pin] of Object.entries(pins) as [string, any][]) {
    assert.deepEqual(Object.keys(pin), ["tag", "version", "sha256", "permissions"], slug);
    assert.ok(typeof pin.tag === "string" && pin.tag, slug);
    assert.ok(validVersion(pin.version), slug);
    assert.match(pin.sha256, /^[0-9a-f]{64}$/, slug);
    assert.deepEqual(pin.permissions, [...new Set(pin.permissions as string[])].sort(), slug);
  }
});
