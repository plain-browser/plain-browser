// Copyright Plain Browser contributors. MPL-2.0.
// Helpers shared by the scriptlets in this directory (dependency "pb-utils.fn" in rs/src/resources.rs).
// adblock-rust puts this text once in front of the scriptlet calls of a page; it runs in the page's main world
// before the page's own scripts.

// A filter argument as a RegExp: "/re/flags" is a regular expression, anything else matches as a plain substring,
// "" matches everything. A leading "!" (kept in `.not`) negates the match for scriptlets that support it.
function pbPattern(arg) {
  let text = String(arg === undefined ? '' : arg);
  let not = false;
  if (text.startsWith('!')) {
    not = true;
    text = text.slice(1);
  }
  let re;
  const m = /^\/(.+)\/([dgimsuy]*)$/.exec(text);
  if (m) {
    try {
      re = new RegExp(m[1], m[2].replace('g', ''));
    } catch (e) {
      re = /(?!)/;
    }
  } else {
    re = new RegExp(text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'));
  }
  return {
    not,
    test(s) {
      return re.test(String(s)) !== not;
    },
  };
}

// The value named by a set-constant style argument, or `pbPatternNoValue` when the name is not allowed.
const pbPatternNoValue = Symbol('no value');
function pbConstant(raw) {
  const named = {
    undefined: undefined, false: false, true: true, null: null, '': '', emptyStr: '',
    noopFunc: function () {}, trueFunc: function () { return true; }, falseFunc: function () { return false; },
    throwFunc: function () { throw new Error(); }, emptyObj: {}, '{}': {}, emptyArr: [], '[]': [],
    noopPromiseResolve: function () { return Promise.resolve(); },
    noopPromiseReject: function () { return Promise.reject(); },
    yes: 'yes', no: 'no',
  };
  if (Object.prototype.hasOwnProperty.call(named, raw)) {
    const v = named[raw];
    // Fresh objects per use, so pages cannot share state through them.
    if (raw === 'emptyObj' || raw === '{}') return {};
    if (raw === 'emptyArr' || raw === '[]') return [];
    return v;
  }
  if (/^-?\d+$/.test(raw) && Math.abs(Number(raw)) <= 0x7fff) return Number(raw);
  if (/^'.*'$/.test(raw)) return raw.slice(1, -1);
  return pbPatternNoValue;
}

// Calls `onOwner(owner, prop)` for the last property of a dotted `chain` under `root` ("a.b.c": owner a.b, prop
// "c"), now if the owner exists, or when a script creates it later (each missing link is trapped with an
// accessor that continues the walk when it is assigned).
function pbOnChain(root, chain, onOwner) {
  const dot = chain.indexOf('.');
  if (dot === -1) {
    onOwner(root, chain);
    return;
  }
  const prop = chain.slice(0, dot);
  const rest = chain.slice(dot + 1);
  const value = root[prop];
  if (value !== undefined && value !== null && (typeof value === 'object' || typeof value === 'function')) {
    pbOnChain(value, rest, onOwner);
    return;
  }
  const desc = Object.getOwnPropertyDescriptor(root, prop);
  if (desc && desc.configurable === false) return;
  // Another scriptlet may already wait on this property: keep its trap working (it runs first).
  const prevGet = desc && desc.get;
  const prevSet = desc && desc.set;
  let current = value;
  try {
    Object.defineProperty(root, prop, {
      configurable: true,
      enumerable: true,
      get() { return prevGet ? prevGet.call(this) : current; },
      set(v) {
        if (prevSet) prevSet.call(this, v);
        current = prevGet ? prevGet.call(this) : v;
        if (current !== undefined && current !== null && (typeof current === 'object' || typeof current === 'function')) {
          pbOnChain(current, rest, onOwner);
        }
      },
    });
  } catch (e) {}
}

// Runs `fn` now if the DOM is parsed, otherwise at DOMContentLoaded, and again for every DOM change (batched).
function pbOnDom(fn) {
  const run = () => { try { fn(); } catch (e) {} };
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', run, { once: true });
  } else {
    run();
  }
  let queued = false;
  new MutationObserver(() => {
    if (queued) return;
    queued = true;
    setTimeout(() => { queued = false; run(); }, 0);
  }).observe(document, { childList: true, subtree: true, attributes: true });
}

// The magic string thrown by the abort-* scriptlets, so a page sees an ordinary ReferenceError.
const pbAbortMessage = 'Plain Browser: blocked by a filter rule';
