// Copyright Plain Browser contributors. MPL-2.0.
// Shared by set-local-storage-item and set-session-storage-item.
function pbSetStorageItem(storage, key, value) {
  if (key === '') return;
  const allowed = new Set(['undefined', 'false', 'true', 'null', '', 'emptyArr', 'emptyObj', 'yes', 'no', 'on', 'off',
    'accept', 'accepted', 'reject', 'rejected', 'allow', 'deny', '$remove$']);
  if (!allowed.has(value) && !(/^\d+$/.test(value) && Number(value) <= 32767)) return;
  try {
    const store = window[storage];
    if (value === '$remove$') {
      const p = /^\/.+\/[a-z]*$/.test(key) ? pbPattern(key) : null;
      if (p === null) {
        store.removeItem(key);
      } else {
        for (const k of Object.keys(store)) if (p.test(k)) store.removeItem(k);
      }
      return;
    }
    const v = value === 'emptyArr' ? '[]' : value === 'emptyObj' ? '{}' : value;
    store.setItem(key, v);
  } catch (e) {}
}
