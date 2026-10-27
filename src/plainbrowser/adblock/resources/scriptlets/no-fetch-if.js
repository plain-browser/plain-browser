function pbNoFetchIf(props = '') {
  // no-fetch-if (prevent-fetch): fetch() calls matching `props` resolve to an empty 200 response instead of
  // reaching the network. `props` is a space-separated list of "key:pattern" (url, method, ...) or a bare
  // pattern for the URL; all must match. "" matches every call.
  // Copyright Plain Browser contributors. MPL-2.0.
  const needles = [];
  for (const token of String(props).split(/\s+/).filter(Boolean)) {
    const i = token.indexOf(':');
    const key = i > 0 && /^[a-z]+$/i.test(token.slice(0, i)) ? token.slice(0, i) : 'url';
    needles.push([key, pbPattern(key === 'url' && i <= 0 ? token : token.slice(i + 1))]);
  }
  const realFetch = window.fetch;
  window.fetch = new Proxy(realFetch, {
    apply(target, thisArg, args) {
      let details;
      try {
        const [input, init] = args;
        details = { url: input instanceof Request ? input.url : String(input), method: 'GET' };
        if (input instanceof Request) details.method = input.method;
        if (init && typeof init === 'object') {
          for (const k of Object.keys(init)) details[k] = init[k];
        }
      } catch (e) {
        return Reflect.apply(target, thisArg, args);
      }
      const match = needles.every(([key, p]) => key in details && p.test(details[key]));
      if (!match) return Reflect.apply(target, thisArg, args);
      return Promise.resolve(new Response('', { status: 200, statusText: 'OK' }));
    },
  });
}
