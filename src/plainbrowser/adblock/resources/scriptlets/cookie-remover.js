function pbCookieRemover(needle = '') {
  // cookie-remover (remove-cookie): deletes first-party cookies whose name matches `needle` ("" for all), now
  // and when the page is hidden or unloaded.
  // Copyright Plain Browser contributors. MPL-2.0.
  const p = pbPattern(needle);
  const remove = () => {
    for (const c of document.cookie.split(/;\s*/)) {
      const name = c.split('=')[0];
      if (name === '' || !p.test(name)) continue;
      const expire = `${name}=; expires=Thu, 01 Jan 1970 00:00:00 GMT`;
      const host = location.hostname;
      const parts = host.split('.');
      document.cookie = `${expire}; path=/`;
      for (let i = 0; i < parts.length - 1; i++) {
        document.cookie = `${expire}; path=/; domain=.${parts.slice(i).join('.')}`;
      }
    }
  };
  remove();
  window.addEventListener('beforeunload', remove);
  document.addEventListener('visibilitychange', remove);
}
