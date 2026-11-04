import { site } from "./site";
import { getText } from "./http";

// Shape shared by the build-time fetch (lib/release.ts), the client refresh (components/useRelease.ts)
// and the Cloudflare Worker (worker/src/index.ts). Keep them in sync.
export type Release = {
  version: string; // "0.1.0-alpha.1"
  tag: string; // "v0.1.0-alpha.1"
  url: string; // release page
  publishedAt: string | null;
  prerelease: boolean;
  installer: string | null;
  zip: string | null;
  msix: string | null;
  sums: string | null;
  // Set when GitHub could not be asked at build time: the version comes from versions.toml, which may be
  // ahead of any published release, so no file links are made from it.
  fallback?: boolean;
};

type GhAsset = { name: string; browser_download_url: string };
type GhRelease = {
  tag_name: string;
  html_url: string;
  draft: boolean;
  prerelease: boolean;
  published_at: string | null;
  assets: GhAsset[];
};

export function pickAssets(r: GhRelease): Release {
  const find = (re: RegExp) => r.assets.find((a) => re.test(a.name))?.browser_download_url ?? null;
  return {
    version: r.tag_name.replace(/^v/, ""),
    tag: r.tag_name,
    url: r.html_url,
    publishedAt: r.published_at,
    prerelease: r.prerelease,
    installer: find(/_installer\.exe$/i),
    zip: find(/x64\.zip$/i),
    msix: find(/\.msix$/i),
    sums: find(/^SHA256SUMS/i),
  };
}

// The newest published release, prereleases included (alphas are prereleases, and
// /releases/latest would skip them). Drafts are never returned by the API without auth.
export function fallbackRelease(): Release {
  const v = site.version;
  return {
    version: v,
    tag: `v${v}`,
    url: site.releases,
    publishedAt: null,
    prerelease: true,
    installer: null,
    zip: null,
    msix: null,
    sums: null,
    fallback: true,
  };
}

export async function getLatestRelease(): Promise<Release> {
  try {
    const headers: Record<string, string> = { Accept: "application/vnd.github+json", "User-Agent": "plain-browser.app build" };
    if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
    const list = JSON.parse(await getText("https://api.github.com/repos/plain-browser/plain-browser/releases?per_page=10", headers)) as GhRelease[];
    const r = list.find((x) => !x.draft);
    if (!r) throw new Error("no published release");
    console.log(`release: ${r.tag_name} from GitHub`);
    return pickAssets(r);
  } catch (e) {
    console.warn(`release: falling back to versions.toml (${(e as Error).message})`);
    return fallbackRelease();
  }
}
