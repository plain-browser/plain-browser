// Copyright Plain Browser contributors. MPL-2.0.
// Shared by no-setTimeout-if and no-setInterval-if.
function pbDefuseTimer(name, needle, delay) {
  const p = pbPattern(needle);
  let delayNot = false;
  let delayValue = null;
  if (delay !== '') {
    delayNot = String(delay).startsWith('!');
    const n = parseInt(String(delay).replace('!', ''), 10);
    if (!isNaN(n)) delayValue = n;
  }
  window[name] = new Proxy(window[name], {
    apply(target, thisArg, args) {
      const [fn, ms] = args;
      const text = typeof fn === 'function' ? Function.prototype.toString.call(fn) : String(fn);
      let delayOk = true;
      if (delayValue !== null) delayOk = (Number(ms || 0) === delayValue) !== delayNot;
      if (p.test(text) && delayOk) return 0;
      return Reflect.apply(target, thisArg, args);
    },
  });
}
