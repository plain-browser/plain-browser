function pbSetCookie(name = '', value = '', path = '', ...options) {
  // set-cookie: sets a first-party cookie to one of a few harmless values (consent answers mostly).
  // `path` is "/" by default ("none" leaves it out). Options "reload, 1": reload the page once if the cookie was
  // not set before, so the page reads it on load.
  // Copyright Plain Browser contributors. MPL-2.0.
  if (name === '' || /[;=\s]/.test(name)) return;
  const allowed = new Set(['true', 'false', 'yes', 'no', 'ok', 'on', 'off', 'accept', 'accepted', 'reject',
    'rejected', 'allow', 'allowed', 'deny', 'denied', '']);
  const v = String(value);
  if (!allowed.has(v.toLowerCase()) && !(/^\d+$/.test(v) && Number(v) <= 32767)) return;
  const before = document.cookie.split(/;\s*/).some((c) => c.split('=')[0] === name);
  let cookie = `${encodeURIComponent(name)}=${encodeURIComponent(v)}`;
  if (path !== 'none') cookie += `; path=${path === '' ? '/' : path}`;
  document.cookie = cookie;
  const reload = options.indexOf('reload');
  if (!before && reload !== -1 && options[reload + 1] === '1') {
    if (document.cookie.split(/;\s*/).some((c) => c.split('=')[0] === name)) location.reload();
  }
}
