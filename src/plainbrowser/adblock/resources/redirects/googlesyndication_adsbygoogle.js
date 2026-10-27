// Copyright Plain Browser contributors. MPL-2.0.
// Stand-in for adsbygoogle.js: `adsbygoogle.push()` is accepted and does nothing.
(function () {
  const w = window;
  const queue = Array.isArray(w.adsbygoogle) ? w.adsbygoogle : [];
  w.adsbygoogle = { loaded: true, push() {}, length: 0 };
  queue.length = 0;
})();
