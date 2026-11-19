# Scenariusz narracji: segment 7. Adblock w środku przeglądarki (18:30–24:00)

Konwencja jak w `VIDEO_SCRIPT_1_5.md`: **[EKRAN]**, **[CIĘCIE]**, **[DO KAMERY]**, **[OFF]**. Około 5:30,
820 słów. Szczegóły techniczne z `docs/ADBLOCK.md` (stan 2026-10-28).

Uwaga do planu: w `VIDEO_PLAN.md` silnik opisany jest jako „serwis w procesie przeglądarki”. Od 28.10 silnik
działa we **własnym procesie pomocniczym w piaskownicy**. Narracja poniżej mówi o aktualnej wersji; zaktualizuj
diagram z listy nagrań (pkt 10).

---

**[EKRAN]** Chrome. Strona uBlock Origin w Chrome Web Store: komunikat, że rozszerzenie nie jest już dostępne.

**[OFF]**
Tak dziś wygląda blokowanie reklam w Chrome. Rozszerzenie, które miało pełny wgląd w ruch sieciowy,
straciło interfejs, z którego korzystało. Nie dlatego, że przestało działać. Dlatego, że przeglądarka
przestała mu pozwalać.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Wniosek był dla mnie prosty. Jeśli bloker ma przeżyć kolejne decyzje Google, nie może być rozszerzeniem.
Musi siedzieć w przeglądarce, obok stosu sieciowego, i decydować o żądaniu zanim ono w ogóle wyjdzie.
Tak robi Brave. I tak zrobiłem ja, zresztą na tym samym silniku.

**[EKRAN]** GitHub, repozytorium brave/adblock-rust. Zbliżenie na opis: Rust, licencja MPL-2.0.

**[OFF]**
Ten silnik to adblock-rust. Napisany w Ruście, rozumie składnię EasyList, czyli tę samą, którą czyta uBlock
Origin. Parsuje listy, buduje z nich struktury do szybkiego dopasowania i odpowiada na jedno pytanie: czy
to żądanie, z tej strony, tego typu, ma przejść.

Problem w tym, że Chromium to C++, a Rust w Chromium ma swoje zasady.

**[EKRAN]** Drzewo `src/plainbrowser/adblock/`: katalogi `rs`, `engine`, `browser`, `renderer`, `common`,
`lists`, `resources`. Otwarty `rs/Cargo.toml`.

**[OFF]**
Nie mogłem po prostu zbudować biblioteki przez cargo i dolinkować. Chromium ma własną bibliotekę
standardową Rusta i własny alokator pamięci. Druga biblioteka standardowa w tym samym procesie to proszenie
się o kłopoty. Więc crate'y trzeba zwendorować do drzewa i opisać w GN, systemie budowania Chromium.

**[EKRAN]** Terminal: `scripts\vendor-adblock-crates.ps1`. Przewija się `gnrt vendor`, `gnrt gen`.
Potem `post-gnrt-fixups.ps1`.

**[OFF]**
Jest do tego narzędzie, gnrt. Działa, ale po drodze zabija pliki konfiguracyjne, nadpisuje cudze patche,
dziewięć crate'ów wymaga zgody na unsafe, a jeden chce funkcji z nocnego kompilatora. Każdą z tych rzeczy
odkrywałem osobno. Dziś to jeden skrypt i drugi, który sprząta po pierwszym.

Most między Rustem a C++ robi biblioteka cxx. Jeden szczegół: cxx zwraca błędy przez wyjątki C++, a Chromium
kompiluje się bez wyjątków. Więc każda funkcja mostu zwraca zwykłego boola.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Teraz najważniejsze: gdzie ten silnik żyje. Pierwsza wersja miała go w procesie przeglądarki. Wczoraj to
zmieniłem. Dlaczego? Bo listy filtrów przychodzą z sieci. Parser, który czyta dane z sieci, w procesie,
który trzyma całą przeglądarkę, to zły pomysł. Jeśli coś w nim pęknie, pęka wszystko.

**[EKRAN]** Diagram, budowany krok po kroku, równo z narracją:
1. prostokąt „Renderer (karta)” po lewej;
2. prostokąt „Proces przeglądarki” w środku;
3. prostokąt „Proces adblocka, piaskownica kService” po prawej;
4. strzałka żądania z renderera: throttle → przeglądarka → silnik → odpowiedź „blok / przepuść”.
Menedżer zadań Windows pokazuje proces „Plain Browser ad blocker”.

**[OFF]**
Dziś wygląda to tak. Strona w rendererze chce pobrać skrypt. Zanim żądanie wyjdzie, mój throttle je
zatrzymuje i pyta proces przeglądarki. Ten przekazuje pytanie dalej, do osobnego procesu, w którym działa
adblock-rust. Proces ma najostrzejszą piaskownicę, jaką Chromium daje procesom pomocniczym. Tę samą, co
usługa pamięci. Nie widzi dysku, nie widzi sieci. Parsuje listy i odpowiada.

Jeśli ten proces padnie, przeglądarka traci blokowanie reklam do następnego zapytania, które uruchamia go
na nowo. Z jednym zabezpieczeniem: pobrane listy są wtedy odrzucane, bo to one mogły go położyć, i wraca
do tych wbudowanych. Maksymalnie cztery starty na sesję.

**[EKRAN]** Fragment `docs/ADBLOCK.md`: „with the filter pipe disconnected, deferred requests hung forever;
the throttle now skips disconnected filters and fails open after 3 s”.

**[OFF]**
I jeszcze jedna zasada, której nauczył mnie pierwszy test. Bloker zawsze zawodzi w stronę przepuszczenia.
W pierwszej wersji, kiedy połączenie z silnikiem padło, zatrzymane żądania wisiały w nieskończoność. Strona
nigdy się nie ładowała. Dziś throttle czeka maksymalnie trzy sekundy i puszcza. Lepiej zobaczyć jedną
reklamę niż pustą kartę.

**[CIĘCIE]** Terminal na VM. `scripts\test-adblock.ps1`.

**[OFF]**
Tak to sprawdzam. Test headless ładuje stronę, która sięga do czterech hostów reklamowych i kilku
kontrolnych. Cztery zablokowane, kontrolne przechodzą.

**[EKRAN]** Ten sam test z `--disable-features=PlainBrowserAdblock`. Wszystkie żądania przechodzą.

**[OFF]**
Z wyłącznikiem awaryjnym, jedną flagą w wierszu poleceń, wszystko przechodzi. Ta flaga jest po to, żeby
w razie problemu dało się wyłączyć bloker bez przebudowy. Pamiętacie, ile trwa przebudowa.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Blokowanie żądań to połowa roboty. Druga połowa to to, co strona robi, kiedy reklama nie przyjdzie.

**[EKRAN]** Strona z pustym białym prostokątem po reklamie. Obok ta sama strona w Plain Browser, bez dziury.

**[OFF]**
Po pierwsze, ukrywanie elementów. EasyList ma około trzynastu tysięcy reguł typu „schowaj element z klasą
ad-box”. Agent w rendererze wstrzykuje je jako arkusz stylów użytkownika i dopytuje o klasy, które strona
naprawdę ma. Każdy selektor w osobnej regule, bo Blink wyrzuca całą regułę, kiedy jeden selektor jest
nieznany. Na tym straciłem kiedyś całe ukrywanie na jednej stronie.

**[EKRAN]** `src/plainbrowser/adblock/resources/`: lista plików `noop.js`, `1x1.gif`,
`googletagmanager_gtm.js`, `set-constant.js`, `json-prune.js`…

**[OFF]**
Po drugie, zamienniki. Wiele stron się wywala, kiedy Google Tag Manager albo Analytics nie wczyta się
wcale. Więc zamiast blokować, podstawiam atrapę: skrypt, który wygląda jak Analytics, wywołuje callbacki,
a nic nie wysyła. Do tego scriptlety, małe skrypty uruchamiane przed pierwszym skryptem strony, które
na przykład ustawiają stałą albo wycinają fragment JSON-a.

Tu jest haczyk licencyjny. Zasoby uBlock Origin są na GPL. Mój kod jest na MPL. Więc wszystkie te atrapy
i scriptlety, dziewiętnaście sztuk, napisałem od nowa. To była ta decyzja za miesiąc, o której mówiłem
na początku.

**[EKRAN]** Ustawienia > Prywatność i bezpieczeństwo. Przełącznik „Aktualizuj listy filtrów codziennie”,
pod nim „Listy sprawdzone: <data>”.

**[OFF]**
I po trzecie, listy muszą być świeże. Raz dziennie przeglądarka pyta mój serwer, Cloudflare Workera bez
logów, o manifest. Manifest jest podpisany kluczem Ed25519. Bez poprawnego podpisu nic nie wchodzi.
Pobierane są tylko listy, których hash się zmienił, każda sprawdzona rozmiarem i SHA-256. Nowy silnik
buduje się w tle, około sekundy, stary odpowiada do końca, potem zamiana.

**[EKRAN]** Pasek narzędzi. Klik w tarczę. Bubble: nazwa strony, przełącznik „Ochrona włączona dla tej
strony”, licznik blokad.

**[OFF]**
Dla użytkownika to wszystko sprowadza się do jednej tarczy na pasku. Liczba to żądania zablokowane na tej
stronie. Przełącznik wyłącza ochronę dla tej domeny. Jedna lista wyjątków, czytana przez bloker,
stripping parametrów i ochronę fingerprintu.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Czego nie ma: blokowania całych nawigacji, czyszczenia parametrów w zasobach podrzędnych, trzech
operatorów proceduralnych i kilku scriptletów. Wszystko spisane w dokumentacji jako „nie zrobione”.
Wolę krótką listę tego, co działa, niż długą tego, co podobno działa.

A skoro mowa o rozszerzeniach, które Google wyrzuciło: zobaczmy, czy uBlock Origin da się tu jeszcze
zainstalować.

**[EKRAN]** Przejście do segmentu 8: okno `chrome://extensions` z włączonym trybem dewelopera.

---

## Uwagi do nagrania

- Windows na nagrywanej maszynie aktywowany (bez znaku wodnego „Aktywuj system Windows” w rogu);
  szczegóły w `VIDEO_PLAN.md`, lista nagrań.

- Diagram trzech procesów to najważniejsza grafika w filmie: budować go warstwami zgodnie z narracją,
  nie pokazywać gotowego.
- Menedżer zadań Windows z procesem „Plain Browser ad blocker” nagrać na realnej sesji, nie headless.
- Test `test-adblock.ps1` z flagą i bez: dwa osobne nagrania, przyspieszone, każde do 8 sekund.
- Porównanie „dziura po reklamie vs bez dziury” wymaga strony, która zostawia pusty kontener; sprawdzić
  wcześniej na kilku portalach.
- „Decyzja za miesiąc” z hooka to własne scriptlety na MPL. Jeśli w hooku zmieni się to zdanie, zmienić tu.
