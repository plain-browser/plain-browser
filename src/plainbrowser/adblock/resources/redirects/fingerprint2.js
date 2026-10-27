// Copyright Plain Browser contributors. MPL-2.0.
// Stand-in for fingerprintjs2: the API exists and every fingerprint is the same constant.
(function () {
  const hash = '74656d70';
  const components = [{ key: 'pb', value: 'pb' }];
  const Fingerprint2 = function () {};
  Fingerprint2.get = function (options, done) {
    const cb = typeof options === 'function' ? options : done;
    setTimeout(() => { if (cb) cb(components); }, 1);
  };
  Fingerprint2.getPromise = () => Promise.resolve(components);
  Fingerprint2.getV18 = function (options, done) {
    const cb = typeof options === 'function' ? options : done;
    setTimeout(() => { if (cb) cb(hash, components); }, 1);
  };
  Fingerprint2.x64hash128 = () => hash;
  Fingerprint2.prototype.get = function (done) { setTimeout(() => done(hash, components), 1); };
  window.Fingerprint2 = Fingerprint2;
})();
