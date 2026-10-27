function pbNoSetTimeoutIf(needle = '', delay = '') {
  // no-setTimeout-if (nostif, prevent-setTimeout): setTimeout calls whose callback source matches `needle` and
  // whose delay matches `delay` (a number, "!number", or "" for any) are not scheduled.
  // Copyright Plain Browser contributors. MPL-2.0.
  pbDefuseTimer('setTimeout', needle, delay);
}
