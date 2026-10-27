function pbAbortOnPropertyRead(chain = '') {
  // abort-on-property-read (aopr): reading `chain` throws, which stops the script that tried.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (chain === '') return;
  pbOnChain(window, chain, (owner, prop) => {
    try {
      Object.defineProperty(owner, prop, {
        configurable: false,
        get() { throw new ReferenceError(pbAbortMessage); },
        set() {},
      });
    } catch (e) {}
  });
}
