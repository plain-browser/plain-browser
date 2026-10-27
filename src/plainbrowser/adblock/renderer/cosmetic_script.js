// Copyright Plain Browser contributors. MPL-2.0.
// Isolated-world script of CosmeticFilterAgent (cosmetic_filter_agent.cc), injected once per document when the
// document has procedural filters or generic class/id rules apply to it. Two jobs:
//   1. Procedural and action filters: selectors with operators that are not CSS (:has-text(), :upward(),
//      :xpath(), :matches-css(), :min-text-length(), :matches-attr(), :matches-path(), and :has()/:not() around
//      them) and the actions :remove(), :style(), :remove-attr(), :remove-class(). Applied now, at
//      DOMContentLoaded and load, and again whenever the DOM changes (batched).
//   2. Watching for class names and ids that appear after the agent's own scans (content added by scripts at any
//      time), reported to the agent through __plainbrowserCosmeticReport(classes, ids), which asks the browser
//      for matching generic rules.
// The agent replaces the config placeholder below (the only occurrence of its name) with a JSON string literal:
//   {"generic": bool, "actions": ["<adblock-rust action JSON>", ...], "selectors": ["<procedural selector>", ...]}
(function () {
  'use strict';
  const config = JSON.parse(__PB_COSMETIC_CONFIG__);
  const report = globalThis.__plainbrowserCosmeticReport;
  delete globalThis.__plainbrowserCosmeticReport;

  // ---------------------------------------------------------------------------------------------------------
  // Procedural selectors

  const OPERATORS = new Set([
    'has-text', 'upward', 'xpath', 'matches-css', 'matches-css-before', 'matches-css-after', 'min-text-length',
    'matches-attr', 'matches-path', 'has', 'not', 'remove', 'style', 'remove-attr', 'remove-class',
  ]);
  const ACTIONS = new Set(['remove', 'style', 'remove-attr', 'remove-class']);

  // "/re/flags" or plain text (substring).
  function pattern(arg) {
    const text = String(arg).trim().replace(/^(["'])(.*)\1$/, '$2');
    const m = /^\/(.+)\/([imsu]*)$/.exec(text);
    if (m) {
      try { return new RegExp(m[1], m[2]); } catch (e) { return /(?!)/; }
    }
    return new RegExp(text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'));
  }

  // Index of the ")" closing the "(" at `open`, honouring nesting, brackets and quotes; -1 if unbalanced.
  function closing(s, open) {
    let depth = 0;
    let quote = '';
    for (let i = open; i < s.length; i++) {
      const c = s[i];
      if (quote) {
        if (c === '\\') i++;
        else if (c === quote) quote = '';
        continue;
      }
      if (c === '"' || c === "'") quote = c;
      else if (c === '\\') i++;
      else if (c === '(' || c === '[') depth++;
      else if (c === ')' || c === ']') {
        depth--;
        if (depth === 0) return c === ')' ? i : -1;
      }
    }
    return -1;
  }

  function containsOperator(s) {
    return /:(has-text|upward|xpath|matches-css(-before|-after)?|min-text-length|matches-attr|matches-path)\(/.test(s);
  }

  // Splits a selector into steps: {css} for plain CSS parts and {op, arg} for operators, plus a trailing action.
  // :has()/:not() stay CSS unless their argument holds an operator. Returns null when it cannot be parsed.
  function compile(selector) {
    const steps = [];
    let action = null;
    let css = '';
    let i = 0;
    while (i < selector.length) {
      const c = selector[i];
      if (c === '[' || c === '"' || c === "'") {
        // Copy attribute selectors and strings whole.
        let j = i;
        if (c === '[') {
          let depth = 0;
          let quote = '';
          for (; j < selector.length; j++) {
            const d = selector[j];
            if (quote) { if (d === '\\') j++; else if (d === quote) quote = ''; continue; }
            if (d === '"' || d === "'") quote = d;
            else if (d === '[') depth++;
            else if (d === ']' && --depth === 0) break;
          }
        } else {
          for (j = i + 1; j < selector.length && selector[j] !== c; j++) if (selector[j] === '\\') j++;
        }
        css += selector.slice(i, j + 1);
        i = j + 1;
        continue;
      }
      if (c === ':') {
        const m = /^:([a-z-]+)\(/.exec(selector.slice(i));
        if (m && OPERATORS.has(m[1])) {
          const open = i + m[0].length - 1;
          const close = closing(selector, open);
          if (close === -1) return null;
          const name = m[1];
          const arg = selector.slice(open + 1, close);
          if ((name === 'has' || name === 'not') && !containsOperator(arg)) {
            css += selector.slice(i, close + 1);
          } else if (ACTIONS.has(name)) {
            if (close !== selector.length - 1 && selector.slice(close + 1).trim() !== '') return null;
            action = { type: name, arg };
          } else {
            if (css.trim() !== '' || steps.length === 0) steps.push({ css });
            css = '';
            steps.push({ op: name, arg, inner: name === 'has' || name === 'not' ? compile(arg) : null });
          }
          i = close + 1;
          continue;
        }
      }
      css += c;
      i++;
    }
    if (css.trim() !== '' || steps.length === 0) steps.push({ css });
    return { steps, action };
  }

  // Elements `css` reaches from `el`: a leading combinator (" ", ">", "+", "~") walks from el; anything else
  // filters el itself.
  function fromElement(el, css) {
    const trimmed = css.trimStart();
    if (trimmed === '') return [el];
    const startsWithCombinator = /^\s/.test(css) || /^[>+~]/.test(trimmed);
    try {
      if (!startsWithCombinator) return el.matches('*' + trimmed) ? [el] : [];
      if (trimmed[0] === '+' || trimmed[0] === '~') {
        const rest = trimmed.slice(1).trim();
        const out = [];
        for (let s = el.nextElementSibling; s; s = s.nextElementSibling) {
          if (s.matches(rest)) out.push(s);
          if (trimmed[0] === '+') break;
        }
        return out;
      }
      return Array.from(el.querySelectorAll(':scope ' + trimmed));
    } catch (e) {
      return [];
    }
  }

  function textOf(el) {
    return el.textContent || '';
  }

  function cssMatches(el, arg, pseudo) {
    const i = arg.indexOf(':');
    if (i === -1) return false;
    const prop = arg.slice(0, i).trim();
    const want = pattern(arg.slice(i + 1).trim());
    try {
      return want.test(getComputedStyle(el, pseudo).getPropertyValue(prop));
    } catch (e) {
      return false;
    }
  }

  function attrMatches(el, arg) {
    const m = /^\s*("?)(.+?)\1\s*(?:=\s*("?)(.*?)\3)?\s*$/.exec(arg);
    if (!m) return false;
    const nameP = pattern(m[2]);
    const valueP = m[4] !== undefined ? pattern(m[4]) : null;
    for (const a of el.attributes) {
      if (nameP.test(a.name) && (valueP === null || valueP.test(a.value))) return true;
    }
    return false;
  }

  // Applies the steps of `compiled` to `roots` (the document, or the element of an enclosing :has()/:not()).
  function evaluate(compiled, roots, relative) {
    let nodes = null;
    for (const step of compiled.steps) {
      if (step.css !== undefined) {
        if (nodes === null) {
          const css = step.css.trim();
          nodes = [];
          for (const root of roots) {
            try {
              if (css === '') nodes.push(...(relative ? root.querySelectorAll(':scope *') : root.querySelectorAll('*')));
              else if (relative) nodes.push(...fromElement(root, /^[>+~]/.test(css) ? css : ' ' + css));
              else nodes.push(...root.querySelectorAll(css));
            } catch (e) {}
          }
        } else {
          nodes = nodes.flatMap((el) => fromElement(el, step.css));
        }
        continue;
      }
      if (nodes === null) nodes = Array.from(document.querySelectorAll('*'));
      const arg = step.arg;
      switch (step.op) {
        case 'has-text': { const p = pattern(arg); nodes = nodes.filter((el) => p.test(textOf(el))); break; }
        case 'min-text-length': { const n = parseInt(arg, 10) || 0; nodes = nodes.filter((el) => textOf(el).length >= n); break; }
        case 'matches-css': nodes = nodes.filter((el) => cssMatches(el, arg, null)); break;
        case 'matches-css-before': nodes = nodes.filter((el) => cssMatches(el, arg, '::before')); break;
        case 'matches-css-after': nodes = nodes.filter((el) => cssMatches(el, arg, '::after')); break;
        case 'matches-attr': nodes = nodes.filter((el) => attrMatches(el, arg)); break;
        case 'matches-path': { const p = pattern(arg); if (!p.test(location.pathname + location.search)) nodes = []; break; }
        case 'upward': {
          const n = /^\d+$/.test(arg.trim()) ? parseInt(arg, 10) : 0;
          nodes = nodes.map((el) => {
            if (n > 0) {
              let p = el;
              for (let k = 0; k < n && p; k++) p = p.parentElement;
              return p;
            }
            try { return el.parentElement && el.parentElement.closest(arg); } catch (e) { return null; }
          }).filter(Boolean);
          break;
        }
        case 'xpath': {
          const out = [];
          for (const el of nodes) {
            try {
              const r = document.evaluate(arg, el, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
              for (let k = 0; k < r.snapshotLength; k++) if (r.snapshotItem(k).nodeType === 1) out.push(r.snapshotItem(k));
            } catch (e) {}
          }
          nodes = out;
          break;
        }
        case 'has':
          nodes = step.inner ? nodes.filter((el) => evaluate(step.inner, [el], true).length > 0) : [];
          break;
        case 'not':
          nodes = step.inner ? nodes.filter((el) => evaluate(step.inner, [el], true).length === 0) : nodes;
          break;
        default:
          nodes = [];
      }
    }
    return Array.from(new Set(nodes || []));
  }

  function act(el, action) {
    if (action === null) {
      el.style.setProperty('display', 'none', 'important');
      return;
    }
    switch (action.type) {
      case 'remove': el.remove(); break;
      case 'style':
        for (const decl of String(action.arg).split(';')) {
          const m = /^\s*([-a-zA-Z]+)\s*:\s*(.+?)\s*(!important)?\s*$/.exec(decl);
          if (m) el.style.setProperty(m[1], m[2], m[3] ? 'important' : '');
        }
        break;
      case 'remove-attr': for (const a of String(action.arg).split('|')) el.removeAttribute(a.trim()); break;
      case 'remove-class': for (const c of String(action.arg).split('|')) el.classList.remove(c.trim()); break;
    }
  }

  // adblock-rust's action JSON: {"selector": [{"type": "css-selector", "arg": ...}, ...], "action": {...}}.
  function fromActionJson(json) {
    try {
      const f = JSON.parse(json);
      const selector = f.selector.map((s) => (s.type === 'css-selector' ? s.arg : `:${s.type}(${s.arg})`)).join('');
      const compiled = compile(selector);
      if (compiled === null) return null;
      if (f.action) compiled.action = { type: f.action.type, arg: f.action.arg === undefined ? '' : f.action.arg };
      return compiled;
    } catch (e) {
      return null;
    }
  }

  const filters = [];
  for (const s of config.selectors || []) {
    const c = compile(s);
    if (c !== null) filters.push(c);
  }
  for (const a of config.actions || []) {
    const c = fromActionJson(a);
    if (c !== null) filters.push(c);
  }

  function applyFilters() {
    for (const f of filters) {
      for (const el of evaluate(f, [document], false)) {
        try { act(el, f.action); } catch (e) {}
      }
    }
  }

  // ---------------------------------------------------------------------------------------------------------
  // New class names and ids

  const MAX_PER_REPORT = 2000;
  const MAX_TOTAL = 50000;
  const seen = new Set();
  let pendingClasses = [];
  let pendingIds = [];

  function note(el) {
    if (seen.size >= MAX_TOTAL) return;
    const id = el.id;
    if (typeof id === 'string' && id !== '' && id.length <= 256 && !seen.has('#' + id)) {
      seen.add('#' + id);
      pendingIds.push(id);
    }
    const cls = el.getAttribute && el.getAttribute('class');
    if (cls) {
      for (const name of cls.split(/\s+/)) {
        if (name !== '' && name.length <= 256 && !seen.has('.' + name)) {
          seen.add('.' + name);
          pendingClasses.push(name);
        }
      }
    }
  }

  function noteTree(node) {
    if (node.nodeType !== 1) return;
    note(node);
    for (const el of node.querySelectorAll('[class],[id]')) note(el);
  }

  function flushNames() {
    while (pendingClasses.length > 0 || pendingIds.length > 0) {
      const classes = pendingClasses.splice(0, MAX_PER_REPORT);
      const ids = pendingIds.splice(0, Math.max(0, MAX_PER_REPORT - classes.length));
      try { report(classes, ids); } catch (e) { return; }
    }
  }

  // ---------------------------------------------------------------------------------------------------------
  // Scheduling

  const generic = config.generic === true && typeof report === 'function';
  let namesTimer = 0;
  let filtersTimer = 0;
  const scheduleNames = () => { if (!namesTimer) namesTimer = setTimeout(() => { namesTimer = 0; flushNames(); }, 250); };
  const scheduleFilters = () => { if (!filtersTimer) filtersTimer = setTimeout(() => { filtersTimer = 0; applyFilters(); }, 100); };

  if (filters.length > 0) {
    applyFilters();
    document.addEventListener('DOMContentLoaded', applyFilters, { once: true });
    window.addEventListener('load', applyFilters, { once: true });
  }
  if (!generic && filters.length === 0) return;
  // The agent reported the names present at its scans; start from the current DOM so they are not sent again.
  if (generic) {
    for (const el of document.querySelectorAll('[class],[id]')) note(el);
    pendingClasses = [];
    pendingIds = [];
  }
  new MutationObserver((records) => {
    if (generic) {
      for (const r of records) {
        if (r.type === 'childList') for (const n of r.addedNodes) noteTree(n);
        else if (r.type === 'attributes' && r.target.nodeType === 1) note(r.target);
      }
      if (pendingClasses.length > 0 || pendingIds.length > 0) scheduleNames();
    }
    if (filters.length > 0) scheduleFilters();
  }).observe(document, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ['class', 'id'],
    characterData: filters.length > 0,
  });
})();
