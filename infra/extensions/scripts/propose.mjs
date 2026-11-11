#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Moves pins.json to the latest upstream release of each extension and prints what changed, as Markdown, for the
// pull request the weekly workflow opens. Nothing is signed or published here: build.mjs builds what pins.json
// names, after a person has merged the change.
//
//   node scripts/propose.mjs [--only slug,...] [--summary FILE]
//
// An extension whose release cannot be fetched or does not pass the manifest checks keeps its pin. Exit status 0
// whether or not pins.json changed (the caller looks at the file), 1 when nothing could be checked at all.
import { writeFileSync } from "node:fs";
import { parseArgs } from "node:util";
import { compareVersions, permissionsOf, repack, SOURCES } from "./extensions.mjs";
import { fetchUpstream, PINS_FILE, readPins, sha256 } from "./upstream.mjs";

const { values: args } = parseArgs({ options: { only: { type: "string" }, summary: { type: "string" } } });
const slugs = args.only ? args.only.split(",") : Object.keys(SOURCES);
for (const s of slugs) if (!(s in SOURCES)) throw new Error(`unknown slug ${s}`);

const pins = readPins();
const lines = [];
let failed = 0;
for (const slug of slugs) {
  const old = pins[slug];
  try {
    const { release, asset, bytes } = await fetchUpstream(SOURCES[slug]);
    const { manifest } = repack(bytes);
    const pin = { tag: release.tag_name, version: manifest.version, sha256: sha256(bytes), permissions: permissionsOf(manifest) };
    if (old && JSON.stringify(old) === JSON.stringify(pin)) {
      lines.push(`- **${slug}**: unchanged (${pin.version})`);
      continue;
    }
    if (old && compareVersions(pin.version, old.version) < 0) {
      throw new Error(`latest release ${pin.tag} has version ${pin.version}, older than the pinned ${old.version}`);
    }
    pins[slug] = pin;
    const url = `https://github.com/${SOURCES[slug].repo}`;
    lines.push(`- **${slug}**: ${old ? `${old.version} → ` : "new, "}${pin.version} ([release ${pin.tag}](${release.html_url})` +
      (old && old.tag !== pin.tag ? `, [source diff](${url}/compare/${encodeURIComponent(old.tag)}...${encodeURIComponent(pin.tag)})` : "") +
      `), \`${asset.name}\` sha256 \`${pin.sha256}\``);
    if (old && old.tag === pin.tag) lines.push(`  - **The asset of the same release changed** (was sha256 \`${old.sha256}\`). Ask upstream why before merging.`);
    const before = new Set(old?.permissions ?? []);
    const added = pin.permissions.filter((p) => !before.has(p));
    const removed = [...before].filter((p) => !pin.permissions.includes(p));
    if (old && added.length) lines.push(`  - **New permissions: ${added.map((p) => `\`${p}\``).join(", ")}**`);
    if (removed.length) lines.push(`  - Permissions dropped: ${removed.map((p) => `\`${p}\``).join(", ")}`);
  } catch (e) {
    failed++;
    lines.push(`- **${slug}**: not checked, pin kept${old ? ` (${old.version})` : ""}: ${e.message}`);
  }
}

writeFileSync(PINS_FILE, JSON.stringify(pins, null, 2) + "\n");
const summary = lines.join("\n") + "\n";
if (args.summary) writeFileSync(args.summary, summary);
console.log(summary);
if (failed === slugs.length) process.exit(1);
