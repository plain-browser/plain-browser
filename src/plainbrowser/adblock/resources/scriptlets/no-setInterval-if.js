function pbNoSetIntervalIf(needle = '', delay = '') {
  // no-setInterval-if (nosiif, prevent-setInterval): as no-setTimeout-if, for setInterval.
  // Copyright Plain Browser contributors. MPL-2.0.
  pbDefuseTimer('setInterval', needle, delay);
}
