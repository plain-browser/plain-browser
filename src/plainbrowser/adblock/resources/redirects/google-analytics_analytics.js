// Copyright Plain Browser contributors. MPL-2.0.
// Stand-in for analytics.js: `ga()` exists, accepts every call and runs hitCallback, sends nothing.
(function () {
  const w = window;
  const name = w.GoogleAnalyticsObject || 'ga';
  const queue = (w[name] && w[name].q) || [];
  const tracker = { get() {}, set() {}, send() {} };
  const ga = function (...args) {
    const last = args[args.length - 1];
    try {
      if (last && typeof last === 'object' && typeof last.hitCallback === 'function') setTimeout(last.hitCallback, 1);
      if (typeof args[0] === 'function') args[0](tracker);
    } catch (e) {}
  };
  ga.create = () => tracker;
  ga.getByName = () => tracker;
  ga.getAll = () => [tracker];
  ga.remove = () => {};
  ga.loaded = true;
  w[name] = ga;
  queue.forEach((args) => ga(...args));
})();
