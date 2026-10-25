# Plain Browser privacy policy

Last updated: 25 October 2026 · [Polski](PRIVACY.pl.md)

Plain Browser is a web browser built from Chromium with the ungoogled-chromium patch set. This policy says what the
browser does with your data. It describes version 0.1.0-alpha.1 and is updated whenever a release changes it.

## Summary

- Plain Browser has **no telemetry, no usage statistics and no crash reports** that are sent to us or anyone else.
- It has **no account, sign-in or sync service**, and we run **no server** that receives data from the browser.
  We cannot see what you browse.
- It does **not contact Google services**. The ungoogled-chromium patches remove them (Google sign-in, Safe
  Browsing, translation, the Chrome Web Store and others).
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
