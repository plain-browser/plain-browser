import Link from "next/link";
import Image from "next/image";
import { dict, prefix, type Lang } from "@/lib/i18n";
import { site } from "@/lib/site";

export function Shell({ lang, children }: { lang: Lang; children: React.ReactNode }) {
  const t = dict[lang];
  const p = prefix(lang);
  return (
    <>
      {/* Set the document language for the Polish tree without a second root layout. */}
      {lang !== "en" && <script dangerouslySetInnerHTML={{ __html: `document.documentElement.lang=${JSON.stringify(t.htmlLang)}` }} />}
      <header className="header">
        <div className="wrap">
          <Link href={`${p}/`} className="brand">
            <Image src="/logo.svg" alt="" width={26} height={26} priority />
            {site.name}
          </Link>
          <nav className="nav" aria-label="Main">
            <Link href={`${p}/download/`}>{t.nav.download}</Link>
            <Link href={`${p}/privacy/`}>{t.nav.privacy}</Link>
            <Link href={`${p}/security/`} className="hide-sm">{t.nav.security}</Link>
            <a href={site.repo} className="hide-sm" rel="noopener">{t.nav.source}</a>
            <Link href={t.nav.switchHref} className="lang" lang={lang === "en" ? "pl" : "en"} hrefLang={lang === "en" ? "pl" : "en"}>
              {t.nav.switchLabel}
            </Link>
          </nav>
        </div>
      </header>
      <main>{children}</main>
      <footer className="footer">
        <div className="wrap">
          <div>
            <div style={{ color: "var(--fg)", fontWeight: 650 }}>{site.name}</div>
            <div>{t.footer.tagline}</div>
            <p className="disclaimer">{t.footer.disclaimer}</p>
            <div>© 2026 Plain Browser contributors · MPL-2.0 / BSD-3-Clause</div>
          </div>
          <ul>
            <li><Link href={`${p}/download/`}>{t.nav.download}</Link></li>
            <li><Link href={`${p}/privacy/`}>{t.footer.privacy}</Link></li>
            <li><Link href={`${p}/security/`}>{t.footer.security}</Link></li>
            <li><a href={site.repo} rel="noopener">{t.footer.source}</a></li>
            <li><a href={site.issues} rel="noopener">{t.footer.issues}</a></li>
            <li><a href={site.releases} rel="noopener">GitHub Releases</a></li>
          </ul>
        </div>
      </footer>
    </>
  );
}
