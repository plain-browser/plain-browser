"use client";
import type { Dict, Lang } from "@/lib/i18n";
import type { Release } from "@/lib/release";
import { useRelease } from "./useRelease";

const fileName = (url: string | null) => (url ? decodeURIComponent(url.split("/").pop() ?? "") : "");

export function DownloadFiles({ initial, t, lang, chromium, os, store, storeInstaller }: { initial: Release; t: Dict["download"]; lang: Lang; chromium: string; os: string; store: string; storeInstaller: string }) {
  const r = useRelease(initial);
  const installer = fileName(r.installer);
  const date = r.publishedAt ? new Date(r.publishedAt).toLocaleDateString(lang === "pl" ? "pl-PL" : "en-GB", { year: "numeric", month: "long", day: "numeric" }) : null;
  return (
    <>
      <div className="meta">
        <div><b>{t.version}</b>{r.version}</div>
        <div><b>Chromium</b>{chromium}</div>
        <div><b>{t.requires}</b>{os}</div>
        {date && <div><b>{t.published}</b>{date}</div>}
      </div>

      <div className="files">
        <div className="file">
          <div className="name">{t.store}</div>
          <p>{t.storeNote}</p>
          <a className="btn btn-primary" href={storeInstaller} rel="noopener">{t.storeButton}</a>{" "}
          <a className="btn" href={store} rel="noopener">{t.storePage}</a>
        </div>
        <div className={`file${r.installer ? "" : " soon"}`}>
          <div className="name">{t.installer}</div>
          {r.installer && <code>{installer}</code>}
          <p>{t.installerNote}</p>
          {r.installer && <a className="btn" href={r.installer}>.exe</a>}
        </div>
        <div className={`file${r.zip ? "" : " soon"}`}>
          <div className="name">{t.zip}</div>
          {r.zip && <code>{fileName(r.zip)}</code>}
          <p>{t.zipNote}</p>
          {r.zip && <a className="btn" href={r.zip}>.zip</a>}
        </div>
        <div className={`file${r.sums ? "" : " soon"}`}>
          <div className="name">{t.sums}</div>
          {r.sums && <code>{fileName(r.sums)}</code>}
          {r.sums && <a className="btn" href={r.sums}>SHA256SUMS.txt</a>}
        </div>
        {r.msix && (
          <div className="file">
            <div className="name">MSIX</div>
            <code>{fileName(r.msix)}</code>
            <p>{t.msixNote}</p>
            <a className="btn" href={r.msix}>.msix</a>
          </div>
        )}
      </div>

      <div className="warn">
        <h3>{t.unsignedTitle}</h3>
        <p>{t.unsignedBody}</p>
      </div>

      <h2>{t.verifyTitle}</h2>
      <p className="muted">{t.verifyBody}</p>
      <pre><code>{`# PowerShell
Get-FileHash .\\${installer || "PlainBrowser_x64_installer.exe"} -Algorithm SHA256
Get-Content .\\SHA256SUMS.txt

# cosign (releases published through sign-release.yml)
cosign verify-blob --bundle ${installer || "<file>"}.bundle \\
  --certificate-identity-regexp 'https://github.com/plain-browser/plain-browser/.github/workflows/sign-release.yml@.*' \\
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \\
  ${installer || "<file>"}`}</code></pre>

      <p><a href={r.url} rel="noopener">{t.releaseNotes} {r.tag} →</a></p>
    </>
  );
}
