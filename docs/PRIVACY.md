# Plain Browser privacy policy

Last updated: 19 November 2026 · [Polski](PRIVACY.pl.md)

Plain Browser is a web browser built from Chromium with the ungoogled-chromium patch set. This policy says what the
browser does with your data. It describes version 0.2.4-beta.2 and is updated whenever a release changes it.

## Summary

- Plain Browser has **no telemetry, no usage statistics and no crash reports** that are sent to us or anyone else.
- It has **no account, sign-in or sync service**, and we run **no server that receives data about the pages you
  visit**. We cannot see what you browse. The three services of ours the browser talks to (the Safe Browsing proxy,
  the filter list mirror and the extension repository, all below) are configured not to keep logs and are sent
  nothing about the pages you visit. Cloudflare, which serves them, still sees your IP address when you connect.
- It does **not contact Google directly**. The ungoogled-chromium patches remove Google sign-in, translation, the
  Chrome Web Store and other services. The one exception is Safe Browsing, which goes through our anonymizing proxy:
  Google receives hash prefixes of URLs from the proxy, never your IP address. You can turn it off.
- Your browsing data stays **on your device**.

## What we collect

Nothing. We do not receive, collect, store or sell personal information. There is no analytics, advertising
identifier or tracking code in the browser.

We measured an earlier build: with a fresh profile and no page open, it made no network requests of its own over
100 seconds, apart from the Windows proxy auto-detection (WPAD) any application using the system proxy settings
performs. The current version makes its first requests of its own a few minutes after start: the filter list check
(about two minutes after start), Safe Browsing list updates through our proxy, and DNS lookups, encrypted when your
resolver supports it. You can turn off each of the first two in Settings.

## Data stored on your device

Like every browser, Plain Browser stores your browsing history, cookies, cache, saved passwords, bookmarks,
downloads and settings in your Windows user profile. This data is not sent to us. You can delete it in the browser
settings (Clear browsing data), or remove it entirely by uninstalling the browser. In the Microsoft Store version,
Windows keeps the profile in the app's private storage and removes it on uninstall.

The ad blocker also keeps on your device: your own filters (the "My filters" box on chrome://adblock, stored in the
browser's settings file), the list of sites where you turned protections off, the filter lists it downloaded (in a
folder inside your profile) and, for each list update, the highest manifest number it has accepted, so that an older
copy cannot be replayed. None of it is sent to us. Turning list updates off deletes the downloaded lists; Clear
browsing data does not remove your own filters or the site exceptions, which you can edit in Settings and on
chrome://adblock.

## Data sent to the websites you visit

When you open a website, the browser sends it what is needed to load the page, including your IP address and
standard request headers. That is how the web works, and the website's own privacy policy applies to it.

The browser also tells every website that you do not want your data sold or shared, with the **Global Privacy
Control** signal (the `Sec-GPC: 1` request header and `navigator.globalPrivacyControl`). Under laws such as the
CCPA in California, sites must honour it; elsewhere it is a request. It is on by default and can be turned off in
Settings > Privacy and security > Third-party cookies and site data > "Send an opt-out request to sites regarding
use of your data".

Plain Browser also changes what some websites would otherwise learn about you, entirely on your device. It removes
known tracking parameters (such as `utm_*`, `fbclid`, `gclid`) from the addresses you open, so the site never
receives them, and it skips known tracking redirectors (such as `l.facebook.com/l.php`) and goes straight to the
page you meant. Fingerprint protection adds small, per-site and per-session variations to canvas, audio and WebGL
output and to a few browser values, so scripts cannot use them to recognise you across sites. Both can be turned off
for a site that breaks, from the shield button in the toolbar. Fingerprint protection is a first version: a page can
detect and bypass it, and it does not cover every fingerprinting method.

The built-in ad blocker runs entirely on your device. Its filter lists (EasyList, EasyPrivacy, EasyList Polish, the
EasyList Cookie List, and Fanboy's Annoyance List and uBlock filters if you turn them on) are bundled with the
browser and, unless you turn updates off, kept current from our mirror (next section). The blocker does not report
to anyone what it blocks. Its request log on chrome://adblock records only while that page is open, stays in
memory and is cleared when the page closes.

## Secure DNS

To find a website, the browser asks a DNS resolver for its address. Plain Browser uses the resolver your network
or Windows is already set to use. When that resolver also offers an encrypted DNS-over-HTTPS endpoint (for
example Cloudflare or Quad9), the browser sends its lookups there encrypted instead of in plain text; otherwise it
asks the resolver as before. No other DNS provider is added and we do not receive your lookups. You can turn this
off, or choose a provider, in Settings > Privacy and security > Security > "Use secure DNS".

## Filter list updates

From version 0.1.3-alpha.1, the browser keeps its filter lists current between releases:

- Once a day, a few minutes after start-up, it asks **our mirror at lists.plain-browser.app** (a Cloudflare
  Worker) for a short manifest with the hash of each list, and downloads a list only when it changed (and an
  optional list only while it is turned on). It never
  contacts the list publishers; the mirror fetches the lists from them.
- The requests carry **no cookies and nothing about the sites you visit**, only the browser's version number
  (`X-Client: PlainBrowser/<major.minor.patch>`, the same header the Safe Browsing proxy gets). The browser checks
  each downloaded list's size and SHA-256 hash against the manifest before using it. It also refuses a manifest that
  is older than one it already accepted or past its expiry date, and keeps its current lists instead.
- **The mirror keeps no logs** and stores only the lists. Cloudflare handles the connection and can see your IP
  address the way it can for any site it serves.
- **You can turn it off**: Settings > Privacy and security > Update filter lists daily. With it off, the browser
  makes no such requests, deletes the downloaded lists and uses the lists it was released with.

## Safe Browsing

Safe Browsing warns before a page known for malware or phishing opens. Plain Browser offers it
without Google learning anything about you:

- **Active since version 0.1.2-alpha.1**, in standard protection only. The browser sends Safe Browsing requests to our
  proxy and to nothing else; it never contacts Google directly.
- The browser keeps a **local database of hash prefixes** (the "Update API", standard protection). URLs you visit
  are checked against it on your device. Only when a prefix matches does the browser ask for the full hashes of
  that prefix. That request carries a short prefix of the hash, not the URL, but a prefix does narrow down which
  addresses it could be, so treat it as partial information, not as none.
- Those requests go to **our proxy at sb.plain-browser.app**, a Cloudflare Worker, never to Google directly. The
  proxy forwards them to Google with our API key and strips everything that identifies you: your IP address,
  cookies, `Referer`, `Accept-Language` and Cloudflare's own headers. Google sees Cloudflare's IP address, a fixed
  `User-Agent` and the hash prefixes.
- The only thing the browser adds is the header `X-Client: PlainBrowser/<version>`. No install ID, user ID or
  Chromium build number is sent.
- **The proxy does not store requests.** Request logging and observability are switched off and it has no storage.
  The Worker does process the hash prefixes in transit, in memory, to forward them to Google, so this is a
  statement about what it is configured and written to do (the code is in `infra/safebrowsing-proxy`), not
  something you can verify from outside. Cloudflare still handles the connection and can see your IP address the way
  any website's host does; its own privacy policy applies to that.
- Enhanced Protection, real-time URL lookups and extended reporting, which would send full URLs and page
  content, are hidden in Settings and have no working endpoint: our proxy serves only the hash-prefix lists and the
  other addresses are disabled. They could still be switched on through an enterprise policy or the preferences
  file, but by design nothing would be sent to Google (this has not been tested).
- The protection covers **website addresses** (malware, phishing, unwanted software). It does not check downloaded
  files or extensions: the lists for those cannot be fetched with a public API key and are not requested.
- You can turn Safe Browsing off in the settings (You and Google > Google services, or Privacy and security >
  Security); with it off, the browser makes no requests to our proxy.

## Extensions

Extensions you install can read and send data according to the permissions you grant them. Their developers, not
Plain Browser, are responsible for that data and for their own privacy policies.

The page **chrome://recommended-extensions** offers uBlock Origin, AdGuard and NoScript, which we host at
**ext.plain-browser.app** (a Cloudflare Worker) because the Chrome Web Store is not part of the browser:

- Nothing is downloaded until you click Install (or Switch). The download carries no cookies, only the browser's
  version (`X-Client`).
- Once one of them is installed, Chromium's extension updater checks ext.plain-browser.app for a new version
  of it every few hours, like Chrome checks the Web Store. That request contains what Chromium puts in every update
  check: the IDs and versions of those extensions, the browser version, the operating system and its architecture,
  and the browser language. It contains nothing about the sites you visit. If you remove them, the checks stop.
- **The repository keeps no logs** and stores only the extension files. Cloudflare sees your IP address as for any
  site it serves.

## Microsoft Store

If you install Plain Browser from the Microsoft Store, Microsoft processes your installation and update
information under [Microsoft's privacy statement](https://privacy.microsoft.com/privacystatement). We receive only
aggregate figures from Microsoft Partner Center (for example, number of acquisitions), not information about you.

## Children

Plain Browser is a general-purpose browser. It does not collect any personal information, including from children.
It does not filter web content, so parental controls should be set up in Windows.

## Changes and contact

If this policy changes, the new text is published here with a new date. Questions and requests:
https://github.com/plain-browser/plain-browser/issues
