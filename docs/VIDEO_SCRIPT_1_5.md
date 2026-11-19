# Scenariusz narracji: segment 1 (Hook) i segment 5 (Build)

Konwencja: **[EKRAN]** to, co widz widzi; **[CIĘCIE]** zmiana ujęcia; tekst zwykły to narracja do kamery lub
z offu. Tempo około 150 słów na minutę. Liczby pochodzą z `docs/BUILD_CACHE.md` (zmierzone na VM 12 vCPU).

---

## Segment 1. Hook i obietnica (0:00–1:30)

**[EKRAN]** Pełny ekran. Plain Browser, pusta karta. Kursor wpisuje adres popularnego polskiego portalu
z dużą liczbą reklam. Enter.

**[OFF]**
To jest przeglądarka, którą zbudowałem sam.

**[EKRAN]** Strona się ładuje. Zbliżenie na ikonę tarczy na pasku. Licznik: 3… 11… 27… 41.

**[OFF]**
Ta liczba w rogu to żądania do serwerów reklamowych, które właśnie nie wyszły z komputera. Nie blokuje ich
rozszerzenie. Blokuje je sama przeglądarka, w miejscu, gdzie żądanie dopiero powstaje.

**[CIĘCIE]** `chrome://version`. Widać etykietę „Plain Browser” i numer Chromium 156.0.8078.47 (ungoogled-chromium). Kadrować bez prawego górnego rogu: tam nadal jest logo Chromium (luka w brandingu).

**[OFF]**
Pod spodem jest Chromium. Ten sam silnik co w Chrome. Ale nie ma tu konta Google, nie ma synchronizacji,
nie ma telemetrii. Jedyna rzecz, z którą ta przeglądarka rozmawia w tle, to moje własne serwery.

**[CIĘCIE]** Kamera. Prowadzący przy biurku, w tle monitor z terminalem.

**[DO KAMERY]**
Nie napisałem silnika przeglądarki. Nikt rozsądny tego nie robi i za chwilę pokażę dlaczego. Zrobiłem coś
innego: wziąłem czterdzieści osiem tysięcy plików źródłowych Chromium, dołożyłem do nich jeden katalog
własnego kodu i kilkanaście łatek. I przez to ta rzecz na ekranie jest moja.

W tym filmie pokażę trzy rzeczy. Jak dopisać kod do Chromium tak, żeby nie edytować Chromium. Jak przeżyć
kompilację, która trwa siedem godzin. I jak zrobić to tak, żeby za cztery tygodnie, kiedy Google wyda
następną wersję, nie zaczynać od zera.

**[EKRAN]** Szybki montaż, po sekundzie: timelapse `ninja`, bubble z przełącznikiem per site, uBlock Origin
zainstalowany jako MV2, paczka MSIX w Explorerze.

**[OFF]**
Będzie trochę Rusta, dużo C++ i jedna decyzja, która kosztowała mnie miesiąc.

**[EKRAN]** Plansza tytułowa: „Zbudowałem własną przeglądarkę”. 2 sekundy.

---

## Segment 5. Pierwszy build: 7 godzin (12:00–16:00)

**[EKRAN]** Terminal PowerShell na VM. Wpisane `scripts\build.ps1`. Enter. Pierwsze linie logu.

**[OFF]**
Mamy patche, mamy własny kod. Teraz trzeba to skompilować. I tutaj zaczyna się prawdziwy koszt własnej
przeglądarki.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Chromium kompiluje się na Windowsie przez Visual Studio, kompilatorem clang-cl. Nie używam depot_tools,
czyli narzędzi Google, bo ungoogled-chromium-windows ma własne skrypty, które pobierają paczkę źródeł
i uruchamiają ninja. Mój skrypt build.ps1 tylko je opakowuje: nakłada nasze patche, kopiuje katalog
plainbrowser do drzewa i odpala budowanie.

Maszyna: wirtualka z dwunastoma wirtualnymi rdzeniami. Nie jest to stacja robocza za trzydzieści tysięcy.
To jest coś, co można wynająć.

**[EKRAN]** Timelapse ninja, przyspieszony tak, żeby trwał około 40 sekund. W rogu licznik kroków
`[1234/59500]` i zegar ścienny. Na dole pasek: 0 h → 7 h.

**[OFF]**
Pełny build to pięćdziesiąt dziewięć i pół tysiąca kroków. Na tej maszynie: siedem godzin zegarowych.
Sześćdziesiąt dziewięć godzin czasu procesora. Ninja trzyma zajętych około dziesięciu z dwunastu rdzeni,
więcej się nie da, bo część kroków czeka na inne.

Z tych siedmiu godzin: czterdzieści osiem tysięcy siedemset obiektów C++. To jest dziewięćdziesiąt siedem
procent całego czasu. Blink i V8, czyli silnik renderowania i silnik JavaScriptu, to jedna trzecia.

**[EKRAN]** Wykres kołowy: C++ 97 %, w tym Blink + V8 35 %. Reszta: mojom, grit, Rust, linkowanie, razem
poniżej 3 %. Osobny mały wycinek „Rust 0,2 %”.

**[OFF]**
Wszystko inne, generowanie kodu, zasoby, Rust, linkowanie, mieści się w trzech procentach. Nasz bloker
w Ruście to zero przecinek dwa procent. Jeśli ktoś mówi, że Rust spowalnia build Chromium, to nie na tym
etapie.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Siedem godzin raz, dobrze, da się przeżyć. Problem zaczyna się, kiedy zmienisz jedną linijkę.

**[EKRAN]** Edytor. Plik z naszymi stringami UI. Dodana jedna linia: nowa etykieta przycisku. Zapis.
Terminal: `ninja` startuje. Licznik rośnie do około 835.

**[OFF]**
Dodałem jedną etykietę w interfejsie. Jeden string. Ninja przekompilował osiemset trzydzieści pięć plików.
Dwie godziny czasu procesora, od dwunastu do trzydziestu pięciu minut czekania.

Dlaczego? Nasze stringi są częścią wielkiego pliku zasobów Chromium, generated_resources. Z niego powstaje
jeden nagłówek z identyfikatorami. A ten nagłówek includuje osiemset dwadzieścia trzy pliki w chrome/browser.
Zmienisz jeden identyfikator, nagłówek się regeneruje, osiemset plików uznaje, że są nieaktualne.

**[EKRAN]** Diagram: `generated_resources.grd` → `generated_resources.h` → 823 strzałek do plików `.cc`.
Obok druga wersja: `plainbrowser_strings.grd` → własny nagłówek → kilkadziesiąt strzałek.

**[OFF]**
Rozwiązanie jest tanie i jeszcze go nie zrobiłem: własny plik zasobów dla naszych stringów, ładowany jako
osobny pakiet. Wtedy zmiana etykiety dotyka kilkudziesięciu plików, które naprawdę jej używają, a nie
ośmiuset.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
I trzeci scenariusz, najgorszy. Co cztery tygodnie Google wydaje nową wersję Chromium. Co tydzień poprawkę.
Nowa wersja to nowa paczka źródeł. Każdy plik ma nową datę modyfikacji, więc ninja z marszu
przekompilowałby wszystko. Siedem godzin od nowa.

**[EKRAN]** GitHub, compare `156.0.8078.47...156.0.8078.73`: 299 commitów, 294 zmienione pliki.

**[OFF]**
Sprawdziłem, ile naprawdę zmienia taka poprawka. Dwieście dziewięćdziesiąt cztery pliki. Z tego dla C++
liczy się trzydzieści dziewięć nagłówków i sto dziewięć źródeł.

Ale jeden z tych nagłówków to hash_table z Blinka. Podstawowa struktura danych, includowana wszędzie.
Osiem tysięcy trzysta obiektów. Razem z innymi, które mogą się zmienić, ponad dziesięć tysięcy.
Trzydzieści siedem procent czasu kompilacji. Za trzydzieści dziewięć nagłówków.

**[EKRAN]** Tabela z dwoma wierszami: „39 zmienionych nagłówków: 8 486 obiektów, 21,3 h CPU (31 %)”,
„plus nagłówki generowane: 10 169 obiektów, 25,4 h CPU (37 %)”.

**[OFF]**
Z cache kompilacji, który patrzy na treść pliku, a nie na datę, taki bump skraca się z siedmiu godzin do
jakichś dwóch i pół, trzech. To jest szacunek, nie pomiar, bo cache jeszcze nie stoi. Ale to pokazuje, że
większość tych siedmiu godzin to praca, którą ninja robi drugi raz bez powodu.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Wniosek z tego segmentu jest jeden. Własna przeglądarka to nie jest problem „jak to napisać”. To jest
problem „jak to przebudować”. Każda decyzja projektowa, o której będę mówił dalej, w tym to, gdzie leży nasz
kod i czego nie dotykamy w drzewie Chromium, wynika z tego, że każda pomyłka kosztuje godziny.

A teraz: jak sprawić, żeby ten skompilowany Chromium wiedział, że nazywa się Plain Browser.

**[EKRAN]** Przejście do segmentu 6: `chrome://version` z napisem „Chromium” zmienia się w „Plain Browser”.

---

## Uwagi do nagrania

- Windows na nagrywanej maszynie aktywowany (bez znaku wodnego „Aktywuj system Windows” w rogu);
  szczegóły w `VIDEO_PLAN.md`, lista nagrań.

- Timelapse z segmentu 5 trzeba nagrać przed wszystkim innym: pełny build na VM z OBS (1 klatka / 5 s).
  Na nagraniu musi być widoczny licznik kroków ninja i zegar systemowy.
- Rebuild po zmianie stringu (835 plików) nagrać osobno, przyspieszony do 10 sekund.
- Liczby w narracji są wypowiadane słownie, na ekranie pokazywane cyframi. Jeśli przed nagraniem zmierzysz
  nowe wartości (np. po wdrożeniu cache), zaktualizuj obie wersje.
- Segment 1 ma działać także bez dźwięku: badge z liczbą, `chrome://version` i plansza tytułowa niosą
  całą treść.
