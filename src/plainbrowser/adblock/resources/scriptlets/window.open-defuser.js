function pbWindowOpenDefuser(pattern = '') {
  // window.open-defuser (nowoif, no-window-open-if, prevent-window-open): window.open() calls whose URL matches
  // `pattern` ("" for all, "!..." for all others) open nothing and return null.
  // Copyright Plain Browser contributors. MPL-2.0.
  const p = pbPattern(pattern);
  window.open = new Proxy(window.open, {
    apply(target, thisArg, args) {
      if (p.test(args.length ? String(args[0]) : '')) return null;
      return Reflect.apply(target, thisArg, args);
    },
  });
}
