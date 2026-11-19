# „Zbudowałem własną przeglądarkę” – plan filmu na YouTube (35 min)

Film o powstawaniu Plain Browser: od pomysłu, przez wybór bazy (ungoogled-chromium), własną warstwę patchy
i kod w `src/plainbrowser`, aż po 7-godzinny build i paczkę MSIX do Microsoft Store. Dokument jest scenariuszem
roboczym: każdy segment ma czas, cel, co mówimy, co jest na ekranie i co trzeba nagrać wcześniej.

Założenia: język polski, widz techniczny (programista lub „power user”), ale bez znajomości Chromium.
Narracja z kamery + nagrania ekranu (OBS) + kilka animowanych diagramów. Ton: szczery, z liczbami,
bez „pierwszy w Polsce”.


## Stan produkcji (2026-10-28)

Gotowy jest **montaż roboczy** `dist/video/plain-browser-montaz.mp4` (26:14, 1920x1080, 30 kl./s, 12 rozdziałów,
napisy PL w pliku i obok jako `.srt`, rozdziały do opisu w `rozdzialy.txt`). Składa się z 24 nagranych ujęć ekranu
(`dist/video/ujecia/`) i plansz z tekstem w miejscach „do kamery”; narracja z offu idzie jako napisy. Narzędzia:
`infra/video/README.md`.

Różnice względem planu powyżej:
- **Długość:** scenariusze mają 3 379 słów, czyli ok. 22,5 min mowy przy 150 słowach/min; z ujęciami wychodzi 26 min,
  nie 35. Żeby dojść do 35 min, trzeba dopisać ok. 1 300 słów albo mówić wolniej (ok. 110 słów/min).
- **Strona z reklamami:** zamiast prawdziwego portalu własna strona demo `kurier.test` (prawdziwe adresy sieci
  reklamowych, treść fikcyjna), żeby w filmie nie było cudzych treści.
- **Timelapse builda:** animacja odtworzona z prawdziwego `.ninja_log` PB-BUILD (58 692 kroki, 7:00:30), z podpisem
  na ekranie; pełny 7-godzinny build nie był nagrywany.
- **GitHub Actions:** repo jest prywatne, więc zamiast strony Actions są pliki workflow w terminalu.
- **Partner Center:** nie nagrany (wymaga logowania); zamiast niego tekst listingu i strona polityki prywatności.

Do zrobienia przez autora: nagranie twarzy i głosu (plansze „DO KAMERY” i napisy „z offu” w montażu roboczym
pokazują, co i gdzie powiedzieć), miniatura, ewentualnie nowa wersja `chrome://version` bez logo Chromium.

---

## Propozycje tytułu i miniatury

Tytuły (A/B w YouTube Studio):
1. „Zbudowałem własną przeglądarkę. Bez Google, z adblockiem w środku”
2. „Ile kosztuje własna przeglądarka? 7 godzin kompilacji i 48 700 plików”
3. „Chrome bez Google: jak powstaje Plain Browser”

Miniatura: ikona Plain Browser obok przekreślonego logo Google + duży napis „7 h BUILD”. Twarz z lewej, ekran
z `ninja` w tle.

---

## Struktura (35:00)

| Czas | Segment | Długość |
|---|---|---|
| 0:00 | 1. Hook i obietnica | 1:30 |
| 1:30 | 2. Dlaczego w ogóle własna przeglądarka | 3:00 |
| 4:30 | 3. Czego NIE robimy: silnik od zera vs fork | 3:00 |
| 7:30 | 4. Architektura warstw: Chromium → ungoogled → nasza warstwa | 4:30 |
| 12:00 | 5. Pierwszy build: 7 godzin, 59 500 kroków | 4:00 |
| 16:00 | 6. Branding: jak Chromium staje się Plain Browser | 2:30 |
| 18:30 | 7. Adblock w procesie przeglądarki (adblock-rust) | 5:30 |
| 24:00 | 8. Manifest V2, fingerprint, stripping parametrów | 4:00 |
| 28:00 | 9. Testy, CI i rebase co 4 tygodnie | 3:00 |
| 31:00 | 10. Pakowanie: MSIX i Microsoft Store | 2:00 |
| 33:00 | 11. Co nie wyszło, co dalej, koszty | 1:30 |
| 34:30 | 12. Outro | 0:30 |

---

## 1. Hook i obietnica (0:00–1:30)

Cel: w 20 sekund pokazać, że przeglądarka działa, a potem obiecać „jak”.

Na ekranie: Plain Browser otwiera stronę z reklamami, badge na pasku pokazuje liczbę zablokowanych żądań
rosnącą na żywo. Cięcie na `chrome://version` z nazwą „Plain Browser”.

Mówimy:
- „To jest przeglądarka, którą zbudowałem sam. Działa na Chromium, nie łączy się z Google, a bloker reklam
  siedzi w stosie sieciowym, nie w rozszerzeniu.”
- „W tym filmie: skąd wziąć 48 tysięcy plików źródłowych, jak nie zwariować przy 7-godzinnej kompilacji
  i jak dopisać własny kod tak, żeby przeżył następną wersję Chromium za 4 tygodnie.”
- Jedno zdanie o tym, czego film nie obiecuje: „Nie napisałem silnika. Pokażę, dlaczego nikt rozsądny tego nie robi.”

Do nagrania: 3 ujęcia przeglądarki w akcji (strona z reklamami, badge, Ustawienia > Prywatność).

## 2. Dlaczego własna przeglądarka (1:30–4:30)

Cel: motywacja, którą widz może podzielać.

Punkty:
- Chrome 139 wyłączył Manifest V2, Web Store usunął rozszerzenia MV2 (31.08.2026). uBlock Origin w pełnej mocy
  przestał działać w Chrome.
- ungoogled-chromium usuwa Google, ale nie ma wbudowanego blokera ani aktualizacji „dla zwykłego człowieka”.
- Brave robi to, co chcę, ale ma własne pomysły (krypto, własny ekosystem). Chciałem „plain”: bez niczego.
- Lista celów z README: brak Google, adblock wbudowany, MV2 żyje, mały zestaw patchy.

Na ekranie: tabelka „Chrome / ungoogled / Brave / Plain” z czterema kolumnami celów. Wycinek strony
chrome.google.com z komunikatem o MV2.

## 3. Silnik od zera vs fork (4:30–7:30)

Cel: rozbroić pytanie z komentarzy „a czemu nie od zera”.

Punkty:
- Silnik przeglądarki to Blink + V8 + stos sieciowy + sandbox. Samo Blink i V8 to 35% czasu kompilacji.
- Opcje: Chromium, Firefox (Gecko), Servo, Ladybird. Krótko o każdej, dlaczego Chromium: ekosystem rozszerzeń,
  ungoogled-chromium jako gotowa baza, narzędzia Windows.
- Kluczowa decyzja: **ungoogled-chromium to nie fork drzewa, tylko zestaw patchy**. My robimy tak samo.
  Nasze repo to patche + jeden katalog własnego kodu, a nie kopia Chromium.

Na ekranie: diagram „4 warstwy” (Chromium → ungoogled-chromium → ungoogled-chromium-windows → Plain Browser).
`ls` repozytorium: `patches/`, `src/plainbrowser/`, `branding/`, `scripts/`, `versions.toml`.

## 4. Architektura warstw (7:30–12:00)

Cel: pokazać, jak wygląda „dopisanie kodu do Chromium” bez edytowania Chromium.

Punkty:
- `versions.toml`: jeden pinowany tag upstreamu (156.0.8078.47-1.1). Bump = nowa wersja Chromium.
- `patches/series` i katalogi `00-build … 90-defaults`. Zasada nr 1: jak najmniej zmian w plikach Chromium,
  jak najwięcej w `src/plainbrowser/`. Zasada nr 2: każdy patch w jednej kategorii, z uzasadnieniem w nagłówku.
- `00-build`: jedyne wpięcie GN, `//plainbrowser` w zależnościach `chrome/browser` i `chrome/renderer`.
- `upstream-revert.list`: patche ungoogled, których NIE nakładamy (przykład: Safe Browsing przywracamy
  i puszczamy przez własne proxy).
- Dlaczego to ma znaczenie: Chromium wydaje stable co 4 tygodnie, punktowe co tydzień. Każdy patch w drzewie
  to koszt przy każdym rebase.

Na ekranie: `cat patches/series`, otwarty jeden patch z `00-build` (kilkanaście linii), drzewo
`src/plainbrowser/` (adblock, shields, fingerprint, common, ungoogled). Animacja: patch „nakłada się” na drzewo.

Do nagrania: `scripts/check-patches.py` nakładający serię „na sucho” na tag upstreamu, bez checkoutu Chromium.

## 5. Pierwszy build: 7 godzin (12:00–16:00)

Cel: najbardziej „klikalny” segment. Liczby i cierpienie.

Punkty (wszystkie z `docs/BUILD_CACHE.md`, oznaczone jako zmierzone):
- Maszyna: VM 12 vCPU, Windows, MSVC, bez depot_tools (ungoogled-chromium-windows).
- Pełny build: 7,0 h wall, 59 500 kroków, 69 godzin CPU. 48 700 obiektów C++ to 97% czasu.
- Rust, mojom, grit, linkowanie: razem poniżej 3%.
- Rebuild po zmianie w naszej warstwie: 12–35 min. Winowajca: `generated_resources.h` includowany przez 823 pliki.
  Jedna nowa etykieta w UI = 835 plików do przekompilowania.
- Bump wersji Chromium: 294 zmienione pliki, ale jeden nagłówek (`hash_table.h` z Blink) ciągnie 8 315 obiektów.
- Co pomaga: cache kompilacji (szacunek 7 h → 2,5–3 h), własny plik `.grd` dla naszych stringów.

Na ekranie: timelapse `ninja` (nagrać wcześniej, przyspieszyć 1000x, licznik `[12345/59500]` widoczny),
wykres kołowy czasu CPU, `scripts/build.ps1` w edytorze. Zegar w rogu.

Do nagrania: timelapse pełnego builda na VM (OBS, 1 klatka na 5 s). Zdjęcie/ujęcie VM w `vm-sync.sh`.

## 6. Branding (16:00–18:30)

Cel: „jak Chromium dowiaduje się, że nazywa się inaczej”.

Punkty:
- Co dotyka zmiana nazwy: ikony, stringi, `install_static` (GUID, klucze rejestru, ProgID, katalog profilu
  `%LOCALAPPDATA%\PlainBrowser`), `chrome://version`, instalator.
- `scripts/apply-branding.py` i `branding/`: nazwa produktu z `versions.toml` wchodzi w jeden skrypt.
- Pułapka: ungoogled robi „domain substitution” po nałożeniu patchy, więc hunk z hostem Google nie przejdzie.

Na ekranie: przed/po `chrome://version`, ikona w pasku zadań Windows, katalog profilu w Explorerze.

## 7. Adblock w procesie przeglądarki (18:30–24:00)

Cel: najbardziej techniczny fragment. Pokazać, czym różni się bloker natywny od rozszerzenia.

Punkty:
- Dlaczego nie rozszerzenie: zależy od `webRequest`, które Google ogranicza. Model Brave Shields: silnik
  w stosie sieciowym.
- adblock-rust (silnik Brave) + cxx: bridge crate z wbundlowanymi listami (EasyList, EasyPrivacy, EasyList Polish).
- Wendorowanie crate'ów do GN przez `gnrt`, skrypt `vendor-adblock-crates.ps1`, `post-gnrt-fixups.ps1`.
- Cztery elementy w `src/plainbrowser/adblock`: serwis w procesie przeglądarki, filtr mojo, throttle żądań,
  agent kosmetyczny w rendererze.
- Co poza blokowaniem: `$redirect` z własnymi zamiennikami (noop, 1x1 GIF, stub analytics), scriptlety
  `##+js(...)`, filtry proceduralne (`:has-text()`, `:upward()`), generyczne ukrywanie po klasach/id.
- Aktualizacja list raz dziennie z `lists.plain-browser.app` (Cloudflare Worker), manifest podpisany Ed25519,
  SHA-256 per lista, podmiana silnika w tle.
- Kill switch: `--disable-features=PlainBrowserAdblock`.

Na ekranie: diagram „żądanie → throttle → adblock-rust → blok / przepuść / redirect”. Demo
`scripts/test-adblock.ps1`: 4 hosty reklamowe zablokowane, kontrole przechodzą, z kill switchem wszystko
przechodzi. Bubble „shields” z przełącznikiem per site, `chrome://adblock`.

Do nagrania: demo testu headless, strona `chrome://adblock`, bubble na realnej stronie z reklamami.

## 8. MV2, fingerprint, stripping parametrów (24:00–28:00)

Cel: pokazać, że „przeglądarka prywatna” to kilka niezależnych mechanizmów.

Punkty:
- **Manifest V2**: ungoogled trzyma flagę, my czyścimy UI deprecacji. Google usuwa kod od ~150/151, Brave
  utrzymuje patche odwracające. To stały koszt rebase'u, nie jednorazowa flaga. Test z realnym rozszerzeniem
  (`docs/MV2.md`).
- **Stripping parametrów**: throttle nawigacji + lista `$removeparam` (`utm_*`, `fbclid`, `gclid`).
- **Omijanie redirectorów**: `l.facebook.com/l.php?u=`, `google.com/url?q=`, Outlook Safe Links, prosto do celu.
- **Fingerprint**: canvas, audio, WebGL, `hardwareConcurrency`, `deviceMemory`. Pierwsza wersja jako skrypt
  w świecie strony, bez patchy w Blink. Lista zgodności dla bankowości.
- **Safe Browsing**: jedyna usługa Google, z którą przeglądarka rozmawia, i to przez nasze proxy
  `sb.plain-browser.app`, tylko listy publiczne, nagłówek `X-Client`. Jeden przełącznik w Ustawieniach.
- Jedna lista wyjątków per site (`plainbrowser.shields.exceptions`) czytana przez wszystkie trzy mechanizmy.

Na ekranie: instalacja uBlock Origin z pliku i działający panel. Link z `utm_` wklejony w pasek, po nawigacji
czysty URL. Strona testowa fingerprintu: hash canvas różny między sesjami.

## 9. Testy, CI, rebase (28:00–31:00)

Cel: „własna przeglądarka” to głównie utrzymanie.

Punkty:
- `scripts/test-*.ps1`: headless na VM, od adblocka przez shields po Safe Browsing. Testy UI naciskają
  kontrolki przez UI Automation, klik myszą tylko jako fallback.
- Testy jednostkowe C++ (`docs/TESTING.md`), testy crate'a Rust w `rust.yml`.
- CI: `patches-check.yml` nakłada serię na sucho przy każdej zmianie w `patches/`; `upstream-watch.yml` przy
  nowym tagu upstreamu wkleja wynik do issue. `build-windows.yml` w łańcuchu etapów.
- Rytm: nowy tag ungoogled-chromium-windows → sprawdzenie patchy → bump `versions.toml` → 7 h builda → testy.
- `upstream-security-check.py`: czy nasza wersja nie zostaje za poprawkami bezpieczeństwa Chromium.

Na ekranie: GitHub Actions, issue od `upstream-watch` z wynikiem, log testu shields.

## 10. Pakowanie: MSIX i Store (31:00–33:00)

Punkty:
- Trzy formaty: portable zip, instalator, MSIX (`scripts/package-*.py`).
- Decyzja: **tylko Microsoft Store**. Store podpisuje paczkę i dostarcza aktualizacje, więc nie ma własnego
  updatera ani certyfikatu Authenticode (koszt i ryzyko).
- Konsekwencja: ungoogled wyrzuca Omaha, a bez updatera nie dostarczymy poprawek. Store zamyka tę dziurę.
- Teksty do listingu w `docs/STORE_LISTING.md`, strona plain-browser.app z politykami EN i PL.

Na ekranie: `package-msix.py`, paczka w Explorerze, szkic listingu w Partner Center (zamazane dane konta).

## 11. Co nie wyszło, co dalej, koszty (33:00–34:30)

Szczerze:
- Stan: alpha 0.1.3-alpha.1, build niepodpisany, listing w przygotowaniu. Nietestowane: UI na realnych stronach
  poza testowymi, instalacja ze Store na czystej maszynie.
- Nie zrobione: kuratorowane repozytorium rozszerzeń MV2, szum fingerprintu na poziomie Blink, cache builda.
- Koszt: VM 12 vCPU przez miesiące, godziny na każdy rebase. Wycofany crash reporting (commit revert), bo
  nie chciałem zbierać danych przed polityką.
- Roadmapa: Store, repozytorium MV2 (uBO i podobne), Blink-level fingerprint.

## 12. Outro (34:30–35:00)

- Link do repo i strony, licencje (MPL-2.0 kod, BSD-3 patche), jak zgłaszać błędy (SECURITY.md).
- Zapowiedź: „Następny film: pierwszy rebase na Chromium 157 na żywo.”

---

## Lista nagrań do zrobienia przed montażem

**Przed pierwszym nagraniem: Windows na maszynie, z której nagrywamy, musi być aktywowany.** Nieaktywowany
Windows rysuje w prawym dolnym rogu znak wodny „Aktywuj system Windows”, którego nie da się wyciąć bez
kadrowania całego ujęcia. Dotyczy to zwłaszcza VM budującej (timelapse builda, testy, Menedżer zadań).
Sprawdzić: Ustawienia > System > Aktywacja, stan „Aktywowano”. Jeśli VM ma zostać nieaktywowana, nagrania
UI robić na aktywowanej maszynie, a z VM brać tylko terminal w oknie, bez pulpitu w kadrze.


1. Timelapse pełnego builda na VM (1 klatka / 5 s, licznik ninja w kadrze).
2. Demo: strona z reklamami w Chrome vs Plain Browser, badge z liczbą blokad.
3. `scripts/test-adblock.ps1` z kill switchem i bez.
4. Instalacja uBlock Origin (MV2) z pliku.
5. `utm_` link przed/po nawigacji; redirector Facebooka.
6. Strona testowa fingerprintu w dwóch sesjach.
7. `chrome://version`, `chrome://adblock`, Ustawienia > Prywatność i bezpieczeństwo.
8. GitHub Actions + issue z `upstream-watch`.
9. Paczka MSIX i Partner Center.
10. 4 diagramy do animacji: warstwy, przepływ żądania przez adblock, koszt rebuildu, rytm rebase'u.

## Opis filmu i rozdziały (do wklejenia)

Plain Browser: przeglądarka na Chromium bez usług Google, z blokerem reklam w stosie sieciowym
(adblock-rust) i działającym Manifest V2. W filmie pokazuję, jak powstaje: warstwa patchy na
ungoogled-chromium, własny kod w jednym katalogu, 7-godzinny build i paczka do Microsoft Store.

Repo: <link>  Strona: https://plain-browser.app

0:00 Działa. Co to jest
1:30 Po co własna przeglądarka
4:30 Silnik od zera? Nie
7:30 Patche zamiast forka
12:00 7 godzin kompilacji
16:00 Branding
18:30 Adblock w środku przeglądarki
24:00 MV2, fingerprint, utm_
28:00 Testy i rebase co 4 tygodnie
31:00 MSIX i Microsoft Store
33:00 Co nie wyszło
34:30 Outro
