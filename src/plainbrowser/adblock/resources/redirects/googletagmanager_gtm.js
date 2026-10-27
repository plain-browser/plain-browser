// Copyright Plain Browser contributors. MPL-2.0.
// Stand-in for Google Tag Manager's gtm.js ($redirect=googletagmanager_gtm.js): pages that push to dataLayer
// and wait for an eventCallback keep working, nothing is loaded or reported.
(function () {
  const w = window;
  w.google_tag_manager = w.google_tag_manager || { dataLayer: { gtmDom: true, gtmLoad: true } };
  const layer = w.dataLayer = w.dataLayer || [];
  const call = (entry) => {
    try {
      if (entry && typeof entry.eventCallback === 'function') setTimeout(entry.eventCallback, 1);
    } catch (e) {}
  };
  layer.forEach(call);
  const push = layer.push.bind(layer);
  layer.push = function (...entries) { entries.forEach(call); return push(...entries); };
})();
