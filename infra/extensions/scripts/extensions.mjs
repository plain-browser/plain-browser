// Copyright Plain Browser contributors. MPL-2.0.
// The curated extensions and the steps between an upstream release zip and what we serve: manifest checks, the
// update_url, updates.xml and catalog.json. Used by build.mjs and the tests. No dependencies.
import { readZip, writeZip } from "./zip.mjs";

export const BASE_URL = "https://ext.plain-browser.app/v1/";
export const UPDATE_URL = `${BASE_URL}updates.xml`;

// slug -> upstream. `asset` matches the release asset; the browser's catalogue uses the same slugs. Keep in sync
// with SLUGS in src/index.ts (a test checks).
export const SOURCES = {
  ublock: { repo: "gorhill/uBlock", asset: /^uBlock0_[\w.]+\.chromium\.zip$/, license: "GPL-3.0" },
  // Archived upstream since 2021; 1.4.4 is the last release.
  umatrix: { repo: "gorhill/uMatrix", asset: /^uMatrix_[\w.]+\.chromium\.zip$/, license: "GPL-3.0" },
  noscript: { repo: "hackademix/noscript", asset: /^noscript-[\w.]+-chrome\.zip$/, license: "GPL-3.0" },
  adguard: { repo: "AdguardTeam/AdguardBrowserExtension", asset: /^chrome\.zip$/, license: "GPL-3.0" },
};

// Chromium's version format: 1 to 4 dot-separated integers, each 0-65535, no leading zeros.
export function validVersion(v) {
  return typeof v === "string" && /^(0|[1-9]\d{0,4})(\.(0|[1-9]\d{0,4})){0,3}$/.test(v) && v.split(".").every((n) => +n <= 65535);
}

// -1, 0 or 1, comparing Chromium versions (missing parts are 0).
export function compareVersions(a, b) {
  const x = a.split(".").map(Number);
  const y = b.split(".").map(Number);
  for (let i = 0; i < 4; i++) {
    const d = (x[i] ?? 0) - (y[i] ?? 0);
    if (d) return Math.sign(d);
  }
  return 0;
}

export function parseManifest(data) {
  const text = data.toString("utf8").replace(/^﻿/, "");
  let m;
  try {
    m = JSON.parse(text);
  } catch (e) {
    throw new Error(`manifest.json is not JSON: ${e.message}`);
  }
  if (!m || typeof m !== "object" || Array.isArray(m)) throw new Error("manifest.json is not an object");
  if (typeof m.name !== "string" || !m.name) throw new Error("manifest has no name");
  if (!validVersion(m.version)) throw new Error(`manifest version ${JSON.stringify(m.version)} is not a Chromium version`);
  if (m.manifest_version !== 2 && m.manifest_version !== 3) {
    throw new Error(`manifest_version ${JSON.stringify(m.manifest_version)}`);
  }
  return m;
}

// The manifest with our update_url. A `key` is dropped: the CRX key decides the ID, and Chromium would refuse a
// manifest key that differs from it. Everything else stays as it was (re-serialized).
export function injectUpdateUrl(manifest, updateUrl = UPDATE_URL) {
  const out = { ...manifest, update_url: updateUrl };
  delete out.key;
  return Buffer.from(JSON.stringify(out, null, 2) + "\n");
}

// "__MSG_x__" names resolve through _locales/<default_locale>/messages.json, as Chromium shows them.
function displayName(manifest, byName) {
  const m = /^__MSG_(\w+)__$/.exec(manifest.name);
  if (!m || !manifest.default_locale) return manifest.name;
  const file = byName.get(`_locales/${manifest.default_locale}/messages.json`);
  if (!file) return manifest.name;
  try {
    const messages = JSON.parse(file.toString("utf8").replace(/^﻿/, ""));
    const key = Object.keys(messages).find((k) => k.toLowerCase() === m[1].toLowerCase());
    return (key && messages[key]?.message) || manifest.name;
  } catch {
    return manifest.name;
  }
}

// Upstream zip -> {zip, manifest, name}: manifest.json at the root (moved up when the zip has it one directory
// down), update_url added, every other entry unchanged.
export function repack(upstreamZip, updateUrl = UPDATE_URL) {
  const entries = readZip(upstreamZip);
  let prefix = "";
  if (!entries.some((e) => e.name === "manifest.json")) {
    const nested = entries.filter((e) => /^[^/]+\/manifest\.json$/.test(e.name));
    if (nested.length !== 1) throw new Error("no manifest.json at the root or one directory down");
    prefix = nested[0].name.slice(0, -"manifest.json".length);
  }
  const files = [];
  for (const e of entries) {
    if (!e.name.startsWith(prefix)) {
      throw new Error(`${e.name} lies outside ${prefix}, the directory with manifest.json`);
    }
    const name = e.name.slice(prefix.length);
    if (name === "") continue; // the wrapping directory itself
    files.push({ ...e, name });
  }
  const byName = new Map(files.map((e) => [e.name, e.data]));
  const manifest = parseManifest(byName.get("manifest.json"));
  const out = files.map((e) => (e.name === "manifest.json" ? { ...e, data: injectUpdateUrl(manifest, updateUrl) } : e));
  return { zip: writeZip(out), manifest, name: displayName(manifest, byName) };
}

export function xmlAttr(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&apos;" })[c]);
}

export function crxFileName(slug, version) {
  return `${slug}-${version}.crx`;
}

// gupdate (protocol 2.0) response for Chromium's extension updater, one <app> per catalog entry.
export function updatesXml(catalog, baseUrl = BASE_URL) {
  const apps = Object.entries(catalog)
    .map(
      ([slug, e]) =>
        `<app appid='${xmlAttr(e.id)}'><updatecheck codebase='${xmlAttr(`${baseUrl}crx/${crxFileName(slug, e.version)}`)}'` +
        ` version='${xmlAttr(e.version)}' hash_sha256='${xmlAttr(e.sha256)}' size='${xmlAttr(e.size)}'/></app>`,
    )
    .join("");
  return (
    "<?xml version='1.0' encoding='UTF-8'?>" +
    `<gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>${apps}</gupdate>\n`
  );
}
