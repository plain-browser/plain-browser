import Link from "next/link";
import { Shell } from "./Shell";
import { DownloadFiles } from "./DownloadFiles";
import { dict, prefix, type Lang } from "@/lib/i18n";
import { site } from "@/lib/site";
import { getLatestRelease } from "@/lib/release";

export async function Download({ lang }: { lang: Lang }) {
  const t = dict[lang].download;
  const p = prefix(lang);
  const release = await getLatestRelease();
  return (
    <Shell lang={lang}>
      <div className="wrap page">
        <h1>{t.title}</h1>
        <p className="lead">{t.lead}</p>
        <DownloadFiles initial={release} t={t} lang={lang} chromium={site.chromium} os={site.os} />
        <p><a href={site.releases} rel="noopener">{t.allReleases} →</a></p>
        <p className="muted" style={{ fontSize: "0.9rem" }}>{t.privacyNote} <Link href={`${p}/privacy/`}>{dict[lang].footer.privacy}</Link>.</p>
      </div>
    </Shell>
  );
}
