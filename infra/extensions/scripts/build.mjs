#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Builds what ext.plain-browser.app serves: for each extension in SOURCES, the latest upstream release zip,
// repacked with our update_url and signed as a CRX3 with that extension's key.
//
//   node scripts/build.mjs [--keys-dir DIR] [--out DIR] [--previous URL|FILE|none] [--only slug,...]
//
// Keys: env EXT_KEY_<SLUG> (PEM text), else <keys-dir>/<slug>.pem. A missing key stops the build before any
// download. --previous is the published catalog.json (default: the live one); an extension whose download or
// checks fail keeps its previous entry, so updates.xml goes on naming the file already published.
//
// Writes <out>/crx/<slug>-<version>.crx, <out>/crx/<slug>.crx (the same bytes), <out>/catalog.json and
// <out>/updates.xml. Exit status 1 when a key is missing or nothing could be built or kept.
import { createHash, createPrivateKey, createPublicKey } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { parseArgs } from "node:util";
import { extensionId, packCrx3, verifyCrx3 } from "./crx3.mjs";
import { BASE_URL, compareVersions, crxFileName, repack, SOURCES, updatesXml } from "./extensions.mjs";

const MAX_ASSET = 64 * 1024 * 1024;
const USER_AGENT = "PlainBrowserExtensionBuild/1 (+https://plain-browser.app)";

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

async function fetchOk(url, headers = {}) {
  const res = await fetch(url, { headers: { "User-Agent": USER_AGENT, ...headers }, redirect: "follow" });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${url}`);
  return res;
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
const github = { Accept: "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28" };
if (process.env.GITHUB_TOKEN) github.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;

async function build(slug) {
  const src = SOURCES[slug];
  const release = await fetchOk(`https://api.github.com/repos/${src.repo}/releases/latest`, github).then((r) => r.json());
  if (release.draft || release.prerelease) throw new Error(`latest release ${release.tag_name} is a pre-release`);
  const asset = release.assets?.find((a) => src.asset.test(a.name));
  if (!asset) throw new Error(`release ${release.tag_name} has no asset matching ${src.asset}`);
  if (asset.size > MAX_ASSET) throw new Error(`${asset.name} is ${asset.size} bytes`);
  const upstream = Buffer.from(await fetchOk(asset.browser_download_url).then((r) => r.arrayBuffer()));
  if (upstream.length > MAX_ASSET) throw new Error(`${asset.name} is ${upstream.length} bytes`);

  const { zip, manifest, name } = repack(upstream);
  const notes = [];
  if (manifest.version !== release.tag_name.replace(/^v/, "")) {
    notes.push(`manifest version ${manifest.version}, tag ${release.tag_name}`);
  }
  if (manifest.key) notes.push("dropped the upstream manifest key");
  const prev = previous[slug];
  if (prev && compareVersions(manifest.version, prev.version) < 0) {
    throw new Error(`upstream ${manifest.version} is older than published ${prev.version}`);
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
      sha256: createHash("sha256").update(crx).digest("hex"),
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
