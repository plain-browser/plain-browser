// Copyright Plain Browser contributors. MPL-2.0.
// Usage: node fingerprint_script_test.js fingerprint_script.js   (CI: .github/workflows/fingerprint-script.yml)
// Runs fingerprint_script.js in a vm context with minimal DOM stubs and checks determinism.
const fs = require('fs'), vm = require('vm'), assert = require('assert');
const src = fs.readFileSync(process.argv[2], 'utf8');
function makeWorld(seedHi, seedLo) {
  class Navigator { get hardwareConcurrency() { return 16; } get deviceMemory() { return 8; } }
  class ImageData { constructor(n) { this.data = new Uint8ClampedArray(n); } }
  class CanvasRenderingContext2D {
    getImageData(x, y, w, h) { const d = new ImageData(w * h * 4); d.data.fill(200); return d; }
    putImageData(img) { this.last = img; }
    drawImage() {}
  }
  class HTMLCanvasElement {
    constructor() { this.width = 8; this.height = 8; this.ctx = new CanvasRenderingContext2D(); }
    getContext() { return this.ctx; }
    toDataURL() { const img = this.ctx.last; return img ? Array.from(img.data).join(',') : 'orig'; }
    toBlob(cb) { cb && cb(this.ctx.last ? 'noised' : 'orig'); }
  }
  class AudioBuffer { constructor() { this.arr = new Float32Array([1, 1, 1]); } getChannelData() { return this.arr; } copyFromChannel(dst) { dst.set(this.arr.subarray(0, dst.length)); } }
  class WebGLRenderingContext { getParameter(p) { return 'real-' + p; } }
  const document = { createElement: () => new HTMLCanvasElement() };
  const ctx = { Navigator, navigator: new Navigator(), HTMLCanvasElement, CanvasRenderingContext2D, AudioBuffer, WebGLRenderingContext, document, Math, Object, Function, WeakMap, WeakSet, Array, Float32Array, Uint8ClampedArray, console };
  vm.createContext(ctx);
  vm.runInContext(src.replace('__PB_SEED_HI__', String(seedHi)).replace('__PB_SEED_LO__', String(seedLo)), ctx);
  return ctx;
}
const a = makeWorld(123456, 654321), b = makeWorld(123456, 654321), c = makeWorld(999, 1);
// deterministic per seed
assert.strictEqual(a.navigator.hardwareConcurrency, b.navigator.hardwareConcurrency);
assert.ok([2, 4, 8].includes(a.navigator.hardwareConcurrency), 'bucketed hwc');
assert.ok([2, 4, 8].includes(a.navigator.deviceMemory), 'bucketed mem');
// canvas: getImageData noised, deterministic, and toDataURL goes through a noised copy
const ca = new a.HTMLCanvasElement(), cb = new b.HTMLCanvasElement(), cc = new c.HTMLCanvasElement();
const ia = ca.getContext('2d').getImageData(0, 0, 8, 8).data, ib = cb.getContext('2d').getImageData(0, 0, 8, 8).data, ic = cc.getContext('2d').getImageData(0, 0, 8, 8).data;
assert.deepStrictEqual(Array.from(ia), Array.from(ib), 'same seed same noise');
assert.notDeepStrictEqual(Array.from(ia), Array.from(ic), 'different seed different noise');
const changed = Array.from(ia).filter(v => v !== 200).length; assert.ok(changed > 0 && changed < 64, 'few pixels changed: ' + changed);
assert.ok(ia.every(v => v === 200 || v === 201), 'only low bit');
assert.notStrictEqual(ca.toDataURL(), 'orig', 'toDataURL used the noised copy');
// native-looking
assert.strictEqual(a.HTMLCanvasElement.prototype.toDataURL.name, 'toDataURL');
assert.ok(/\[native code\]|toDataURL\(\)/.test(a.Function.prototype.toString.call(a.HTMLCanvasElement.prototype.toDataURL)));
assert.strictEqual(a.Function.prototype.toString.call(a.Function.prototype.toString), a.Function.prototype.toString.call(a.Function.prototype.toString));
// audio fudge applied once
const ab = new a.AudioBuffer(); const d1 = ab.getChannelData(0); const v = d1[0]; ab.getChannelData(0);
assert.ok(v < 1 && v >= 0.99 && d1[0] === v, 'audio fudge once: ' + v + ' ' + d1[0]);
// webgl strings
const gl = new a.WebGLRenderingContext(); assert.strictEqual(gl.getParameter(0x9245), 'Google Inc. (Intel)'); assert.strictEqual(gl.getParameter(1), 'real-1');
// ---- worker scope: WorkerNavigator, OffscreenCanvas and WebGL, no document, no Navigator, no AudioBuffer
function makeWorkerWorld(seedHi, seedLo) {
  class WorkerNavigator { get hardwareConcurrency() { return 16; } get deviceMemory() { return 8; } }
  class ImageData { constructor(n) { this.data = new Uint8ClampedArray(n); } }
  class OffscreenCanvasRenderingContext2D {
    getImageData(x, y, w, h) { const d = new ImageData(w * h * 4); d.data.fill(200); return d; }
    putImageData(img) { this.last = img; }
    drawImage() {}
  }
  class OffscreenCanvas {
    constructor(w, h) { this.width = w || 8; this.height = h || 8; this.ctx = new OffscreenCanvasRenderingContext2D(); }
    getContext() { return this.ctx; }
    convertToBlob() { return Promise.resolve(this.ctx.last ? 'noised' : 'orig'); }
  }
  class WebGLRenderingContext { getParameter(p) { return 'real-' + p; } }
  const ctx = { WorkerNavigator, navigator: new WorkerNavigator(), OffscreenCanvas, OffscreenCanvasRenderingContext2D, WebGLRenderingContext, Math, Object, Function, WeakMap, WeakSet, Array, Float32Array, Uint8ClampedArray, Promise, console };
  vm.createContext(ctx);
  vm.runInContext(src.replace('__PB_SEED_HI__', String(seedHi)).replace('__PB_SEED_LO__', String(seedLo)), ctx);
  return ctx;
}
const wa = makeWorkerWorld(123456, 654321), wb = makeWorkerWorld(123456, 654321), wc = makeWorkerWorld(999, 1);
assert.ok([2, 4, 8].includes(wa.navigator.hardwareConcurrency), 'worker hwc bucketed: ' + wa.navigator.hardwareConcurrency);
assert.ok([2, 4, 8].includes(wa.navigator.deviceMemory), 'worker mem bucketed');
// same seed: the page and its worker report the same buckets
assert.strictEqual(wa.navigator.hardwareConcurrency, a.navigator.hardwareConcurrency, 'page and worker agree (hwc)');
assert.strictEqual(wa.navigator.deviceMemory, a.navigator.deviceMemory, 'page and worker agree (mem)');
assert.strictEqual(wa.navigator.hardwareConcurrency, wb.navigator.hardwareConcurrency);
const oa = new wa.OffscreenCanvas(8, 8).getContext('2d').getImageData(0, 0, 8, 8).data;
const ob = new wb.OffscreenCanvas(8, 8).getContext('2d').getImageData(0, 0, 8, 8).data;
const oc = new wc.OffscreenCanvas(8, 8).getContext('2d').getImageData(0, 0, 8, 8).data;
assert.deepStrictEqual(Array.from(oa), Array.from(ob), 'worker: same seed same noise');
assert.notDeepStrictEqual(Array.from(oa), Array.from(oc), 'worker: different seed different noise');
assert.deepStrictEqual(Array.from(oa), Array.from(ia), 'page and worker noise the same pixels (same seed)');
assert.ok(oa.every(v => v === 200 || v === 201), 'worker: only low bit');
const oco = new wa.OffscreenCanvas(8, 8); oco.convertToBlob().then(r => assert.strictEqual(r, 'noised', 'convertToBlob used the noised copy'));
assert.strictEqual(new wa.WebGLRenderingContext().getParameter(0x9246).startsWith('ANGLE (Intel'), true, 'worker webgl renderer');
console.log('fingerprint_script.js: all checks passed (hwc=%d, mem=%d, pixels changed=%d)', a.navigator.hardwareConcurrency, a.navigator.deviceMemory, changed);
