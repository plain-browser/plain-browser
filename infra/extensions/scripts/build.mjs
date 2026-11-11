#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Builds what ext.plain-browser.app serves: for each extension in SOURCES, the upstream release zip that pins.json
// names (tag, version, sha256, permissions; a zip that differs in any of them is refused), repacked with our
// update_url and signed as a CRX3 with that extension's key. scripts/propose.mjs moves the pins.
//
//   node scripts/build.mjs [--keys-dir DIR] [--out DIR] [--previous URL|FILE|none] [--only slug,...]
//
// Keys: env EXT_KEY_<SLUG> (PEM text), else <keys-dir>/<slug>.pem. A missing key stops the build before any
// download. --previous is the published catalog.json (default: the live one); an extension whose download or
// checks fail keeps its previous entry, so updates.xml goes on naming the file already published.
//
// Writes <out>/crx/<slug>-<version>.crx, <out>/crx/<slug>.crx (the same bytes), <out>/catalog.json and
// <out>/updates.xml. Exit status 1 when a key is missing or nothing could be built or kept.
import { createPrivateKey, createPublicKey } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { parseArgs } from "node:util";
import { extensionId, packCrx3, verifyCrx3 } from "./crx3.mjs";
import { BASE_URL, compareVersions, crxFileName, permissionsOf, repack, SOURCES, updatesXml } from "./extensions.mjs";
import { fetchUpstream, readPins, sha256, USER_AGENT } from "./upstream.mjs";

const PINS = readPins();

const { values: args } = parseArgs({
  options: {
    "keys-dir": { type: "string" },
    out: { type: "string", default: "dist" },
    previous: { type: "string", default: `${BASE_URL}catalog.json` },
    only: { type: "string" },
  },
});
const slugs = args.only ? args.only.split(",") : Object.keys(SOURCES);
for (const s of slugs) if (!(s in SOURCES)) throw new Error(`unknown slug ${s}`);

// 1. Keys, before anything is downloaded.
const keys = {};
const missing = [];
for (const slug of slugs) {
  const env = process.env[`EXT_KEY_${slug.toUpperCase()}`];
  const file = args["keys-dir"] && join(args["keys-dir"], `${slug}.pem`);
  const pem = env?.trim() ? env : file && existsSync(file) ? readFileSync(file, "utf8") : null;
  if (!pem) {
    missing.push(`EXT_KEY_${slug.toUpperCase()}${file ? ` or ${file}` : ""}`);
    continue;
  }
  const key = createPrivateKey(pem);
  if (key.asymmetricKeyType !== "rsa") throw new Error(`${slug}: the key is not RSA`);
  keys[slug] = { key, id: extensionId(createPublicKey(key).export({ type: "spki", format: "der" })) };
}
if (missing.length) {
  console.error(`missing signing key(s): ${missing.join(", ")}`);
  process.exit(1);
}

// 2. The published catalog, to keep entries that fail now and to catch a swapped key.
// "none", or a 404 from a server with nothing published yet, means a first build. Any other failure stops the
// build: without the published catalog a swapped key would go unnoticed.
async function loadPrevious(where) {
  if (where === "none") return {};
  try {
    if (!/^https?:/.test(where)) return JSON.parse(readFileSync(where, "utf8"));
    const res = await fetch(where, { headers: { "User-Agent": USER_AGENT } });
    if (res.status === 404) {
      console.warn(`no previous catalog at ${where} (404): first build`);
      return {};
    }
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (e) {
    console.error(`cannot read the previous catalog ${where}: ${e.message} (--previous none for a first build)`);
    process.exit(1);
  }
}

const previous = await loadPrevious(args.previous);
for (const slug of slugs) {
  const old = previous[slug]?.id;
  if (old && old !== keys[slug].id) {
    // A different key means a different extension ID: every installed copy would stop getting updates.
    console.error(`${slug}: key gives ID ${keys[slug].id}, the published one is ${old}. Wrong key? ` +
      "If the change is intended, build with --previous none (users must reinstall).");
    process.exit(1);
  }
}

// 3. Each extension.
async function build(slug) {
  const src = SOURCES[slug];
  const pin = PINS[slug];
  if (!pin) throw new Error("no entry in pins.json");
  const { release, asset, bytes: upstream } = await fetchUpstream(src, pin.tag);
  const digest = sha256(upstream);
  if (digest !== pin.sha256) throw new Error(`${asset.name} of ${pin.tag} has sha256 ${digest}, pins.json has ${pin.sha256}`);

  const { zip, manifest, name } = repack(upstream);
  if (manifest.version !== pin.version) throw new Error(`manifest version ${manifest.version}, pins.json has ${pin.version}`);
  // Follows from the sha256 unless pins.json was edited by hand: the list a reviewer saw is the list that ships.
  if (JSON.stringify(permissionsOf(manifest)) !== JSON.stringify(pin.permissions)) {
    throw new Error("the manifest's permissions differ from pins.json (run scripts/propose.mjs)");
  }
  const notes = [];
  if (manifest.key) notes.push("dropped the upstream manifest key");
  const prev = previous[slug];
  if (prev && compareVersions(manifest.version, prev.version) < 0) {
    throw new Error(`pinned ${manifest.version} is older than published ${prev.version}`);
  }

  const crx = packCrx3(zip, keys[slug].key);
  const checked = verifyCrx3(crx);
  if (checked.id !== keys[slug].id) throw new Error("signed CRX has the wrong ID");
  mkdirSync(join(args.out, "crx"), { recursive: true });
  writeFileSync(join(args.out, "crx", crxFileName(slug, manifest.version)), crx);
  writeFileSync(join(args.out, "crx", `${slug}.crx`), crx);
  return {
    entry: {
      id: keys[slug].id,
      name,
      version: manifest.version,
      manifest_version: manifest.manifest_version,
      sha256: sha256(crx),
      size: crx.length,
      source_url: release.html_url,
      license: src.license,
    },
    notes,
  };
}

const catalog = {};
const report = [];
for (const slug of Object.keys(SOURCES)) {
  if (!slugs.includes(slug)) {
    if (previous[slug]) catalog[slug] = previous[slug];
    continue;
  }
  try {
    const { entry, notes } = await build(slug);
    catalog[slug] = entry;
    report.push(`${slug}: built ${entry.version} (MV${entry.manifest_version}, ${entry.size} bytes, ${entry.id})` +
      (notes.length ? `; ${notes.join("; ")}` : ""));
  } catch (e) {
    if (previous[slug]) {
      catalog[slug] = previous[slug];
      report.push(`${slug}: FAILED, kept published ${previous[slug].version}: ${e.message}`);
    } else {
      report.push(`${slug}: FAILED, nothing published: ${e.message}`);
    }
  }
}

console.log(report.join("\n"));
// In GitHub Actions a failure also shows on the run's summary page.
if (process.env.GITHUB_ACTIONS) for (const line of report) if (line.includes("FAILED")) console.log(`::warning::${line}`);
if (Object.keys(catalog).length === 0) {
  console.error("nothing to publish");
  process.exit(1);
}
mkdirSync(args.out, { recursive: true });
writeFileSync(join(args.out, "catalog.json"), JSON.stringify(catalog, null, 2) + "\n");
writeFileSync(join(args.out, "updates.xml"), updatesXml(catalog));
console.log(`wrote ${args.out}/catalog.json and ${args.out}/updates.xml`);
