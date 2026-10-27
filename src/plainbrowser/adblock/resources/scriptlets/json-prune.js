function pbJsonPrune(prunePaths = '', requiredPaths = '') {
  // json-prune: removes the space-separated dotted `prunePaths` ("[]" or "*" matches every array index or key)
  // from objects made by JSON.parse and Response.json(), when all `requiredPaths` exist in them ("" = always).
  // Copyright Plain Browser contributors. MPL-2.0.
  const prune = String(prunePaths).split(/\s+/).filter(Boolean);
  const required = String(requiredPaths).split(/\s+/).filter(Boolean);
  if (prune.length === 0) return;
  const walk = (obj, parts, onLeaf) => {
    if (obj === null || typeof obj !== 'object') return;
    const [head, ...rest] = parts;
    const keys = head === '[]' || head === '*' ? Object.keys(obj) : [head];
    for (const k of keys) {
      if (!(k in obj)) continue;
      if (rest.length === 0) onLeaf(obj, k);
      else walk(obj[k], rest, onLeaf);
    }
  };
  const has = (obj, path) => {
    let found = false;
    walk(obj, path.split('.'), () => { found = true; });
    return found;
  };
  const apply = (obj) => {
    try {
      if (!required.every((p) => has(obj, p))) return obj;
      for (const p of prune) walk(obj, p.split('.'), (o, k) => { delete o[k]; });
    } catch (e) {}
    return obj;
  };
  JSON.parse = new Proxy(JSON.parse, {
    apply(target, thisArg, args) { return apply(Reflect.apply(target, thisArg, args)); },
  });
  Response.prototype.json = new Proxy(Response.prototype.json, {
    apply(target, thisArg, args) { return Reflect.apply(target, thisArg, args).then(apply); },
  });
}
