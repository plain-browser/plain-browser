// Copyright Plain Browser contributors. MPL-2.0.
// Stand-in for Chartbeat's chartbeat.js: the pSUPERFLY API exists and does nothing.
(function () {
  const noop = function () {};
  window.pSUPERFLY = { activity: noop, virtualPage: noop };
})();
