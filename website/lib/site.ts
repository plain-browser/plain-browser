import fs from "node:fs";
import path from "node:path";

// Single source of truth for the version: ../versions.toml at the repository root.
function readVersion(): string {
  try {
    const toml = fs.readFileSync(path.join(process.cwd(), "..", "versions.toml"), "utf8");
    const m = toml.match(/^version\s*=\s*"([^"]+)"/m);
    if (m) return m[1];
  } catch {
    /* built outside the repository: fall back below */
  }
  return process.env.PLAINBROWSER_VERSION ?? "0.2.0-beta.2";
}

export const site = {
  name: "Plain Browser",
  url: "https://plain-browser.app",
  repo: "https://github.com/plain-browser/plain-browser",
  issues: "https://github.com/plain-browser/plain-browser/issues",
  releases: "https://github.com/plain-browser/plain-browser/releases",
  store: "https://apps.microsoft.com/detail/9PKZGSKC9FWP",
  // Microsoft's Store web installer: a small signed .exe that installs the current Store version.
  storeInstaller: "https://get.microsoft.com/installer/download/9PKZGSKC9FWP",
  version: readVersion(),
  chromium: "156",
  os: "Windows 11, x64",
};

export function releaseFile(name: string) {
  return `${site.repo}/releases/download/v${site.version}/${name}`;
}

export const files = {
  installer: `PlainBrowser_${site.version}_x64_installer.exe`,
  zip: `PlainBrowser_${site.version}_x64.zip`,
  sums: "SHA256SUMS.txt",
};
