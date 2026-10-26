import Link from "next/link";
import { Shell } from "./Shell";
import { BrowserMock } from "./BrowserMock";
import { dict, prefix, type Lang } from "@/lib/i18n";
import { site } from "@/lib/site";
import { getLatestRelease } from "@/lib/release";
import { HeroNote } from "./HeroNote";

const icons = [
  <path key="0" d="M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z" />,
  <g key="1"><circle cx="12" cy="12" r="9" /><path d="M5 5l14 14" /></g>,
  <g key="2"><path d="M4 7h16M4 12h16M4 17h10" /></g>,
  <g key="3"><circle cx="12" cy="12" r="9" /><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18" /></g>,
  <g key="4"><path d="M3 6h18l-7 8v5l-4 2v-7z" /></g>,
  <g key="5"><path d="M8 4l-5 8 5 8M16 4l5 8-5 8" /></g>,
];

export async function Home({ lang }: { lang: Lang }) {
  const t = dict[lang];
  const p = prefix(lang);
  const release = await getLatestRelease();
  return (
    <Shell lang={lang}>
      <section className="wrap hero">
        <div>
          <span className="badge">{t.hero.badge}</span>
          <h1>
            {t.hero.title.split(/(?<=\.) /).map((line, i) => (
              <span key={i} style={{ display: "block" }}>{line}</span>
            ))}
          </h1>
          <p className="lead">{t.hero.lead}</p>
          <div className="hero-actions">
            <Link href={`${p}/download/`} className="btn btn-primary">{t.hero.download}</Link>
            <a href={site.repo} className="btn" rel="noopener">{t.hero.source}</a>
          </div>
          <HeroNote initial={release} note={t.hero.downloadNote} chromium={site.chromium} />
        </div>
        <BrowserMock t={t.mock} />
      </section>

      <section className="section" id="features">
        <div className="wrap">
          <h2>{t.pillars.title}</h2>
          <div className="grid">
            {t.pillars.items.map((it, i) => (
              <div className="card" key={it.title}>
                <div className="icon">
                  <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">{icons[i]}</svg>
                </div>
                <h3>{it.title}</h3>
                <p>{it.body}</p>
              </div>
            ))}
          </div>
        </div>
      </section>

      <section className="section" id="privacy">
        <div className="wrap two">
          <div>
            <h2>{t.nothing.title}</h2>
            <p className="section-lead">{t.nothing.lead}</p>
            <ul className="checklist">
              {t.nothing.items.map((s) => (
                <li key={s}><span className="x no">×</span>{s}</li>
              ))}
            </ul>
          </div>
          <div>
            <p className="proof">{t.nothing.proof}</p>
            <p><Link href={`${p}/privacy/`}>{t.footer.privacy} →</Link></p>
          </div>
        </div>
      </section>

      <section className="section" id="how">
        <div className="wrap two">
          <div>
            <h2>{t.built.title}</h2>
            <p className="section-lead">{t.built.lead}</p>
            <p className="muted" style={{ fontSize: "0.95rem" }}>{t.built.licenses}</p>
          </div>
          <div className="stack">
            {t.built.layers.map((l) => (
              <div className="layer" key={l.name}><b>{l.name}</b><span>{l.note}</span></div>
            ))}
          </div>
        </div>
      </section>

      <section className="section" id="status">
        <div className="wrap">
          <h2>{t.status.title}</h2>
          <p className="section-lead">{t.status.lead}</p>
          <div className="two">
            <ul className="checklist">
              {t.status.done.map((s) => <li key={s}><span className="x yes">✓</span>{s}</li>)}
            </ul>
            <ul className="checklist">
              {t.status.todo.map((s) => <li key={s}><span className="x todo">·</span>{s}</li>)}
            </ul>
          </div>
        </div>
      </section>

      <section className="section" id="faq">
        <div className="wrap">
          <h2>{t.faq.title}</h2>
          <div className="faq">
            {t.faq.items.map((f) => (
              <details key={f.q}><summary>{f.q}</summary><p>{f.a}</p></details>
            ))}
          </div>
        </div>
      </section>

      <section className="cta">
        <div className="wrap">
          <h2>{t.cta.title}</h2>
          <p>{t.cta.body}</p>
          <div className="actions">
            <Link href={`${p}/download/`} className="btn btn-primary">{t.cta.download}</Link>
            <a href={site.issues} className="btn" rel="noopener">{t.cta.issues}</a>
          </div>
        </div>
      </section>
    </Shell>
  );
}
