export type Lang = "en" | "pl";

export const prefix = (lang: Lang) => (lang === "pl" ? "/pl" : "");

export type Dict = {
  htmlLang: string;
  metaTitle: string;
  metaDescription: string;
  nav: { download: string; privacy: string; security: string; source: string; switchLabel: string; switchHref: string };
  hero: {
    badge: string;
    title: string;
    lead: string;
    download: string;
    downloadNote: string;
    source: string;
  };
  mock: { tab: string; url: string; blocked: string; placeholder: string };
  pillars: { title: string; items: { title: string; body: string }[] };
  nothing: { title: string; lead: string; items: string[]; proof: string };
  built: { title: string; lead: string; layers: { name: string; note: string }[]; licenses: string };
  status: { title: string; lead: string; done: string[]; todo: string[] };
  faq: { title: string; items: { q: string; a: string }[] };
  cta: { title: string; body: string; download: string; issues: string };
  download: {
    title: string;
    lead: string;
    version: string;
    requires: string;
    installer: string;
    installerNote: string;
    zip: string;
    zipNote: string;
    sums: string;
    store: string;
    storeNote: string;
    msixNote: string;
    published: string;
    releaseNotes: string;
    unsignedTitle: string;
    unsignedBody: string;
    verifyTitle: string;
    verifyBody: string;
    allReleases: string;
    privacyNote: string;
  };
  footer: { tagline: string; disclaimer: string; privacy: string; security: string; source: string; issues: string };
  notFound: { title: string; body: string; home: string };
};

const en: Dict = {
  htmlLang: "en",
  metaTitle: "Plain Browser",
  metaDescription:
    "A plain Chromium browser for Windows: no Google services, built-in ad and tracker blocking, Manifest V2 extensions kept alive. Free and open source.",
  nav: { download: "Download", privacy: "Privacy", security: "Security", source: "Source", switchLabel: "Polski", switchHref: "/pl/" },
  hero: {
    badge: "Early alpha · Windows 11",
    title: "A browser. Nothing else.",
    lead:
      "Plain Browser is Chromium with the Google parts removed, an ad and tracker blocker built into the network stack, and Manifest V2 extensions left switched on. No account, no sync, no telemetry, no server of ours that could receive your data.",
    download: "Download for Windows",
    downloadNote: "Unsigned alpha build. Expect rough edges.",
    source: "Source on GitHub",
  },
  mock: { tab: "New tab", url: "plain-browser.app", blocked: "14 requests blocked", placeholder: "Search or type a URL" },
  pillars: {
    title: "What you get",
    items: [
      {
        title: "Blocking that lives in the browser",
        body:
          "The blocker is adblock-rust, the engine Brave uses, compiled into the network layer. It does not depend on extension APIs, runs on your device and reports nothing. EasyList, EasyPrivacy and EasyList Polish are bundled in and updated daily from our own mirror, so the browser never contacts the list publishers.",
      },
      {
        title: "No Google services",
        body:
          "Everything the ungoogled-chromium project strips stays stripped: sign-in, sync, translation, the Web Store, usage reporting. The one service we keep, Safe Browsing, goes through our own proxy that hides you from Google, and a switch in the settings turns it off.",
      },
      {
        title: "Manifest V2 stays alive",
        body:
          "Chrome dropped MV2 and the Web Store removed those extensions. Plain Browser keeps MV2 and blocking webRequest working, so tools like uBlock Origin keep their full power. Tested with uBlock Origin 1.75.0.",
      },
      {
        title: "Still Chromium",
        body:
          "Same engine, same speed, same tabs, bookmarks, downloads, PDF viewer and developer tools you already know. Sites work the way they do everywhere else.",
      },
      {
        title: "Tracking cleanup and a per-site switch",
        body:
          "Tracking parameters (utm_, fbclid, gclid and others) are stripped from links, and canvas, audio and WebGL fingerprinting is blunted, with a built-in exception list for banks. One toolbar button turns all of it, and the blocker, off for a site that breaks.",
      },
      {
        title: "Open source, small patch set",
        body:
          "Our code sits in one directory on top of ungoogled-chromium, with a short list of patches, so each Chromium release can be followed within days. MPL-2.0 and BSD-3-Clause.",
      },
    ],
  },
  nothing: {
    title: "What it does not do",
    lead: "The privacy policy fits in a sentence: we collect nothing, because there is nothing that could.",
    items: [
      "No telemetry, usage statistics or crash reports",
      "No account, sign-in or sync",
      "No analytics or advertising identifier in the browser",
      "No filter list downloads: the lists ship inside the binary",
      "No contact with Google services",
      "Safe Browsing only through our no-log proxy, with the option to turn it off",
    ],
    proof:
      "Checked on first start: a fresh profile with no page open made no network requests of its own for 100 seconds. The only traffic was Windows proxy auto-detection, which every application using system proxy settings performs.",
  },
  built: {
    title: "How it is built",
    lead: "Plain Browser is a thin layer. Each release of Chromium flows through the stack below, which is why security fixes can arrive within days of an upstream stable tag.",
    layers: [
      { name: "Chromium", note: "pinned stable tag" },
      { name: "ungoogled-chromium", note: "removes Google services and binary blobs" },
      { name: "ungoogled-chromium-windows", note: "Windows build scripts" },
      { name: "Plain Browser", note: "patches + src/plainbrowser: blocker, MV2, branding" },
    ],
    licenses: "Our code is MPL-2.0, our patches BSD-3-Clause. Chromium and ungoogled-chromium keep their BSD-3-Clause licenses, adblock-rust its MPL-2.0, and the bundled filter lists GPLv3 or CC BY-SA 3.0.",
  },
  status: {
    title: "Where the project stands",
    lead: "This is a pre-release and the page says so. What has been verified, and what has not:",
    done: [
      "Built from Chromium 156 on our own Windows builder",
      "Blocker compiles, links and blocks: headless test stops four ad hosts and passes controls",
      "Kill switch verified: with blocking disabled all requests go through",
      "Branding, product icon, Polish and English interface",
      "First-start network silence verified",
      "Tracking-parameter stripping and fingerprint protection built in and verified",
      "Toolbar button and bubble with a per-site toggle for all protections",
      "Safe Browsing through our proxy, with a settings switch; the list update goes only to our proxy",
      "Manifest V2: uBlock Origin 1.75.0 installed, blocking webRequest works",
      "MSIX package built for the Microsoft Store",
    ],
    todo: [
      "Microsoft Store listing and first public release through it",
      "Blocked counter and a settings page for the exception list",
      "Generic cosmetic filtering and filter-list updates between releases",
      "Curated Manifest V2 extension repository",
      "Installation tested on a clean machine",
    ],
  },
  faq: {
    title: "Questions",
    items: [
      {
        q: "Why not just use ungoogled-chromium?",
        a: "You can, and Plain Browser is built on it. We add a content blocker inside the browser, a per-site switch, tracking cleanup and fingerprint protection, keep MV2 working, ship through the Microsoft Store with automatic updates and plan a curated extension repository, so the result is something a non-technical person can install and forget.",
      },
      {
        q: "Why not Brave?",
        a: "Brave is a good browser, and we use its blocking engine. Plain Browser has no crypto wallet, rewards, news feed, AI assistant or VPN upsell. It is the engine, the blocker and nothing else.",
      },
      {
        q: "Can I install uBlock Origin?",
        a: "Manifest V2 is enabled, so it should work, and that is the point. We tested uBlock Origin 1.75.0 end to end and its blocking webRequest rules work, but not every extension has been tried, so treat it as experimental. Extensions are installed manually from a .crx or unpacked folder; a curated repository is planned.",
      },
      {
        q: "Does it update itself?",
        a: "The browser has no updater of its own. Plain Browser is distributed through the Microsoft Store as an MSIX package, and the Store signs it and delivers updates, so we need no updater and no code-signing certificate. The Store listing is being prepared; until it is live, test builds are published on GitHub.",
      },
      {
        q: "Is it safe to run an unsigned build?",
        a: "The test builds on GitHub are unsigned: SmartScreen will warn and Windows with Smart App Control on will refuse to run them. Every release carries SHA-256 sums and a keyless cosign signature that ties it to this repository's build workflow. The Microsoft Store version is signed by the Store. See the Download page for how to verify.",
      },
      {
        q: "Does Safe Browsing work, and what does Google see?",
        a: "Yes, in standard protection with a local hash-prefix database. List updates and the rare full-hash lookups go to our Cloudflare proxy at sb.plain-browser.app instead of Google. The proxy strips your IP address, cookies and identifying headers, keeps no logs, and Google sees only Cloudflare's address and hash prefixes. Enhanced Protection and URL reporting are removed for good, and a switch under Settings > You and Google turns Safe Browsing off. Details are in the privacy policy.",
      },
      {
        q: "Which platforms?",
        a: "Windows 11 x64 only for now. The build scripts come from ungoogled-chromium-windows; other platforms are not planned until the Windows version is solid.",
      },
    ],
  },
  cta: {
    title: "Try the alpha",
    body: "If you are comfortable testing early software, download it, break it, and tell us what happened.",
    download: "Download",
    issues: "Report a bug",
  },
  download: {
    title: "Download",
    lead: "Early alpha for testers. The browser works, the blocker works, the rest is in progress.",
    version: "Version",
    requires: "Requires",
    installer: "Installer",
    installerNote: "Standard per-user installer. Run it and follow the steps.",
    zip: "Portable ZIP",
    zipNote: "Unpack anywhere and start chrome.exe. Nothing is written to the registry.",
    sums: "SHA-256 checksums",
    store: "Microsoft Store",
    storeNote: "Listing in preparation, submitted for Store certification. The Store version is signed by Microsoft, updates itself and keeps its profile in app storage.",
    msixNote: "Package for the Microsoft Store or for sideloading with Add-AppxPackage.",
    published: "Published",
    releaseNotes: "Release notes for",
    unsignedTitle: "This build is not code-signed",
    unsignedBody:
      "Windows SmartScreen will show a warning. Windows 11 with Smart App Control turned on will refuse to run the files. Test in a virtual machine or on a computer where that is acceptable. We will sign releases once a certificate is in place.",
    verifyTitle: "Verify what you downloaded",
    verifyBody:
      "Compare the SHA-256 of each file against SHA256SUMS.txt. Releases published through our workflow also carry a cosign bundle per file; the signature proves the file came from this repository's release workflow and was not altered afterwards.",
    allReleases: "All releases and release notes on GitHub",
    privacyNote: "Downloads are served by GitHub. This site itself sets no cookies and loads nothing from third parties.",
  },
  footer: {
    tagline: "A plain Chromium browser for Windows.",
    disclaimer:
      "Plain Browser is not affiliated with Google or the Chromium project. Chromium is a project of The Chromium Authors. Other names belong to their owners.",
    privacy: "Privacy policy",
    security: "Security policy",
    source: "Source code",
    issues: "Issues",
  },
  notFound: { title: "Page not found", body: "There is nothing at this address.", home: "Back to the start page" },
};

const pl: Dict = {
  htmlLang: "pl",
  metaTitle: "Plain Browser",
  metaDescription:
    "Zwykła przeglądarka na Chromium dla Windows: bez usług Google, z wbudowanym blokowaniem reklam i trackerów, z zachowanymi rozszerzeniami Manifest V2. Darmowa i otwartoźródłowa.",
  nav: { download: "Pobierz", privacy: "Prywatność", security: "Bezpieczeństwo", source: "Kod", switchLabel: "English", switchHref: "/" },
  hero: {
    badge: "Wczesna alfa · Windows 11",
    title: "Przeglądarka. Nic więcej.",
    lead:
      "Plain Browser to Chromium z usuniętymi elementami Google, blokerem reklam i trackerów wbudowanym w warstwę sieciową oraz włączonymi rozszerzeniami Manifest V2. Bez konta, synchronizacji i telemetrii. Nie mamy serwera, który mógłby odebrać Twoje dane.",
    download: "Pobierz dla Windows",
    downloadNote: "Niepodpisana wersja alfa. Mogą zdarzać się niedociągnięcia.",
    source: "Kod na GitHubie",
  },
  mock: { tab: "Nowa karta", url: "plain-browser.app", blocked: "Zablokowano 14 żądań", placeholder: "Szukaj lub wpisz adres" },
  pillars: {
    title: "Co dostajesz",
    items: [
      {
        title: "Blokowanie wbudowane w przeglądarkę",
        body:
          "Bloker to adblock-rust, silnik używany przez Brave, wkompilowany w warstwę sieciową. Nie zależy od API rozszerzeń, działa na Twoim urządzeniu i niczego nie zgłasza. EasyList, EasyPrivacy i polska EasyList są dołączone i aktualizowane codziennie z naszego serwera, więc przeglądarka nie łączy się z autorami list.",
      },
      {
        title: "Bez usług Google",
        body:
          "Wszystko, co usuwa projekt ungoogled-chromium, pozostaje usunięte: logowanie, synchronizacja, tłumaczenie, Web Store, raportowanie użycia. Jedyną usługę, którą zachowujemy, Safe Browsing, puszczamy przez własne proxy, które ukrywa Cię przed Google, a przełącznik w ustawieniach ją wyłącza.",
      },
      {
        title: "Manifest V2 zostaje",
        body:
          "Chrome porzucił MV2, a Web Store usunął te rozszerzenia. Plain Browser zachowuje MV2 i blokujące webRequest, więc narzędzia takie jak uBlock Origin działają z pełną mocą. Sprawdzone z uBlock Origin 1.75.0.",
      },
      {
        title: "Nadal Chromium",
        body:
          "Ten sam silnik, ta sama szybkość, te same karty, zakładki, pobieranie, przeglądarka PDF i narzędzia deweloperskie, które znasz. Strony działają tak jak wszędzie.",
      },
      {
        title: "Czyszczenie śledzenia i przełącznik per strona",
        body:
          "Parametry śledzące (utm_, fbclid, gclid i inne) są usuwane z linków, a fingerprinting przez canvas, audio i WebGL jest osłabiany, z wbudowaną listą wyjątków dla banków. Jeden przycisk na pasku wyłącza to wszystko, razem z blokerem, na stronie, która przestała działać.",
      },
      {
        title: "Otwarty kod, mały zestaw łatek",
        body:
          "Nasz kod leży w jednym katalogu nad ungoogled-chromium, z krótką listą łatek, więc za każdym wydaniem Chromium można nadążyć w kilka dni. MPL-2.0 i BSD-3-Clause.",
      },
    ],
  },
  nothing: {
    title: "Czego nie robi",
    lead: "Polityka prywatności mieści się w jednym zdaniu: niczego nie zbieramy, bo nie ma czym.",
    items: [
      "Brak telemetrii, statystyk użycia i raportów o awariach",
      "Brak konta, logowania i synchronizacji",
      "Brak analityki i identyfikatora reklamowego w przeglądarce",
      "Brak pobierania list filtrów: listy są w pliku binarnym",
      "Brak kontaktu z usługami Google",
      "Safe Browsing wyłącznie przez nasze proxy bez logów, z możliwością wyłączenia",
    ],
    proof:
      "Sprawdzone przy pierwszym uruchomieniu: świeży profil bez otwartej strony przez 100 sekund nie wysłał żadnych własnych żądań sieciowych. Jedynym ruchem było wykrywanie proxy w Windows, które wykonuje każda aplikacja używająca ustawień systemowych.",
  },
  built: {
    title: "Jak jest zbudowana",
    lead: "Plain Browser to cienka warstwa. Każde wydanie Chromium przechodzi przez poniższy stos, dlatego poprawki bezpieczeństwa mogą trafić do Ciebie w kilka dni od stabilnego tagu.",
    layers: [
      { name: "Chromium", note: "przypięty stabilny tag" },
      { name: "ungoogled-chromium", note: "usuwa usługi Google i binarne bloby" },
      { name: "ungoogled-chromium-windows", note: "skrypty budowania dla Windows" },
      { name: "Plain Browser", note: "łatki + src/plainbrowser: bloker, MV2, branding" },
    ],
    licenses: "Nasz kod ma licencję MPL-2.0, łatki BSD-3-Clause. Chromium i ungoogled-chromium zachowują BSD-3-Clause, adblock-rust MPL-2.0, a dołączone listy filtrów GPLv3 lub CC BY-SA 3.0.",
  },
  status: {
    title: "Na jakim etapie jest projekt",
    lead: "To wersja przedpremierowa i ta strona tego nie ukrywa. Co zostało sprawdzone, a co nie:",
    done: [
      "Zbudowana z Chromium 156 na własnej maszynie budującej Windows",
      "Bloker kompiluje się, linkuje i blokuje: test headless zatrzymuje cztery hosty reklamowe i przepuszcza kontrolne",
      "Wyłącznik sprawdzony: z wyłączonym blokowaniem wszystkie żądania przechodzą",
      "Branding, ikona produktu, interfejs po polsku i angielsku",
      "Cisza sieciowa przy pierwszym uruchomieniu potwierdzona",
      "Usuwanie parametrów śledzących i ochrona fingerprintu wbudowane i sprawdzone",
      "Przycisk i dymek na pasku z przełącznikiem per strona dla wszystkich zabezpieczeń",
      "Safe Browsing przez nasze proxy, z przełącznikiem w ustawieniach; aktualizacja listy idzie wyłącznie do naszego proxy",
      "Manifest V2: uBlock Origin 1.75.0 zainstalowany, blokujące webRequest działa",
      "Pakiet MSIX dla Microsoft Store zbudowany",
    ],
    todo: [
      "Wpis w Microsoft Store i pierwsze publiczne wydanie przez Store",
      "Licznik zablokowanych i strona ustawień z listą wyjątków",
      "Ogólne ukrywanie elementów (cosmetic) i aktualizacje list filtrów między wydaniami",
      "Wyselekcjonowane repozytorium rozszerzeń Manifest V2",
      "Instalacja sprawdzona na czystym komputerze",
    ],
  },
  faq: {
    title: "Pytania",
    items: [
      {
        q: "Czemu nie po prostu ungoogled-chromium?",
        a: "Można, i na nim Plain Browser jest zbudowany. My dodajemy bloker w środku przeglądarki, przełącznik per strona, czyszczenie śledzenia i ochronę fingerprintu, utrzymujemy MV2, wydajemy przez Microsoft Store z automatycznymi aktualizacjami i planujemy wyselekcjonowane repozytorium rozszerzeń, żeby całość mogła zainstalować osoba nietechniczna i o niej zapomnieć.",
      },
      {
        q: "Czemu nie Brave?",
        a: "Brave to dobra przeglądarka i używamy jej silnika blokowania. Plain Browser nie ma portfela krypto, nagród, kanału wiadomości, asystenta AI ani reklamy VPN. Jest silnik, bloker i nic więcej.",
      },
      {
        q: "Czy zainstaluję uBlock Origin?",
        a: "Manifest V2 jest włączony, więc powinien działać, i o to chodzi. Przetestowaliśmy uBlock Origin 1.75.0 od początku do końca i jego reguły blokujące webRequest działają, ale nie sprawdziliśmy każdego rozszerzenia, więc traktuj to jako eksperyment. Rozszerzenia instaluje się ręcznie z pliku .crx lub rozpakowanego katalogu; wyselekcjonowane repozytorium jest w planach.",
      },
      {
        q: "Czy aktualizuje się sama?",
        a: "Przeglądarka nie ma własnego mechanizmu aktualizacji. Plain Browser jest dystrybuowany przez Microsoft Store jako pakiet MSIX, a Store go podpisuje i dostarcza aktualizacje, więc nie potrzebujemy ani updatera, ani certyfikatu do podpisu kodu. Wpis w Store jest w przygotowaniu; do czasu jego publikacji wersje testowe publikujemy na GitHubie.",
      },
      {
        q: "Czy bezpiecznie uruchamiać niepodpisaną wersję?",
        a: "Wersje testowe z GitHuba są niepodpisane: SmartScreen ostrzeże, a Windows z włączonym Smart App Control odmówi ich uruchomienia. Każde wydanie ma sumy SHA-256 i bezkluczowy podpis cosign powiązany z procesem budowania w tym repozytorium. Wersję z Microsoft Store podpisuje Store. Na stronie Pobierz jest opis, jak to sprawdzić.",
      },
      {
        q: "Czy Safe Browsing działa i co widzi Google?",
        a: "Tak, w trybie ochrony standardowej z lokalną bazą prefiksów hashy. Aktualizacje listy i rzadkie zapytania o pełne hashe idą do naszego proxy Cloudflare pod sb.plain-browser.app zamiast do Google. Proxy usuwa Twój adres IP, ciasteczka i nagłówki identyfikujące, nie prowadzi logów, a Google widzi tylko adres Cloudflare i prefiksy hashy. Ochrona rozszerzona i raportowanie adresów są usunięte na stałe, a przełącznik w Ustawienia → Ty i Google wyłącza Safe Browsing. Szczegóły w polityce prywatności.",
      },
      {
        q: "Jakie platformy?",
        a: "Na razie tylko Windows 11 x64. Skrypty budowania pochodzą z ungoogled-chromium-windows; inne platformy nie są planowane, dopóki wersja na Windows nie będzie dopracowana.",
      },
    ],
  },
  cta: {
    title: "Wypróbuj alfę",
    body: "Jeśli nie boisz się testować wczesnego oprogramowania, pobierz, popsuj i opowiedz nam, co się stało.",
    download: "Pobierz",
    issues: "Zgłoś błąd",
  },
  download: {
    title: "Pobierz",
    lead: "Wczesna alfa dla testerów. Przeglądarka działa, bloker działa, reszta jest w toku.",
    version: "Wersja",
    requires: "Wymagania",
    installer: "Instalator",
    installerNote: "Standardowy instalator dla bieżącego użytkownika. Uruchom i przejdź przez kroki.",
    zip: "Wersja przenośna (ZIP)",
    zipNote: "Rozpakuj gdziekolwiek i uruchom chrome.exe. Nic nie zapisuje się w rejestrze.",
    sums: "Sumy kontrolne SHA-256",
    store: "Microsoft Store",
    storeNote: "Wpis w przygotowaniu, wysłany do certyfikacji w Store. Wersja ze Store jest podpisana przez Microsoft, aktualizuje się sama i trzyma profil w pamięci aplikacji.",
    msixNote: "Pakiet dla Microsoft Store albo do ręcznej instalacji przez Add-AppxPackage.",
    published: "Opublikowano",
    releaseNotes: "Informacje o wydaniu",
    unsignedTitle: "Ta wersja nie jest podpisana",
    unsignedBody:
      "Windows SmartScreen pokaże ostrzeżenie. Windows 11 z włączonym Smart App Control odmówi uruchomienia plików. Testuj w maszynie wirtualnej albo na komputerze, gdzie to akceptowalne. Podpiszemy wydania, gdy tylko będziemy mieli certyfikat.",
    verifyTitle: "Sprawdź, co pobrałeś",
    verifyBody:
      "Porównaj SHA-256 każdego pliku z SHA256SUMS.txt. Wydania opublikowane przez nasz workflow mają też pakiet cosign dla każdego pliku; podpis dowodzi, że plik pochodzi z procesu wydawania w tym repozytorium i nie został później zmieniony.",
    allReleases: "Wszystkie wydania i informacje o zmianach na GitHubie",
    privacyNote: "Pliki serwuje GitHub. Ta strona nie ustawia ciasteczek i nie ładuje niczego z serwerów stron trzecich.",
  },
  footer: {
    tagline: "Zwykła przeglądarka na Chromium dla Windows.",
    disclaimer:
      "Plain Browser nie jest powiązany z Google ani z projektem Chromium. Chromium to projekt The Chromium Authors. Pozostałe nazwy należą do ich właścicieli.",
    privacy: "Polityka prywatności",
    security: "Polityka bezpieczeństwa",
    source: "Kod źródłowy",
    issues: "Zgłoszenia",
  },
  notFound: { title: "Nie ma takiej strony", body: "Pod tym adresem niczego nie ma.", home: "Wróć na stronę główną" },
};

export const dict: Record<Lang, Dict> = { en, pl };
