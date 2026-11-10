#!/usr/bin/env node
// Copyright Plain Browser contributors. MPL-2.0.
// Uploads the output of build.mjs to the R2 bucket the Worker serves from.
//
//   node scripts/publish.mjs [--dist DIR] [--bucket NAME] [--dry-run]
//
// Order matters: the versioned CRX files first, then <slug>.crx, catalog.json, and updates.xml last, so a reader
// never gets an updates.xml naming a file that is not there yet. Needs CLOUDFLARE_API_TOKEN (or `wrangler login`).
import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseArgs } from "node:util";

// Pinned so CI does not pick up a wrangler release on the day it comes out.
const WRANGLER = "wrangler@4.151.3";

const { values: args } = parseArgs({
  options: {
    dist: { type: "string", default: "dist" },
    bucket: { type: "string" },
    "dry-run": { type: "boolean", default: false },
  },
});

// The bucket named in wrangler.toml ([[r2_buckets]] bucket_name), unless given.
const bucket =
  args.bucket ??
  /^\s*bucket_name\s*=\s*"([^"]+)"/m.exec(readFileSync(new URL("../wrangler.toml", import.meta.url), "utf8"))?.[1];
if (!bucket) throw new Error("no bucket_name in wrangler.toml; pass --bucket");

const crxDir = join(args.dist, "crx");
// No crx/ when every extension kept its published version: only catalog.json and updates.xml go up.
const crx = existsSync(crxDir) ? readdirSync(crxDir).filter((f) => f.endsWith(".crx")).sort() : [];
const versioned = crx.filter((f) => /^[a-z]+-[\d.]+\.crx$/.test(f));
const latest = crx.filter((f) => /^[a-z]+\.crx$/.test(f));
const uploads = [
  ...versioned.map((f) => [`crx/${f}`, join(crxDir, f), "application/x-chrome-extension"]),
  ...latest.map((f) => [`crx/${f}`, join(crxDir, f), "application/x-chrome-extension"]),
  ["catalog.json", join(args.dist, "catalog.json"), "application/json"],
  ["updates.xml", join(args.dist, "updates.xml"), "application/xml"],
];

for (const [key, file, type] of uploads) {
  const argv = ["--yes", WRANGLER, "r2", "object", "put", `${bucket}/${key}`, "--file", file, "--content-type", type, "--remote"];
  console.log(`npx ${argv.join(" ")}`);
  if (args["dry-run"]) continue;
  const r = spawnSync("npx", argv, { stdio: "inherit" });
  if (r.status !== 0) {
    console.error(`upload of ${key} failed; stopping before anything that names it`);
    process.exit(1);
  }
}
