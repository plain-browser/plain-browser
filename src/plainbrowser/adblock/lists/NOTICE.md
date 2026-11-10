# Bundled filter lists

Default lists compiled into the browser (rs/src/lib.rs, `include_str!`), downloaded from the URLs below. Snapshot: 2026-10-23 for the three default lists, 2026-11-09 for the optional ones.

| File | List | Source | License |
|---|---|---|---|
| easylist.txt | EasyList | https://easylist.to/easylist/easylist.txt | GPLv3 or CC BY-SA 3.0 (https://easylist.to/pages/licence.html) |
| easyprivacy.txt | EasyPrivacy | https://easylist.to/easylist/easyprivacy.txt | GPLv3 or CC BY-SA 3.0 |
| easylistpolish.txt | EasyList Polish | https://easylist-downloads.adblockplus.org/easylistpolish.txt | GPLv3 or CC BY-SA 3.0 |
| easylistcookie.txt | EasyList Cookie List (optional, on by default) | https://secure.fanboy.co.nz/fanboy-cookiemonster.txt | GPLv3 or CC BY-SA 3.0 (the file's header says CC BY 3.0) |
| fanboyannoyance.txt | Fanboy's Annoyance List (optional, off by default) | https://easylist.to/easylist/fanboy-annoyance.txt | GPLv3 or CC BY-SA 3.0 (the file's header says CC BY 3.0) |
| ublock/ublockfilters.txt | uBlock filters (optional, off by default) | https://ublockorigin.github.io/uAssets/filters/filters.txt | GPLv3, ublock/LICENSE |
| plainbrowser-removeparam.txt | Plain Browser tracking parameters (`$removeparam`) | this repository | MPL-2.0 |

The lists are data, distributed unmodified; credit the EasyList authors in chrome://credits.

Between releases the browser replaces the first three, and the optional lists that are on, with newer copies of the
same lists, unmodified, served by our mirror lists.plain-browser.app (`infra/filter-lists`); the bundled copies stay
the fallback. The optional lists are turned on and off in Settings > Privacy and security.
