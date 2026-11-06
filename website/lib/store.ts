import { getText } from "./http";

// The version the Microsoft Store currently serves, read from the public product catalog the Store app and
// winget use. The catalog is not a documented API: on any failure the site simply shows no Store version.
// The parser is shared in spirit with worker/src/index.ts (GET /api/store-version); keep the two in sync.

export const STORE_ID = "9PKZGSKC9FWP";
export const storeCatalogUrl = `https://displaycatalog.mp.microsoft.com/v7.0/products?bigIds=${STORE_ID}&market=US&languages=en-us,neutral`;

// Package names look like "PlainBrowser.PlainBrowser_0.1.5.0_x64__7m7psx7yqqmx8"; take the highest x64 one.
// "0.1.5.0" is shown as "0.1.5", matching the product version without its "-alpha.N" / "-beta.N" suffix.
export function parseStoreVersion(text: string): string | null {
  const found = [...text.matchAll(/_(\d+)\.(\d+)\.(\d+)\.(\d+)_x64__/g)].map((m) => m.slice(1, 5).map(Number));
  if (!found.length) return null;
  found.sort((a, b) => a.reduce((d, _, i) => d || b[i] - a[i], 0));
  const [major, minor, patch, rev] = found[0];
  return rev ? `${major}.${minor}.${patch}.${rev}` : `${major}.${minor}.${patch}`;
}

export async function getStoreVersion(): Promise<string | null> {
  try {
    const v = parseStoreVersion(await getText(storeCatalogUrl, { accept: "application/json", "user-agent": "plain-browser.app build" }));
    if (!v) throw new Error("no x64 package in the catalog");
    console.log(`store: ${v} from the Microsoft Store catalog`);
    return v;
  } catch (e) {
    console.warn(`store: version unknown (${(e as Error).message})`);
    return null;
  }
}
