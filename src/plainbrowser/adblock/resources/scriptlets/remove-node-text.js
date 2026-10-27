function pbRemoveNodeText(nodeName = '', needle = '') {
  // remove-node-text (rmnt): empties elements named `nodeName` (e.g. "script") whose text matches `needle`, as
  // the parser adds them, so an inline script is gone before it runs.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (nodeName === '' || needle === '') return;
  const name = pbPattern(nodeName);
  const text = pbPattern(needle);
  const handle = (node) => {
    if (node.nodeType !== 1 || !name.test(node.nodeName.toLowerCase())) return;
    if (text.test(node.textContent)) node.textContent = '';
  };
  const observer = new MutationObserver((records) => {
    for (const r of records) for (const n of r.addedNodes) handle(n);
  });
  observer.observe(document, { childList: true, subtree: true });
  document.querySelectorAll('*').forEach(handle);
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => observer.disconnect(), { once: true });
  }
}
