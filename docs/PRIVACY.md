# Plain Browser privacy policy

Last updated: 25 October 2026 · [Polski](PRIVACY.pl.md)

Plain Browser is a web browser built from Chromium with the ungoogled-chromium patch set. This policy says what the
browser does with your data. It describes version 0.1.0-alpha.1 and is updated whenever a release changes it.

## Summary

- Plain Browser has **no telemetry, no usage statistics and no crash reports** that are sent to us or anyone else.
- It has **no account, sign-in or sync service**, and we run **no server** that receives data from the browser.
  We cannot see what you browse.
- It does **not contact Google services**. The ungoogled-chromium patches remove them (Google sign-in,
  translation, the Chrome Web Store and others). The one planned exception is Safe Browsing through our own
  anonymizing proxy, described below; it is not active in this version.
- Your browsing data stays **on your device**.

## What we collect

Nothing. We do not receive, collect, store or sell personal information. There is no analytics, advertising
identifier or tracking code in the browser.

We checked this for the first start of the browser: with a fresh profile and no page open, Plain Browser made no
network requests of its own over 100 seconds. The only network activity was the Windows proxy auto-detection
(WPAD) that any application using the system proxy settings performs.

## Data stored on your device

Like every browser, Plain Browser stores your browsing history, cookies, cache, saved passwords, bookmarks,
downloads and settings in your Windows user profile. This data is not sent to us. You can delete it in the browser
settings (Clear browsing data), or remove it entirely by uninstalling the browser. In the Microsoft Store version,
Windows keeps the profile in the app's private storage and removes it on uninstall.

## Data sent to the websites you visit

When you open a website, the browser sends it what is needed to load the page, including your IP address and
standard request headers. That is how the web works, and the website's own privacy policy applies to it.

The built-in ad blocker runs entirely on your device. Its filter lists (EasyList, EasyPrivacy, EasyList Polish) are
bundled with the browser. The blocker does not download lists, and it does not report to anyone what it blocks.

## Safe Browsing

Safe Browsing warns before a page known for malware or phishing opens. Plain Browser is designed to offer it
without Google learning anything about you:

- **Not active in version 0.1.0-alpha.1.** Safe Browsing is compiled out of this build, so the browser makes no
  Safe Browsing requests at all. The paragraphs below describe the design that a future version will ship; this
  policy will be updated when it does.
- The browser keeps a **local database of hash prefixes** (the "Update API", standard protection). URLs you visit
  are checked against it on your device. Only when a prefix matches does the browser ask for the full hashes of
  that prefix, which does not reveal the URL.
- Those requests go to **our proxy at sb.plain-browser.app**, a Cloudflare Worker, never to Google directly. The
  proxy forwards them to Google with our API key and strips everything that identifies you: your IP address,
  cookies, `Referer`, `Accept-Language` and Cloudflare's own headers. Google sees Cloudflare's IP address, a fixed
  `User-Agent` and the hash prefixes.
- The only thing the browser adds is the header `X-Client: PlainBrowser/<version>`. No install ID, user ID or
  Chromium build number is sent.
- **The proxy keeps no logs.** Request logging and observability are switched off and it has no storage. We cannot
  see which prefixes you asked about. Cloudflare still handles the connection and can see your IP address the way
  any website's host does; its own privacy policy applies to that.
- Enhanced Protection, real-time URL lookups and extended reporting, which would send full URLs and page
  content, are disabled and cannot be turned on.
- You will be able to turn Safe Browsing off in the settings; with it off, the browser makes no requests to our
  proxy.

## Extensions

Extensions you install can read and send data according to the permissions you grant them. Their developers, not
Plain Browser, are responsible for that data and for their own privacy policies.

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
