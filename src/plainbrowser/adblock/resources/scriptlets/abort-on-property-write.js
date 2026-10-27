function pbAbortOnPropertyWrite(chain = '') {
  // abort-on-property-write (aopw): assigning `chain` throws.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (chain === '') return;
  pbOnChain(window, chain, (owner, prop) => {
    let current = owner[prop];
    try {
      Object.defineProperty(owner, prop, {
        configurable: false,
        get() { return current; },
        set() { throw new ReferenceError(pbAbortMessage); },
      });
    } catch (e) {}
  });
}
