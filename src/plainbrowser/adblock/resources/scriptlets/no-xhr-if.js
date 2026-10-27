function pbNoXhrIf(props = '') {
  // no-xhr-if (prevent-xhr): XMLHttpRequests whose URL/method match `props` (as in no-fetch-if) complete at once
  // with an empty 200 response and never reach the network.
  // Copyright Plain Browser contributors. MPL-2.0.
  const needles = [];
  for (const token of String(props).split(/\s+/).filter(Boolean)) {
    const i = token.indexOf(':');
    const key = i > 0 && /^[a-z]+$/i.test(token.slice(0, i)) ? token.slice(0, i) : 'url';
    needles.push([key, pbPattern(key === 'url' && i <= 0 ? token : token.slice(i + 1))]);
  }
  const blocked = new WeakSet();
  const proto = XMLHttpRequest.prototype;
  const realOpen = proto.open;
  const realSend = proto.send;
  proto.open = function (method, url, ...rest) {
    const details = { method: String(method), url: String(url) };
    if (needles.every(([key, p]) => key in details && p.test(details[key]))) blocked.add(this);
    else blocked.delete(this);
    return realOpen.call(this, method, url, ...rest);
  };
  proto.send = function (...args) {
    if (!blocked.has(this)) return realSend.apply(this, args);
    const xhr = this;
    const fake = { readyState: 4, status: 200, statusText: 'OK', responseText: '', response: '', responseURL: '' };
    for (const [k, v] of Object.entries(fake)) {
      try { Object.defineProperty(xhr, k, { configurable: true, get: () => v }); } catch (e) {}
    }
    setTimeout(() => {
      for (const type of ['readystatechange', 'load', 'loadend']) xhr.dispatchEvent(new Event(type));
    }, 0);
  };
}
