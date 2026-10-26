# Polityka bezpieczeństwa

[English](../SECURITY.md)

## Zgłaszanie podatności

Prosimy nie zakładać publicznego zgłoszenia dla problemu bezpieczeństwa. Użyj prywatnego zgłaszania GitHuba dla tego
repozytorium: **zakładka Security, następnie „Report a vulnerability”**. Podaj wersję Plain Browser, kroki do
odtworzenia i to, co według Ciebie zyskałby atakujący. Pierwszej odpowiedzi możesz spodziewać się w ciągu tygodnia.

Jeśli błąd leży w samym Chromium (odtwarza się w niezmienionym ungoogled-chromium albo Chromium tej samej wersji),
zgłoś go projektowi Chromium; ich poprawkę przejmiemy przez nasze śledzenie upstreamu.

## Co utrzymujemy na bieżąco

Plain Browser to cienka warstwa nad ungoogled-chromium, więc większość poprawek bezpieczeństwa przychodzi z nowym
wydaniem Chromium. Naszym celem jest wydanie nowej wersji w ciągu kilku dni od każdego stabilnego tagu upstreamu
(`.github/workflows/upstream-watch.yml` zakłada zgłoszenie, gdy pojawi się nowszy tag). Wspierane jest tylko
najnowsze wydanie.

## Weryfikacja wydania

Wydania mają plik `SHA256SUMS.txt` i bezkluczowy pakiet cosign dla każdego pliku (`.github/workflows/sign-release.yml`):

```
cosign verify-blob --bundle <plik>.bundle \
  --certificate-identity-regexp 'https://github.com/plain-browser/plain-browser/.github/workflows/sign-release.yml@.*' \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com <plik>
sha256sum -c SHA256SUMS.txt
```

Podpis pokazuje, kto opublikował plik i że nie został on później zmieniony. Nie dowodzi, jak plik binarny został
zbudowany. Wspieranym sposobem instalacji Plain Browser jest pakiet MSIX z Microsoft Store: Store podpisuje go
i dostarcza aktualizacje. Pliki dołączone do wydań na GitHubie (archiwum przenośne, instalator) to kompilacje
testowe bez podpisu Authenticode, więc SmartScreen i Smart App Control będą przed nimi ostrzegać lub odmawiać ich
uruchomienia.

Commity na gałęzi `main` są podpisane kluczem SSH opiekuna; `scripts/github-protect-main.sh` ustawia reguły gałęzi
(bez force-push, bez usuwania, liniowa historia, wymagane podpisane commity).
