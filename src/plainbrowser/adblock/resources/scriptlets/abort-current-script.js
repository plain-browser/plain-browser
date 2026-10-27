function pbAbortCurrentScript(chain = '', needle = '') {
  // abort-current-script (acs, abort-current-inline-script, acis): reading or writing `chain` throws when the
  // running script's source (src or inline text) matches `needle`. Other scripts see the property as before.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (chain === '') return;
  const pattern = pbPattern(needle);
  const thisScript = document.currentScript;
  const check = () => {
    const s = document.currentScript;
    if (!(s instanceof HTMLScriptElement) || s === thisScript) return;
    const text = s.src || s.textContent;
    if (pattern.test(text)) throw new ReferenceError(pbAbortMessage);
  };
  pbOnChain(window, chain, (owner, prop) => {
    const desc = Object.getOwnPropertyDescriptor(owner, prop);
    let current = owner[prop];
    try {
      Object.defineProperty(owner, prop, {
        configurable: true,
        get() {
          check();
          return desc && desc.get ? desc.get.call(this) : current;
        },
        set(v) {
          check();
          if (desc && desc.set) desc.set.call(this, v);
          else current = v;
        },
      });
    } catch (e) {}
  });
}
