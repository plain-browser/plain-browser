# Microsoft Store listing text

Paste into Partner Center → Store listings. Limits: short description 100 characters, description 10,000,
features 20 × 200 characters, search terms 7 × 30 characters. Version 0.1.0-alpha.1 is an early test build; the
text says so and claims only what has been checked.

## English (en-us)

**Product name:** Plain Browser

**Short description (100):**
A plain Chromium browser: no Google services, built-in ad blocking, no tracking.

**Description:**
Plain Browser is a fast, familiar Chromium-based web browser without the extras. It has no Google account, no
sync, no telemetry and no usage reporting. Nothing about what you browse leaves your computer. The one exception
is Safe Browsing, which goes through our own proxy that hides your IP address and never sees the sites you visit;
you can turn it off in the settings.

What you get:

• Built-in ad and tracker blocking. The blocker is a fast Rust engine (adblock-rust, the same engine used by Brave)
  with EasyList, EasyPrivacy and the Polish EasyList bundled in. It runs on your device and reports nothing.
• No Google services. Google sign-in, sync, telemetry and the other Google components are removed, based on the
  ungoogled-chromium project. The one Google service kept is Safe Browsing: sites are checked against a list on
  your device, and the list updates go through our proxy at sb.plain-browser.app, so Google never sees your IP
  address or the sites you visit. A switch in the settings turns it off.
• One toggle for the current site: the toolbar button turns ad blocking, tracking-parameter stripping and
  fingerprint protection off for a site that breaks, and back on.
• Chromium's engine and speed: modern web standards, tabs, bookmarks, downloads, the PDF viewer and developer tools.
• Manifest V2 extensions stay enabled in this build (experimental; not every extension has been tested).
• Private by design: your history, cookies and passwords stay in your profile on your device.
• Free and open source. Our code is MPL-2.0; the full source and licenses are on GitHub.

This is an early alpha. Expect rough edges: the ad blocker has no global on/off setting yet (only the per-site
toggle), and some features of larger browsers are missing (for example sync and a built-in extension store). Extensions can be
installed manually. Bug reports are welcome on GitHub.

Plain Browser is not affiliated with Google or the Chromium project. Chromium is a project of The Chromium Authors.

**What's new in this version:**
First public alpha: Plain Browser branding, built-in ad blocking with a per-site toggle, Safe Browsing through our
anonymizing proxy, Polish and English interface, Chromium 156.

**Product features (up to 20):**
- Built-in ad and tracker blocking (adblock-rust, EasyList, EasyPrivacy)
- Per-site toggle for all protections in the toolbar
- No Google services, no account, no telemetry
- Safe Browsing through our proxy: Google never sees your IP address; can be turned off
- Chromium engine: modern web standards, PDF viewer, developer tools
- Manifest V2 extensions stay enabled (experimental)
- Your data stays on your device
- Free and open source (MPL-2.0)

**Search terms (7):** browser, web browser, ad blocker, privacy, chromium, no tracking, open source

**Copyright and trademark info:** © 2026 Plain Browser contributors. Based on Chromium, © The Chromium Authors.
Chromium, Google and other names belong to their owners.

**Additional license terms:** none beyond the open source licenses listed in chrome://credits.

**Website:** https://github.com/plain-browser/plain-browser
**Support contact:** https://github.com/plain-browser/plain-browser/issues
**Privacy policy:** https://github.com/plain-browser/plain-browser/blob/main/docs/PRIVACY.md

## Polski (pl-pl)

**Nazwa produktu:** Plain Browser

**Krótki opis (100):**
Zwykła przeglądarka na Chromium: bez usług Google, z wbudowanym blokowaniem reklam, bez śledzenia.

**Opis:**
Plain Browser to szybka, znajoma przeglądarka oparta na Chromium, bez zbędnych dodatków. Nie ma konta Google,
synchronizacji, telemetrii ani raportowania użycia. Nic z tego, co przeglądasz, nie opuszcza Twojego komputera.
Jedynym wyjątkiem jest Safe Browsing, który przechodzi przez nasze własne proxy ukrywające Twój adres IP i
niewidzące odwiedzanych stron; możesz go wyłączyć w ustawieniach.

Co dostajesz:

• Wbudowane blokowanie reklam i trackerów. Bloker to szybki silnik w Rust (adblock-rust, ten sam, którego używa
  Brave) z dołączonymi listami EasyList, EasyPrivacy i polską EasyList. Działa na Twoim urządzeniu i niczego nie
  zgłasza.
• Bez usług Google. Logowanie Google, synchronizacja, telemetria i pozostałe komponenty Google są usunięte, na
  bazie projektu ungoogled-chromium. Jedyną zachowaną usługą Google jest Safe Browsing: strony są sprawdzane w
  liście na Twoim urządzeniu, a aktualizacje listy idą przez nasze proxy sb.plain-browser.app, więc Google nigdy
  nie widzi Twojego adresu IP ani odwiedzanych stron. Przełącznik w ustawieniach pozwala to wyłączyć.
• Jeden przełącznik dla bieżącej witryny: przycisk na pasku narzędzi wyłącza blokowanie reklam, obcinanie
  parametrów śledzących i ochronę odcisku palca dla strony, która nie działa, i włącza je z powrotem.
• Silnik i szybkość Chromium: nowoczesne standardy sieci, karty, zakładki, pobieranie, przeglądarka PDF i narzędzia
  deweloperskie.
• Rozszerzenia Manifest V2 pozostają włączone w tej wersji (eksperymentalnie; nie każde rozszerzenie było testowane).
• Prywatność z założenia: historia, ciasteczka i hasła zostają w Twoim profilu na Twoim urządzeniu.
• Darmowa i otwartoźródłowa. Nasz kod ma licencję MPL-2.0, a pełny kod źródłowy i licencje są na GitHubie.

To wczesna wersja alfa. Mogą zdarzać się niedociągnięcia: bloker reklam nie ma jeszcze globalnego wyłącznika
(tylko przełącznik dla witryny), a brakuje niektórych funkcji większych przeglądarek (np. synchronizacji i wbudowanego sklepu z
rozszerzeniami). Rozszerzenia można instalować ręcznie. Zgłoszenia błędów są mile widziane na GitHubie.

Plain Browser nie jest powiązany z Google ani z projektem Chromium. Chromium to projekt The Chromium Authors.

**Co nowego w tej wersji:**
Pierwsza publiczna alfa: marka Plain Browser, wbudowane blokowanie reklam z przełącznikiem dla witryny, Safe
Browsing przez nasze anonimizujące proxy, interfejs po polsku i angielsku, Chromium 156.

**Funkcje produktu:**
- Wbudowane blokowanie reklam i trackerów (adblock-rust, EasyList, EasyPrivacy)
- Przełącznik wszystkich ochron dla bieżącej witryny na pasku narzędzi
- Bez usług Google, bez konta, bez telemetrii
- Safe Browsing przez nasze proxy: Google nigdy nie widzi Twojego adresu IP; można wyłączyć
- Silnik Chromium: nowoczesne standardy, przeglądarka PDF, narzędzia deweloperskie
- Rozszerzenia Manifest V2 pozostają włączone (eksperymentalnie)
- Twoje dane zostają na Twoim urządzeniu
- Darmowa i otwartoźródłowa (MPL-2.0)

**Słowa kluczowe:** przeglądarka, blokowanie reklam, prywatność, chromium, bez śledzenia, open source, adblock

**Informacje o prawach autorskich i znakach towarowych:** © 2026 Plain Browser contributors. Oparta na Chromium,
© The Chromium Authors. Chromium, Google i inne nazwy należą do ich właścicieli.

## Pricing and availability

- **Price:** Free.
- **Markets:** all available markets (or start with Poland and the EU if you prefer a slower rollout).
- **Visibility:** Public, with "Don't publish until I select Publish now" under Submission Options.
- **Free trial, sale, discounts:** none.
