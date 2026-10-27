function pbSetLocalStorageItem(key = '', value = '') {
  // set-local-storage-item: sets localStorage[key] to one of a few harmless values, or removes it ("$remove$";
  // `key` may then be a /regex/).
  // Copyright Plain Browser contributors. MPL-2.0.
  pbSetStorageItem('localStorage', key, value);
}
