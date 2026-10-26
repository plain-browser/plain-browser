import type { Dict } from "@/lib/i18n";

export function BrowserMock({ t }: { t: Dict["mock"] }) {
  return (
    <div className="mock" aria-hidden="true">
      <div className="mock-tabs">
        <div className="mock-tab"><span className="dot" />{t.tab}</div>
        <div className="mock-plus">+</div>
      </div>
      <div className="mock-bar">
        <span className="mock-nav">‹ › ↻</span>
        <div className="mock-url">
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><rect x="4" y="10" width="16" height="11" rx="2" /><path d="M8 10V7a4 4 0 0 1 8 0v3" /></svg>
          {t.url}
        </div>
        <div className="mock-shield">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><path d="M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z" /><path d="M9 12l2 2 4-4" /></svg>
          {t.blocked}
        </div>
      </div>
      <div className="mock-body">
        <div className="mock-search">{t.placeholder}</div>
        <div className="mock-lines"><span /><span /><span /></div>
        <div className="mock-blocked"><s>ad</s></div>
      </div>
    </div>
  );
}
