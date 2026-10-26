import fs from "node:fs";
import path from "node:path";

// The privacy policy and security policy live in the repository (docs/, SECURITY.md) and are
// rendered here at build time, so the site can never drift from what the browser ships with.
const root = path.join(process.cwd(), "..");

const linkMap: Record<string, string> = {
  "PRIVACY.md": "/privacy/",
  "PRIVACY.pl.md": "/pl/privacy/",
  "docs/PRIVACY.md": "/privacy/",
  "docs/PRIVACY.pl.md": "/pl/privacy/",
  "docs/SECURITY.pl.md": "/pl/security/",
  "../SECURITY.md": "/security/",
};

export function readDoc(relative: string): string {
  let text = fs.readFileSync(path.join(root, relative), "utf8");
  for (const [from, to] of Object.entries(linkMap)) {
    text = text.split(`](${from})`).join(`](${to})`);
  }
  // Repository-relative references in SECURITY.md point at files on GitHub.
  text = text.replace(/`(\.github\/workflows\/[\w.-]+|scripts\/[\w.-]+)`/g, (_m, p) =>
    `[\`${p}\`](https://github.com/plain-browser/plain-browser/blob/main/${p})`,
  );
  return text;
}
