function pbSetSessionStorageItem(key = '', value = '') {
  // set-session-storage-item: as set-local-storage-item, for sessionStorage.
  // Copyright Plain Browser contributors. MPL-2.0.
  pbSetStorageItem('sessionStorage', key, value);
}
