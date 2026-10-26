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
console.log('fingerprint_script.js: all checks passed (hwc=%d, mem=%d, pixels changed=%d)', a.navigator.hardwareConcurrency, a.navigator.deviceMemory, changed);
