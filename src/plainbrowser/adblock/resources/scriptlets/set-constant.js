function pbSetConstant(chain = '', raw = '') {
  // set-constant (alias set): the property at `chain` always reads `raw` (see pbConstant) and ignores writes.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (chain === '') return;
  const value = pbConstant(raw);
  if (value === pbPatternNoValue) return;
  pbOnChain(window, chain, (owner, prop) => {
    try {
      Object.defineProperty(owner, prop, {
        configurable: false,
        get() { return value; },
        set() {},
      });
    } catch (e) {}
  });
}
