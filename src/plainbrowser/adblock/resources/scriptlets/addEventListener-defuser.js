function pbAddEventListenerDefuser(type = '', pattern = '') {
  // addEventListener-defuser (aeld, prevent-addEventListener): listeners whose event type matches `type` and
  // whose source matches `pattern` are not added.
  // Copyright Plain Browser contributors. MPL-2.0.
  const typeP = pbPattern(type);
  const handlerP = pbPattern(pattern);
  const proto = EventTarget.prototype;
  const real = proto.addEventListener;
  proto.addEventListener = new Proxy(real, {
    apply(target, thisArg, args) {
      let handler = '';
      try {
        const h = args[1];
        handler = typeof h === 'function' ? Function.prototype.toString.call(h) : String(h);
      } catch (e) {}
      if (typeP.test(args[0]) && handlerP.test(handler)) return undefined;
      return Reflect.apply(target, thisArg, args);
    },
  });
}
