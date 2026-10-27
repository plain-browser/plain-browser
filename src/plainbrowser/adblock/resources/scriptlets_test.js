// Copyright Plain Browser contributors. MPL-2.0.
// Tests for our scriptlets, in Node without a DOM: each test runs the helpers and one scriptlet in a fresh
// vm context that plays `window`. Run: node --test src/plainbrowser/adblock/resources/scriptlets_test.js
'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const dir = path.join(__dirname, 'scriptlets');
const read = (f) => fs.readFileSync(path.join(dir, f), 'utf8');

// A context with the globals the scriptlets touch; `setup` runs first (page state), then the helpers, the
// scriptlet and `call` (what adblock-rust emits), then `after` (the page's later code), each as its own
// script. Returns the context.
function run(files, call, { setup = '', after = '', extra = {} } = {}) {
  const storage = () => {
    const m = new Map();
    return {
      getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)),
      removeItem: (k) => m.delete(k), _map: m,
    };
  };
  const ctx = {
    console, Response, Promise, Proxy, Reflect, Symbol, RegExp, Error, ReferenceError, JSON, Object, Array,
    localStorage: storage(), sessionStorage: storage(), document: { currentScript: null }, ...extra,
  };
  ctx.window = ctx;
  vm.createContext(ctx);
  const source = ['pb-utils.fn.js', ...files].map(read).join('\n');
  // Separate scripts, as on a page: a `var` in the page's code must not be hoisted above the scriptlets.
  if (setup) vm.runInContext(setup, ctx);
  vm.runInContext(`${source}\n;(function(){ ${call} })();`, ctx);
  if (after) vm.runInContext(after, ctx);
  return ctx;
}

test('pbPattern: plain text, regex, negation', () => {
  const ctx = run([], '');
  const p = vm.runInContext('pbPattern', ctx);
  assert.ok(p('ads').test('x-ads-y'));
  assert.ok(!p('ads').test('news'));
  assert.ok(p('/^ad\\d$/').test('ad1'));
  assert.ok(!p('/^ad\\d$/').test('ad12'));
  assert.ok(p('!ads').test('news'));
  assert.ok(p('').test('anything'));
  assert.ok(p('a.b').test('a.b') && !p('a.b').test('axb'), 'dots are literal');
});

test('set-constant: existing and not-yet-created chains, writes ignored', () => {
  const ctx = run(['set-constant.js'],
    "pbSetConstant('adsEnabled', 'false'); pbSetConstant('cfg.ads.show', 'noopFunc'); pbSetConstant('n', '42');",
    { setup: 'var adsEnabled = true;', after: 'adsEnabled = true; var cfg = { ads: { show: () => 1 } };' });
  assert.equal(vm.runInContext('adsEnabled', ctx), false);
  assert.equal(vm.runInContext('typeof cfg.ads.show === "function" && cfg.ads.show()', ctx), undefined);
  assert.equal(vm.runInContext('n', ctx), 42);
  // A value outside the allowed set is ignored.
  const other = run(['set-constant.js'], "pbSetConstant('x', 'alert(1)');", { setup: 'var x = 1;' });
  assert.equal(vm.runInContext('x', other), 1);
});

test('set-constant: two rules waiting on the same missing object both apply', () => {
  const ctx = run(['set-constant.js'], "pbSetConstant('ncbi.sg', '{}'); pbSetConstant('ncbi.sg.ping', 'noopFunc');",
    { after: "window.ncbi = { sg: { ping: function () { return 'real'; }, other: 1 } };" });
  assert.equal(vm.runInContext('ncbi.sg.ping()', ctx), undefined);
  assert.equal(vm.runInContext('ncbi.sg.other', ctx), undefined, 'ncbi.sg is the constant {}');
});

test('abort-on-property-read/write throw for the page', () => {
  const ctx = run(['abort-on-property-read.js', 'abort-on-property-write.js'],
    "pbAbortOnPropertyRead('adblockDetector'); pbAbortOnPropertyWrite('tracker');");
  assert.throws(() => vm.runInContext('adblockDetector.run()', ctx), ReferenceError);
  assert.throws(() => vm.runInContext('tracker = {}', ctx), ReferenceError);
});

test('no-setTimeout-if: matching callbacks are dropped, others run', async () => {
  const calls = [];
  const ctx = run(['pb-timers.fn.js', 'no-setTimeout-if.js'], "pbNoSetTimeoutIf('Adblock', '');",
    { extra: { setTimeout: (fn, ms) => { calls.push(String(fn)); return 1; } } });
  vm.runInContext('setTimeout(function checkAdblock() {}, 10); setTimeout(function render() {}, 10);', ctx);
  assert.equal(calls.length, 1);
  assert.match(calls[0], /render/);
});

test('no-fetch-if: matching requests get an empty 200, others reach fetch', async () => {
  const reached = [];
  const ctx = run(['no-fetch-if.js'], "pbNoFetchIf('ads.example');",
    { extra: { fetch: async (u) => { reached.push(String(u)); return new Response('real'); }, Request } });
  const blocked = await vm.runInContext("fetch('https://ads.example/x')", ctx);
  assert.equal(blocked.status, 200);
  assert.equal(await blocked.text(), '');
  const real = await vm.runInContext("fetch('https://site.example/api')", ctx);
  assert.equal(await real.text(), 'real');
  assert.deepEqual(reached, ['https://site.example/api']);
});

test('json-prune removes paths when the required ones exist', () => {
  const ctx = run(['json-prune.js'], "pbJsonPrune('ads playerAds.[].url', 'video');");
  const out = vm.runInContext(
    'JSON.stringify(JSON.parse(\'{"video":1,"ads":[1],"playerAds":[{"url":"a","id":1}]}\'))', ctx);
  assert.deepEqual(JSON.parse(out), { video: 1, playerAds: [{ id: 1 }] });
  const untouched = vm.runInContext('JSON.stringify(JSON.parse(\'{"ads":[1]}\'))', ctx);
  assert.deepEqual(JSON.parse(untouched), { ads: [1] });
});

test('set-local-storage-item: allowed values and $remove$', () => {
  const ctx = run(['pb-storage.fn.js', 'set-local-storage-item.js'],
    "pbSetLocalStorageItem('consent', 'true'); pbSetLocalStorageItem('evil', '<script>'); pbSetLocalStorageItem('old', '$remove$');",
    { setup: "localStorage.setItem('old', '1');" });
  const m = ctx.localStorage._map;
  assert.equal(m.get('consent'), 'true');
  assert.equal(m.has('evil'), false, 'values outside the allowed set are not written');
  assert.equal(m.has('old'), false);
});

test('window.open-defuser blocks matching URLs', () => {
  const opened = [];
  const ctx = run(['window.open-defuser.js'], "pbWindowOpenDefuser('popunder');",
    { extra: { open: (u) => { opened.push(u); return {}; } } });
  assert.equal(vm.runInContext("open('https://popunder.example/')", ctx), null);
  assert.notEqual(vm.runInContext("open('https://site.example/help')", ctx), null);
  assert.deepEqual(opened, ['https://site.example/help']);
});

test('every scriptlet file starts with its function', () => {
  for (const f of fs.readdirSync(dir).filter((f) => f.endsWith('.js') && !f.endsWith('.fn.js'))) {
    assert.match(read(f), /^function pb[A-Za-z]+\(/, f);
  }
});
