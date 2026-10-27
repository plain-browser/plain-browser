function pbRemoveAttr(attrs = '', selector = '') {
  // remove-attr (ra): removes the "|"-separated attributes `attrs` from elements matching `selector` (by
  // default, any element that has one of them), now and as the page changes.
  // Copyright Plain Browser contributors. MPL-2.0.
  const names = String(attrs).split('|').map((s) => s.trim()).filter(Boolean);
  if (names.length === 0) return;
  const sel = selector !== '' ? selector : names.map((n) => `[${CSS.escape(n)}]`).join(',');
  pbOnDom(() => {
    for (const el of document.querySelectorAll(sel)) for (const n of names) el.removeAttribute(n);
  });
}
