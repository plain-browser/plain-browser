# Konfiguracja GitHuba i kont zewnętrznych: lista rzeczy do zrobienia ręcznie

Stan 2026-10-26. Wszystko poniżej wymaga uprawnień właściciela organizacji `plain-browser` lub konta w Partner
Center, których asystent nie ma. Kolejność ma znaczenie tylko tam, gdzie napisano.

## 1. Zabezpieczenie gałęzi `main` i podpisy commitów (jednorazowo, 5 minut)

```bash
gh auth refresh -h github.com -s admin:ssh_signing_key
bash scripts/github-protect-main.sh
```

Skrypt tworzy ruleset `protect-main` (zakaz usuwania i force-push, historia liniowa, wymagane podpisy) i rejestruje
`~/.ssh/id_ed25519.pub` jako klucz **podpisujący** (osobna lista od kluczy do logowania). Od tej chwili commity
z tego komputera pokazują się jako „Verified”. Uwaga: wymóg podpisów blokuje commity robione przez interfejs
GitHuba w przeglądarce, chyba że włączysz vigilant mode; to zamierzone.

Nie włączaj wymaganej recenzji pull requestów, dopóki nie ma drugiej osoby: pojedynczy maintainer nie mógłby
niczego scalić.

## 2. Ustawienia bezpieczeństwa repozytorium (Settings → Code security)

Adres: https://github.com/plain-browser/plain-browser/settings/security_analysis

- **Private vulnerability reporting: włącz.** `SECURITY.md` już odsyła zgłaszających do zakładki Security.
- **Dependabot alerts** i **Dependabot security updates**: włącz. Plik `.github/dependabot.yml` jest w repo
  i obejmuje GitHub Actions oraz crate'y Rusta w `src/plainbrowser/adblock/rs`.
- **Secret scanning** i **Push protection**: włącz.

## 3. Organizacja `plain-browser` (Settings organizacji)

- **Wymóg 2FA dla wszystkich członków**: Settings → Authentication security → „Require two-factor authentication”.
- **Drugi właściciel.** Jedno konto właściciela to pojedynczy punkt awarii (utrata telefonu, blokada konta).
  Dodaj zaufaną osobę jako Owner albo przynajmniej zapisz kody odzyskiwania 2FA w menedżerze haseł.
- **Konto bota do wydań** (opcjonalnie, gdy zaczniemy publikować regularnie): osobne konto typu machine user
  z rolą Write tylko do tego repozytorium, albo GitHub App. Tokeny CI nigdy na koncie osobistym.
- **Domyślne uprawnienia tokena workflow**: Settings → Actions → General → „Read repository contents and packages
  permissions” oraz zaznacz „Require approval for all outside collaborators” dla uruchomień z forków.

## 4. Pierwsze uruchomienie workflow

- `upstream-watch`: Actions → upstream-watch → „Run workflow”. Powinien zakończyć się bez issue (tag w
  `versions.toml` jest aktualny) albo otworzyć issue z wynikiem `check-patches.py` dla nowszego taga.
- `website`: potrzebuje zmiennych repozytorium `GCP_WORKLOAD_IDENTITY_PROVIDER` i `GCP_SERVICE_ACCOUNT`
  (Settings → Secrets and variables → Actions → Variables) oraz opcjonalnie `CLOUDFLARE_ZONE_ID` (zmienna)
  i `CLOUDFLARE_API_TOKEN` (sekret). Konfiguracja GCP i Cloudflare jest opisana w `website/README.md`.
- `build-windows`: patrz `docs/CI.md`. Pierwsze uruchomienie zrób ręcznie z profilem `alpha` i sprawdź czasy
  etapów; dopiero potem używaj tagów `v*`.
- `sign-release`: uruchamiany ręcznie na gotowym wydaniu z plikami; pierwszy przebieg na prawdziwym wydaniu jest
  jednocześnie jego testem.

## 5. Microsoft Store (Partner Center)

Decyzja z 2026-10-26: Plain Browser jest dystrybuowany **wyłącznie jako MSIX przez Microsoft Store**. Store
podpisuje paczkę i dostarcza aktualizacje, więc nie kupujemy certyfikatu Authenticode i nie piszemy własnego
updatera.

1. Konto deweloperskie Partner Center (indywidualne, opłata jednorazowa) → zarezerwuj nazwę „Plain Browser”.
2. Z karty **Product identity** przepisz `Package/Identity/Name`, `Package/Identity/Publisher` i
   `Publisher display name` do wywołania `scripts/package-msix.py` (parametry `--identity-name`, `--publisher`,
   `--publisher-display`). Paczkę wysyłamy **niepodpisaną**; ostatnia część wersji musi być `0`.
3. Teksty listingu są w `docs/STORE_LISTING.md`, grafiki w `branding/icons/store` i `branding/icons/msix`.
4. Polityka prywatności: adres `https://plain-browser.app/privacy` (strona generowana z `docs/PRIVACY.md`).
5. Kategoria wiekowa i deklaracje: przeglądarka wyświetla dowolne treści z internetu; zaznacz to w kwestionariuszu
   IARC.
6. Po certyfikacji sprawdź na czystej maszynie, że aktualizacja Store faktycznie wchodzi (Microsoft Store →
   Biblioteka → Pobierz aktualizacje).

## 6. Co z tego wynika dla aktualizacji

Aplikacje ze Store aktualizują się automatycznie (ustawienie „Aktualizacje aplikacji” w Microsoft Store, domyślnie
włączone), zwykle w ciągu doby od zakończenia certyfikacji. Przeglądarka w trakcie działania nie jest podmieniana;
nowe pliki wchodzą przy kolejnym uruchomieniu. Ograniczenia: certyfikacja trwa od kilku godzin do kilku dni, więc
poprawka bezpieczeństwa Chromium dociera do użytkowników później niż przy własnym kanale; użytkownik, który
wyłączył automatyczne aktualizacje Store, nie dostanie nic. Jeśli kiedyś wrócimy do dystrybucji poza Store,
potrzebny będzie własny updater (`docs/ARCHITECTURE.md`, sekcja 6) i certyfikat.

Sprawy otwarte (2026-10-27):

1. **Przeglądarka otwarta tygodniami nie dostanie aktualizacji.** Store czeka, aż aplikacja się zamknie, a w
   środku nie ma podpowiedzi „jest nowa wersja, uruchom ponownie” (mechanizm Chromium, `UpgradeDetector`, wykrywa
   nową wersję na dysku, której przy MSIX nie ma, dopóki paczka nie wejdzie). Kierunek: aplikacja pakowana z
   `runFullTrust` może sama pytać Store przez `Windows.Services.Store.StoreContext`
   (`GetAppAndOptionalStorePackageUpdatesAsync`) i zainicjować pobranie z restartem
   (`RequestDownloadAndInstallStorePackageUpdatesAsync`). Niezweryfikowane w kodzie; do sprawdzenia, zanim trafi
   do planu.
2. **Przepustowość builda, nie Store, ogranicza tempo łatek.** Chromium stable wydaje poprawki bezpieczeństwa co
   1–2 tygodnie; każda to pełny build (na VM z 9 GB RAM około doby z przerwami, `scripts/build.ps1`), CI i
   certyfikacja. Regularne aktualizacje wymagają albo więcej pamięci dla VM, albo buildów w GitHub Actions
   (`docs/CI.md`, napisane, jeszcze nie uruchomione).
3. **Wycofanie złej wersji** w Store to wgranie kolejnej; „cofnij” nie istnieje. Partner Center ma stopniowe
   wdrażanie paczki (procent użytkowników): włączać je przy każdym wydaniu i podnosić dopiero po sprawdzeniu, że
   aktualizacja wchodzi (punkt 6 w sekcji 5).
