// Copyright Plain Browser contributors. MPL-2.0.
// Downloads one extension's release asset from GitHub. Used by build.mjs (the pinned tag) and propose.mjs (the
// latest release).
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";

export const MAX_ASSET = 64 * 1024 * 1024;
export const USER_AGENT = "PlainBrowserExtensionBuild/1 (+https://plain-browser.app)";
export const PINS_FILE = new URL("../pins.json", import.meta.url);

export const sha256 = (data) => createHash("sha256").update(data).digest("hex");
export const readPins = () => JSON.parse(readFileSync(PINS_FILE, "utf8"));

export async function fetchOk(url, headers = {}) {
  const res = await fetch(url, { headers: { "User-Agent": USER_AGENT, ...headers }, redirect: "follow" });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${url}`);
  return res;
}

// {release, asset, bytes} of the release with that tag, or of the latest release when `tag` is null.
export async function fetchUpstream(src, tag = null) {
  const github = { Accept: "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28" };
  if (process.env.GITHUB_TOKEN) github.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const where = tag === null ? "latest" : `tags/${encodeURIComponent(tag)}`;
  const release = await fetchOk(`https://api.github.com/repos/${src.repo}/releases/${where}`, github).then((r) => r.json());
  if (release.draft || release.prerelease) throw new Error(`release ${release.tag_name} is a pre-release`);
  const asset = release.assets?.find((a) => src.asset.test(a.name));
  if (!asset) throw new Error(`release ${release.tag_name} has no asset matching ${src.asset}`);
  if (asset.size > MAX_ASSET) throw new Error(`${asset.name} is ${asset.size} bytes`);
  const bytes = Buffer.from(await fetchOk(asset.browser_download_url).then((r) => r.arrayBuffer()));
  if (bytes.length > MAX_ASSET) throw new Error(`${asset.name} is ${bytes.length} bytes`);
  return { release, asset, bytes };
}
