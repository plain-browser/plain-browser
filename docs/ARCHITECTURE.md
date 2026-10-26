# Plain Browser – projekt architektury (Windows 11)

Stan: szkic v0.1, 2026-10-23. Nazwa produktu: **Plain Browser** (wybrana 2026-10-23). „plainbrowser” zostaje wewnętrznym kryptonimem w kodzie (GN `//plainbrowser`, przestrzeń nazw C++, nazwy VM i zadań).

## 1. Założenia i co z nich wynika

| Założenie | Konsekwencja projektowa |
|---|---|
| Baza: ungoogled-chromium | ungoogled-chromium **nie jest forkiem drzewa źródeł**, tylko zestawem patchy + skryptów nakładanych na upstream Chromium. My robimy to samo: nasze repo to **warstwa patchy i własny kod**, a nie kopia Chromium. |
| Windows 11 | Budujemy przez `ungoogled-chromium-windows` (MSVC, bez depot_tools). Kompilacja tylko na Windows (Linux/WSL nie zbuduje targetu win). |
| adblock-rust | Integracja natywna w procesie przeglądarki (model Brave Shields), nie jako rozszerzenie. |
| MV2 „jak Brave” | Od Chromium 139 MV2 jest wyłączone, a od ~150/151 Google **usuwa kod** (flagi, gating). Brave utrzymuje własne patche odwracające te zmiany i **sam hostuje** 4 rozszerzenia MV2 (uBO, uMatrix, NoScript, AdGuard), bo Chrome Web Store usunął MV2 31.08.2026. „Przywrócenie MV2” to więc stały koszt rebase'u, nie jednorazowa flaga. |
| Własny branding | Dotyka: ikon, stringów, `install_static` (GUID, klucze rejestru, ProgID, katalog profilu), instalatora, strony `chrome://version`, NTP, domyślnej wyszukiwarki. |
| Brak Google w tle | ungoogled wyrzuca Omaha/Google Update, component updater, Safe Browsing. **Musimy mieć własny serwer aktualizacji** – inaczej nie dostarczymy poprawek bezpieczeństwa (Chromium: stable co 4 tyg., punktowe co tydzień). |

## 2. Model warstw

```
Chromium (tag upstream, np. 150.0.7871.x)
  └─ ungoogled-chromium (patches/, pruning.list, domain_substitution.list, flags.gn)
       └─ ungoogled-chromium-windows (patche Windows, build.py, package.py)
            └─ Plain Browser (to repo)
                 ├─ patches/        – minimalne zmiany w drzewie Chromium
                 ├─ src/plainbrowser/   – NASZ kod, kopiowany do chromium/src/plainbrowser (GN //plainbrowser)
                 ├─ branding/       – ikony, grd, install_static, instalator
                 └─ third_party/adblock-rust – submoduł (nasz fork)
```

Zasada nr 1: **jak najmniej edycji w plikach Chromium, jak najwięcej w `src/plainbrowser/`**. Patch w drzewie Chromium to koszt przy każdym rebase (co 4 tygodnie). Plik w `//plainbrowser` kompiluje się dalej, póki nie zmieni się API, którego używa.

Zasada nr 2: każdy patch należy do jednej kategorii i ma nagłówek z uzasadnieniem, żeby bot rebase'u i człowiek wiedzieli, co wolno wyrzucić.

```
patches/
  series                 # kolejność nakładania (format quilt, kompatybilny z utils/patches.py ungoogled)
  00-build/              # //plainbrowser w deps chrome/browser i chrome/renderer (jedyne wpięcie GN)
  10-branding/           # nazwa, ikony, install_static, instalator
  20-updater/            # podmiana URL-i update_client / crx na nasz serwer (jeszcze nie ma)
  30-adblock/            # hooki: throttle, cosmetic injection, throttle nawigacji (removeparam)
  40-mv2/                # odwrócenie usunięcia MV2 (port z brave-core)
  50-credits/            # chrome://credits
  60-ui/                 # pierwsze uruchomienie, Web Store
  70-safebrowsing/       # Safe Browsing przez nasze proxy, nagłówek X-Client
  80-fingerprint/        # wpięcie agenta fingerprintu i przekazanie klucza sesji do rendererów
```

Poza `series` drzewo Chromium edytują jeszcze dwa skrypty (`scripts/apply-branding.py`, `scripts/post-gnrt-fixups.ps1`);
są wymienione w komentarzu w `series`. Hunki nie mogą zawierać hostów Google: ungoogled podmienia je (domain
substitution) po nałożeniu patchy, więc taki hunk wchodzi w jednym przepływie (`prepare.py`) i nie wchodzi w drugim
(`apply-plainbrowser.ps1`). `scripts/check-patches.py` nakłada całą serię „na sucho” na dowolny tag
ungoogled-chromium-windows bez checkoutu Chromium (pobiera tylko dotknięte pliki); CI robi to przy każdej zmianie
w `patches/` (`patches-check.yml`) i przy nowym tagu upstream (`upstream-watch.yml` wkleja wynik do issue).

Opcjonalnie (rekomendowane po kilku miesiącach, gdy patchy przybędzie): przejąć mechanizm Brave `chromium_src/` – podmiana pojedynczych plików przez kolejność include path i makra `#define`. Skaluje się lepiej niż `.patch`, ale wymaga własnego skryptu GN. Na start wystarczą zwykłe patche przez tooling ungoogled.

## 3. Pinowanie wersji i cadence

Plik `versions.toml`:
```
chromium = "150.0.7871.xx"
ungoogled_chromium = "150.0.7871.xx-1"
ungoogled_chromium_windows = "150.0.7871.xx-1.1"
adblock_rust = "<sha submodułu>"
```
Cykl: śledzimy tagi ungoogled-chromium (podążają za Chromium stable). Cel: **release do 7 dni** od tagu ungoogled. Bot (GitHub Actions) codziennie: sprawdza nowy tag → próbuje nałożyć `patches/series` → raportuje, które patche nie weszły. Rebase MV2 to miejsce, gdzie najczęściej będzie bolało.

## 4. Build i CI

- Maszyny: Windows Server 2022/11, 32+ vCPU, 64 GB RAM, 300 GB NVMe. Pełny build ~3–5 h, inkrementalny minuty. Self-hosted runner (GitHub Actions / Azure DevOps). Minimum 2 maszyny (release + PR/canary).
- Toolchain: VS 2022 (Chromium pinuje konkretną wersję i Windows SDK – odczytać z `build/vs_toolchain.py` danego taga), Python 3.11+, Rust z `third_party/rust-toolchain` (Chromium ma własny).
- Pipeline: `build.py` ungoogled-windows z naszymi flagami → `package.py` → nasz `mini_installer` + archiwum portable → **podpis kodu** (EV cert albo Azure Trusted Signing; bez tego SmartScreen blokuje) → upload do serwera Omaha → smoke test (uruchomienie, `chrome://version`, 3 strony testowe adblocka, instalacja uBO MV2).
- Architektura: start x64. ARM64 (Windows on ARM) jako faza późniejsza – Chromium to wspiera, ale podwaja czas CI.
- `flags.gn` (dodatkowo do ungoogled): `is_official_build=true`, `symbol_level=1` (symbole do crash reportów), `enable_rust=true`, `chrome_pgo_phase=0` (bez profili Google; strata ~5–10% perf – akceptowalna), `use_siso=false` jeśli ungoogled dalej jedzie na ninja.

## 5. Branding

Miejsca w Chromium (wszystko za `BUILDFLAG(CHROMIUM_BRANDING)` – budujemy jako „Chromium branding”, podmieniamy zawartość):

- `chrome/app/theme/chromium/` – ikony aplikacji, instalatora, dokumentów (ico, png w rozmiarach 16–256).
- `chrome/app/chromium_strings.grd` + `chrome/app/settings_chromium_strings.grdp` – nazwa produktu. Zamiast patchować setki stringów: skrypt na etapie prepare (jak domain substitution ungoogled) robi `Chromium` → `Plain Browser` w plikach `*_chromium_strings*.grd*` z listą wyjątków.
- `chrome/install_static/chromium_install_modes.cc` – **najważniejszy plik**: nazwa, `app_guid`, klucze rejestru (`Software\PlainBrowser\...`), `AppUserModelId`, ProgID (`PlainBrowserHTML`), katalog `%LOCALAPPDATA%\PlainBrowser\User Data`, kanały (stable/beta). Dzięki temu Plain Browser instaluje się obok Chrome/Chromium bez konfliktu.
- `chrome/installer/setup/`, `chrome/installer/mini_installer/` – nazwa w Add/Remove Programs, skróty, rejestracja jako domyślna przeglądarka (Win11: użytkownik klika w `ms-settings:defaultapps`; rejestracja ProgID musi być kompletna: http, https, .html, .htm, .pdf, .svg, .webp).
- `chrome/browser/ui/webui/version/` i about – wersja Plain Browser + wersja Chromium.
- `components/search_engines/prepopulated_engines.json` + `template_url_prepopulate_data.cc` – domyślna wyszukiwarka (ungoogled zostawia Google jako domyślną z wyciętymi sugestiami; my ustawiamy swoją/DDG/Brave Search – decyzja biznesowa).
- NTP: ungoogled ma pustą/lokalną; dla brandingu własna lokalna strona w `//plainbrowser/ntp` (WebUI, bez sieci).
- Znaki towarowe: nie wolno użyć logo Chrome/Chromium ani nazwy w produkcie poza `chrome://version`.

## 6. Serwer aktualizacji (jedna rzecz, trzy zastosowania)

Protokół Omaha 3/4 obsługuje w Chromium: aktualizację przeglądarki (`chrome/updater`, open-source Omaha 4), komponenty (`components/update_client`) i rozszerzenia CRX (`extensions/browser/updater`). Jeden serwer obsłuży wszystko:

- Serwer: **Nebraska** (Flatcar, Go, Omaha-compatible) albo `omaha-server` (Python). Nebraska jest lżejsza i utrzymywana.
- Klient: `chrome/updater` zbudowany z naszymi stałymi w `chrome/updater/branding.gni` (URL, nazwa serwisu `PlainBrowserUpdater`). Alternatywa na start (szybsza): prosty in-browser updater w `//plainbrowser/updater` sprawdzający JSON i pobierający `mini_installer.exe` (ungoogled tak nie robi – użytkownicy ręcznie aktualizują; dla produktu konsumenckiego to nie przejdzie).
- Patch `20-updater`: przywrócenie `update_client` (ungoogled go nie usuwa, tylko domain substitution kieruje URL-e na martwe hosty) z URL-em `https://update.plainbrowser.example/service/update2`.
- Przez ten sam kanał dystrybuujemy: listy filtrów (jako komponent), zasoby redirect dla adblocka, oraz **nasze repozytorium CRX MV2** (sekcja 8).

## 7. Adblock: integracja adblock-rust

Model Brave Shields, uproszczony.

### 7.1 Budowanie Rusta
Dwie drogi:
- (A) `gnrt` + wendorowanie wszystkich crate'ów adblock-rust do `third_party/rust` – „kanoniczne” dla Chromium, ale ~40 crate'ów do utrzymania, audyty `//third_party/rust` przy każdym bumpie.
- (B) **rekomendowane**: crate `plainbrowser-adblock-ffi` (nasz, cienki) z zależnością na `adblock` (submoduł), budowany przez cargo (`cargo vendor` do offline build) do `staticlib`; GN target `//plainbrowser/adblock/rs` uruchamia cargo jako `action`, a C++ łączy się przez **cxx** (jest w drzewie Chromium). Tak robi Brave.

### 7.2 Komponenty (wszystko w `//plainbrowser/adblock`)
- `AdBlockService` (KeyedService per profil, w procesie przeglądarki). Trzyma `adblock::Engine` (jeden na zestaw list: default + regionalne + custom użytkownika). Silnik żyje na **dedykowanym `SequencedTaskRunner`**; żadnych wywołań na UI/IO bez PostTask.
- `AdBlockURLLoaderThrottle` (`blink::URLLoaderThrottle`, rejestrowany w `ChromeContentBrowserClient::CreateURLLoaderThrottles` – to jest 1 patch). W `WillStartRequest`: deferuje, odpytuje silnik (`check_network_request`: url, source_url/initiator, request type), następnie: cancel / redirect do lokalnego zasobu (`$redirect`) / modyfikacja nagłówków CSP (`$csp`) / kontynuacja. Hot path: cache wyników per (host, typ) w pamięci, bo throttle jest na każdym żądaniu.
- `CosmeticFilterAgent` (renderer, `content::RenderFrameObserver`) + mojo do browsera. Przy `DidCommitProvisionalLoad` pyta `url_cosmetic_resources(url)`; wstrzykuje CSS (`WebDocument::InsertStyleSheet`) i uruchamia skrypt scriptletów w isolated world. Generic hides z `hidden_class_id_selectors` po skanie DOM (jak uBO: lazy, przy mutacjach).
- `FilterListService` – pobiera listy (EasyList, EasyPrivacy, uBO filters, Polish filters – CertyficateIT, Fanboy Annoyances jako opcja) z naszego serwera; serializujemy silnik serwerowo (`Engine::serialize`) do DAT i dystrybuujemy gotowy DAT jako komponent – start zimny < 50 ms zamiast parsowania tekstu. Odświeżanie co 24 h.
- `ResourceStore` – zasoby redirect (noop.js, 1x1.gif, …) – wziąć z uBO `web_accessible_resources` (GPLv3 – OK dla dystrybucji danych).
- UI: ikona w pasku z licznikiem (`//plainbrowser/adblock/ui`, bubble jak Brave Shields), per-site toggle, `chrome://adblock` (WebUI) z listą, custom filters, logger żądań dla debugowania. Stan per-site w `HostContentSettingsMap` (własny `ContentSettingsType` – 1 patch w `components/content_settings`).
- Współistnienie z uBO MV2: jeśli użytkownik instaluje uBO/AdGuard, pokazujemy propozycję wyłączenia wbudowanego blokera (podwójne filtrowanie = podwójny koszt, zero zysku).

### 7.3 Fork adblock-rust: po co i jak mało
Fork trzymamy jako upstream + kilka commitów (np. parametry PL, FFI). Każdy nasz commit = koszt przy bumpie upstreamu. Jeśli potrzeba to tylko FFI – fork nie jest potrzebny, wystarczy nasz crate wrapper. MPL-2.0 wymaga publikacji zmian w plikach crate'a.

## 8. MV2 – realistyczny plan

> **Aktualizacja 2026-10-23 (Chromium 156, zweryfikowane w drzewie):** ungoogled-chromium ma już
> `extensions-manifestv2.patch`, który wyłącza jedyną blokadę MV2 (`ShouldDisableLegacyExtensions()`
> zwraca `false` w `extensions/browser/manifest_v2_handler.cc`). `webRequestBlocking` jest dostępne
> dla MV2 bez ograniczenia do polityk (`_permission_features.json`), a implementacja blokującego
> webRequest i API MV2 (background pages, browserAction, pageAction) są w drzewie. Nasz patch
> `40-mv2/0001` tylko usuwa pozostałości UI wycofania. Punkty 8.1.1–8.1.6 poniżej są więc na teraz
> nieaktualne; zostaje dystrybucja (8.2) i pilnowanie, czy kolejne wersje Chromium nie usuną kodu.

### 8.1 Co faktycznie trzeba odwrócić (stan na 2026)
Weryfikować na konkretnym tagu Chromium (`git log --grep="MV2"` w `extensions/` i `chrome/browser/extensions/`):
1. `extensions/common/extension_features.cc` – feature'y `kExtensionManifestV2*` (Disabled/Unsupported/Deprecation…). Część już usunięta i zastąpiona stałym „MV2 unsupported”.
2. `chrome/browser/extensions/manifest_v2_experiment_manager.*` – maszyna stanów deprecjacji; wyłączenie MV2 jest w niej zahardkodowane.
3. `extensions/common/manifest.cc` / `manifest_handlers` – walidacja `manifest_version >= 3` dla instalacji nie-policy.
4. `extensions/common/api/_permission_features.json` – `webRequestBlocking` ograniczone do `policy` location. **Implementacja blokującego webRequest nadal istnieje**, bo Google deklaruje wsparcie dla rozszerzeń instalowanych politykami enterprise. To jest nasza kotwica: nie przywracamy usuniętego kodu, tylko poszerzamy gating.
5. `chrome/browser/extensions/extension_management.cc` – polityka `ExtensionManifestV2Availability` (była do usunięcia; sprawdzić czy jest).
6. Usunięte flagi `kAllowLegacyMV2Extensions`, `ExtensionManifestV2Disabled` (Chrome 150/151) – przywrócić jako nasze pref/flag w `//plainbrowser`.

Ryzyko: jeśli Google usunie implementację blokującego `webRequest` także dla enterprise, koszt rośnie z „patch gatingu” do „utrzymania martwego podsystemu”. Brave zadeklarował, że będzie to robił – **portujemy ich patche** (brave-core jest MPL-2.0: `brave/patches/*.patch` i `brave/chromium_src/extensions/...`; nasze pliki pochodne z ich kodu zostają MPL). Ustalić kontakt/śledzenie ich PR-ów z labelem MV2.

### 8.2 Dystrybucja rozszerzeń MV2 (bez Chrome Web Store)
- Własne repozytorium CRX na serwerze Omaha (sekcja 6): `update_url` → nasz host. Pakujemy uBO, uMatrix, NoScript, AdGuard z ich GitHub releases, podpisujemy **naszym** kluczem (więc ID różni się od CWS, jak u Brave).
- UI: `chrome://settings/extensions/mv2` – lista kuratorowana z przełącznikami (jak Brave). Instalacja przez `ExtensionInstaller` z naszego CRX, flaga „from Plain Browser store” pozwala `webRequestBlocking` (traktujemy jak location policy w gatingu z 8.1 pkt 4).
- Migracja: wykrywamy ID znanych rozszerzeń z CWS (uBO `cjpalhdlnbpafiamejdnhcphjbkeiagm` itd.), importujemy ustawienia (`chrome.storage.local` → nasz CRX) i podmieniamy – Brave to ma, dobra UX.
- Instalacja dowolnego CRX / unpacked MV2 przez dev mode – zostawiamy, z ostrzeżeniem.

### 8.3 Czego NIE obiecujemy
Pełnej, długoterminowej zgodności MV2 dla dowolnych rozszerzeń. Cel: **kuratorowany zestaw blokerów** działa, reszta „best effort”. To uczciwy zakres i taki sam, jaki komunikuje Brave.

## 9. Pozostałe decyzje produktowe (do podjęcia)
- Safe Browsing: ungoogled usuwa. Opcje: brak (jak ungoogled), własny proxy do Google Safe Browsing Update API v4 (dane idą przez nasz serwer, nie do Google z klienta), lub listy phishingowe w adblock-engine. Rekomendacja: faza 2, własny proxy.
- Crash reporting: Crashpad jest w drzewie; ungoogled wyłącza upload. Włączyć **opt-in** upload minidumpów na własny serwer (Sentry obsługuje minidumpy; symbole z `symbol_level=1`).
- Telemetria: zero domyślnie. Jeśli kiedyś – opt-in, własny endpoint, opisane w polityce.
- Synchronizacja: brak (Chrome Sync wymaga Google). Ewentualnie import/eksport profilu.
- Hasła, autofill lokalnie – działają bez Google.
- Tłumaczenie stron – Google Translate wycięte; alternatywa: brak albo własny backend (LibreTranslate).
- Kodeki: `proprietary_codecs=true`, `ffmpeg_branding="Chrome"` dla H.264/AAC – sprawdzić licencje patentowe (MPEG LA) dla dystrybucji komercyjnej. Widevine (Netflix): wymaga umowy z Google – prawdopodobnie bez.

## 10. Licencje
- Chromium, ungoogled: BSD-3 – zachować NOTICE w `chrome://credits`.
- adblock-rust, kod portowany z brave-core: MPL-2.0 – zmiany w ich plikach publikujemy; nasz własny kod może być zamknięty (file-level copyleft), ale rekomendacja: całe repo publiczne (zaufanie w kategorii „przeglądarka bez śledzenia” bez otwartego kodu nie istnieje).
- Listy filtrów: EasyList GPLv3/CC BY-SA, uBO filters GPLv3 – dystrybucja jako dane, podać atrybucję.

## 11. Fazy i wysiłek (orientacyjnie, zespół 2–3 dev. z doświadczeniem w Chromium)

| Faza | Zakres | Czas |
|---|---|---|
| 0 | Reprodukowalny build stock ungoogled-chromium-windows na naszym CI, podpis, pakowanie | 2–3 tyg. |
| 1 | Branding + install_static + instalator + serwer Omaha + auto-update | 3–4 tyg. |
| 2 | adblock-rust: throttle, cosmetic, listy, UI, DAT z serwera | 5–6 tyg. |
| 3 | MV2: patche gatingu (port Brave), repo CRX, UI kuratorowane, migracja z CWS | 4–6 tyg. + stały koszt |
| 4 | Bot rebase, crash reporting, Safe Browsing proxy, beta publiczna | 3–4 tyg. |

Koszt stały po starcie: ~1 FTE na rebase co 4 tygodnie (większość to MV2 i konflikty z ungoogled), plus infrastruktura (2 buildery Windows, serwer Omaha, CDN na instalatory ~200 MB).

## 12. Największe ryzyka
1. **MV2** – Google może usunąć implementację webRequestBlocking całkowicie; wtedy utrzymujemy ją sami albo polegamy na Brave.
2. **Cadence bezpieczeństwa** – jeśli rebase trwa > 2 tygodnie, wysyłamy użytkownikom znane 0-daye. Automatyzacja z fazy 4 nie jest opcjonalna.
3. **Czas builda** – bez cache i z 1 maszyną nie da się testować PR-ów. Budżet na sprzęt zaplanować od dnia 1.
4. **Podpis kodu / SmartScreen** – nowy certyfikat ma zerową reputację; pierwsze tygodnie użytkownicy zobaczą ostrzeżenia. Azure Trusted Signing skraca to.
5. **Zależność od ungoogled** – jeśli projekt spowolni (bywało), musimy sami nakładać ich patche na nowy Chromium. Plan B: nasza warstwa musi dać się nałożyć bezpośrednio na upstream Chromium.

## 13. Następne kroki
1. Nazwa + domena + certyfikat podpisu (długi lead time).
2. Zamówić/postawić builder Windows; zbudować stock ungoogled-chromium-windows z aktualnego taga (faza 0).
3. Sklonować brave-core i wylistować patche MV2 pod aktualny tag – to ustali realny koszt sekcji 8.
4. Założyć repo wg struktury z sekcji 2, `versions.toml`, pusty `patches/series`.
