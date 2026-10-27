function pbAbortOnStackTrace(chain = '', needle = '') {
  // abort-on-stack-trace (aost): reading or writing `chain` throws when the JavaScript stack at that moment
  // matches `needle` (for example a script file name).
  // Copyright Plain Browser contributors. MPL-2.0.
  if (chain === '') return;
  const pattern = pbPattern(needle);
  const check = () => {
    const stack = (new Error().stack || '').split('\n').slice(2).join('\n');
    if (pattern.test(stack)) throw new ReferenceError(pbAbortMessage);
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
