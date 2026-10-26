// Copyright Plain Browser contributors. MPL-2.0.
//
// Fingerprint protection, injected into the main world of every document
// before page scripts run (renderer/fingerprint_agent.cc). The two seed
// placeholders are replaced with a value that is fixed for (session, top-level
// site), so a site sees stable values for the whole session, two sites see
// different ones, and the next browser run gives new ones.
//
// Model: Brave's "farbling". The noise stays inside plausible ranges (a flipped
// low bit in a few canvas pixels, an audio gain change of under 1 %,
// hardware values rounded to common buckets). Surfaces: 2D canvas readback,
// WebGL vendor/renderer strings, Web Audio readback, navigator.hardwareConcurrency,
// navigator.deviceMemory. Not covered: workers (no document), WebGL readPixels,
// fonts, screen size.
//
// Known limit: these are JavaScript overrides in the page world. A script can
// detect them (for example by comparing with an iframe whose context it
// snapshots very early) more easily than Blink-level changes, and a site on
// the exception list sees nothing. The wrappers report "[native code]" from
// Function.prototype.toString, which stops the cheap checks.
(function () {
  'use strict';

  var SEED_HI = __PB_SEED_HI__;
  var SEED_LO = __PB_SEED_LO__;

  // mulberry32: small, deterministic, good enough for noise.
  function prng(seed) {
    var a = seed >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  var siteSeed = (SEED_HI ^ Math.imul(SEED_LO, 0x9E3779B1)) >>> 0;
  var siteRand = prng(siteSeed);
  // Per-site constants, drawn once so every read agrees.
  var audioFudge = 0.99 + siteRand() * 0.01;        // [0.99, 1.0)
  var hwBucketPick = siteRand();
  var memBucketPick = siteRand();
  var canvasSalt = (siteRand() * 0xffffffff) >>> 0;

  // ---- native-looking wrappers -------------------------------------------
  var originals = new WeakMap();
  var defineProperty = Object.defineProperty;
  var getOwn = Object.getOwnPropertyDescriptor;
  var apply = Function.prototype.apply;
  var call = Function.prototype.call;

  function makeNative(fn, name, length) {
    try {
      defineProperty(fn, 'name', { value: name, configurable: true });
      defineProperty(fn, 'length', { value: length, configurable: true });
    } catch (e) { /* ignore */ }
    return fn;
  }

  // Replace a method on a prototype; `make(orig)` returns the replacement.
  function wrapMethod(proto, name, make) {
    if (!proto) return;
    var d = getOwn(proto, name);
    if (!d || typeof d.value !== 'function') return;
    var orig = d.value;
    var fn = makeNative(make(orig), name, orig.length);
    originals.set(fn, orig);
    defineProperty(proto, name, { value: fn, writable: d.writable, enumerable: d.enumerable, configurable: d.configurable });
  }

  // Replace an accessor on a prototype; `make(origGetter)` returns the getter.
  function wrapGetter(proto, name, make) {
    if (!proto) return;
    var d = getOwn(proto, name);
    if (!d || typeof d.get !== 'function') return;
    var orig = d.get;
    var fn = makeNative(make(orig), 'get ' + name, 0);
    originals.set(fn, orig);
    defineProperty(proto, name, { get: fn, set: d.set, enumerable: d.enumerable, configurable: d.configurable });
  }

  // Function.prototype.toString: report the original's source for wrappers.
  (function () {
    var toStringOrig = Function.prototype.toString;
    var toStringNew = function toString() {
      var o = originals.get(this);
      return call.call(toStringOrig, o !== undefined ? o : this);
    };
    makeNative(toStringNew, 'toString', 0);
    originals.set(toStringNew, toStringOrig);
    defineProperty(Function.prototype, 'toString', { value: toStringNew, writable: true, enumerable: false, configurable: true });
  })();

  // ---- navigator ------------------------------------------------------------
  function bucket(real, buckets, pick) {
    var allowed = [];
    for (var i = 0; i < buckets.length; i++) if (buckets[i] <= real) allowed.push(buckets[i]);
    if (allowed.length === 0) return real;
    return allowed[Math.min(allowed.length - 1, Math.floor(pick * allowed.length))];
  }
  try {
    var NavProto = typeof Navigator !== 'undefined' ? Navigator.prototype : null;
    wrapGetter(NavProto, 'hardwareConcurrency', function (orig) {
      return function hardwareConcurrency() {
        var real = call.call(orig, this);
        return typeof real === 'number' ? bucket(real, [2, 4, 8], hwBucketPick) : real;
      };
    });
    wrapGetter(NavProto, 'deviceMemory', function (orig) {
      return function deviceMemory() {
        var real = call.call(orig, this);
        return typeof real === 'number' ? bucket(real, [2, 4, 8], memBucketPick) : real;
      };
    });
  } catch (e) { /* keep going */ }

  // ---- 2D canvas ------------------------------------------------------------
  // Flip the low bit of one channel in about 3 % of pixels. Deterministic in
  // (site, buffer length, position): reading the same canvas twice gives the
  // same bytes, so pages that compare readbacks still work.
  function noise(data) {
    var n = data.length;
    if (!n) return;
    var r = prng((canvasSalt ^ n) >>> 0);
    for (var i = 0; i < n; i += 4) {
      if (r() < 0.03) {
        var ch = (r() * 3) | 0;
        data[i + ch] = data[i + ch] ^ 1;
      }
    }
  }
  try {
    var HC = typeof HTMLCanvasElement !== 'undefined' ? HTMLCanvasElement : null;
    var OC = typeof OffscreenCanvas !== 'undefined' ? OffscreenCanvas : null;
    var C2D = typeof CanvasRenderingContext2D !== 'undefined' ? CanvasRenderingContext2D : null;
    var O2D = typeof OffscreenCanvasRenderingContext2D !== 'undefined' ? OffscreenCanvasRenderingContext2D : null;
    var getContextHC = HC && HC.prototype.getContext;
    var getContextOC = OC && OC.prototype.getContext;
    var getImageData2D = C2D && C2D.prototype.getImageData;
    var getImageDataO2D = O2D && O2D.prototype.getImageData;

    // A copy of `canvas` with noised pixels, or null when it cannot be read
    // (zero size, tainted by cross-origin content: the original call will
    // throw the same SecurityError the page expects).
    function noisedCopy(canvas, offscreen) {
      var w = canvas.width, h = canvas.height;
      if (!w || !h) return null;
      var copy, ctx, getImageData;
      if (offscreen) {
        copy = new OC(w, h);
        ctx = call.call(getContextOC, copy, '2d');
        getImageData = getImageDataO2D;
      } else {
        copy = document.createElement('canvas');
        copy.width = w; copy.height = h;
        ctx = call.call(getContextHC, copy, '2d');
        getImageData = getImageData2D;
      }
      if (!ctx) return null;
      ctx.drawImage(canvas, 0, 0);  // works for 2d, webgl and bitmaprenderer sources
      var img = call.call(getImageData, ctx, 0, 0, w, h);
      noise(img.data);
      ctx.putImageData(img, 0, 0);
      return copy;
    }

    wrapMethod(C2D && C2D.prototype, 'getImageData', function (orig) {
      return function getImageData() {
        var img = apply.call(orig, this, arguments);
        try { noise(img.data); } catch (e) { /* ignore */ }
        return img;
      };
    });
    wrapMethod(O2D && O2D.prototype, 'getImageData', function (orig) {
      return function getImageData() {
        var img = apply.call(orig, this, arguments);
        try { noise(img.data); } catch (e) { /* ignore */ }
        return img;
      };
    });
    wrapMethod(HC && HC.prototype, 'toDataURL', function (orig) {
      return function toDataURL() {
        var copy = null;
        try { copy = noisedCopy(this, false); } catch (e) { copy = null; }
        return apply.call(orig, copy || this, arguments);
      };
    });
    wrapMethod(HC && HC.prototype, 'toBlob', function (orig) {
      return function toBlob() {
        var copy = null;
        try { copy = noisedCopy(this, false); } catch (e) { copy = null; }
        return apply.call(orig, copy || this, arguments);
      };
    });
    wrapMethod(OC && OC.prototype, 'convertToBlob', function (orig) {
      return function convertToBlob() {
        var copy = null;
        try { copy = noisedCopy(this, true); } catch (e) { copy = null; }
        return apply.call(orig, copy || this, arguments);
      };
    });
  } catch (e) { /* keep going */ }

  // ---- WebGL ----------------------------------------------------------------
  // WEBGL_debug_renderer_info exposes the exact GPU and driver. Report one
  // common Windows configuration instead of the real one (bucketing, not
  // noise: a random GPU string would itself be a fingerprint).
  try {
    var UNMASKED_VENDOR_WEBGL = 0x9245, UNMASKED_RENDERER_WEBGL = 0x9246;
    var vendor = 'Google Inc. (Intel)';
    var renderer = 'ANGLE (Intel, Intel(R) UHD Graphics 620 (0x00005917) Direct3D11 vs_5_0 ps_5_0, D3D11)';
    var glProtos = [];
    if (typeof WebGLRenderingContext !== 'undefined') glProtos.push(WebGLRenderingContext.prototype);
    if (typeof WebGL2RenderingContext !== 'undefined') glProtos.push(WebGL2RenderingContext.prototype);
    for (var g = 0; g < glProtos.length; g++) {
      wrapMethod(glProtos[g], 'getParameter', function (orig) {
        return function getParameter(pname) {
          if (pname === UNMASKED_VENDOR_WEBGL) return vendor;
          if (pname === UNMASKED_RENDERER_WEBGL) return renderer;
          return apply.call(orig, this, arguments);
        };
      });
    }
  } catch (e) { /* keep going */ }

  // ---- Web Audio ------------------------------------------------------------
  // Scale readback by a per-site factor just under 1. Inaudible, but it moves
  // every sum and hash the fingerprinting libraries compute.
  var fudged = new WeakSet();
  function fudgeArray(arr) {
    if (!arr || typeof arr.length !== 'number') return;
    for (var i = 0; i < arr.length; i++) arr[i] = arr[i] * audioFudge;
  }
  try {
    var AB = typeof AudioBuffer !== 'undefined' ? AudioBuffer.prototype : null;
    wrapMethod(AB, 'getChannelData', function (orig) {
      return function getChannelData() {
        var arr = apply.call(orig, this, arguments);
        // The same Float32Array comes back on every call: scale it once.
        if (arr && !fudged.has(arr)) { fudged.add(arr); fudgeArray(arr); }
        return arr;
      };
    });
    wrapMethod(AB, 'copyFromChannel', function (orig) {
      return function copyFromChannel(destination) {
        var r = apply.call(orig, this, arguments);
        fudgeArray(destination);
        return r;
      };
    });
    var AN = typeof AnalyserNode !== 'undefined' ? AnalyserNode.prototype : null;
    var analyserMethods = ['getFloatFrequencyData', 'getByteFrequencyData', 'getFloatTimeDomainData', 'getByteTimeDomainData'];
    for (var m = 0; m < analyserMethods.length; m++) {
      wrapMethod(AN, analyserMethods[m], function (orig) {
        return function (array) {
          var r = apply.call(orig, this, arguments);
          fudgeArray(array);
          return r;
        };
      });
    }
  } catch (e) { /* keep going */ }
})();
