# Scenariusz narracji: segmenty 2, 3, 4, 6, 8, 9, 10, 11, 12

Konwencja jak w `VIDEO_SCRIPT_1_5.md`. Razem około 24 minut, 3 300 słów. Segmenty 1 i 5 są w
`VIDEO_SCRIPT_1_5.md`, segment 7 w `VIDEO_SCRIPT_7.md`. Grafiki do animacji: `docs/video-assets/*.svg`.

---

## Segment 2. Po co własna przeglądarka (1:30–4:30)

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Zacznijmy od pytania, które dostanę w pierwszym komentarzu: po co. Jest Chrome, jest Firefox, jest Brave.
Odpowiedź ma datę: trzydziesty pierwszy sierpnia dwa tysiące dwudziestego szóstego.

**[EKRAN]** Strona Chrome Web Store z uBlock Origin: „To rozszerzenie nie jest już dostępne”. Obok
`chrome://extensions` w Chrome z panelem „Te rozszerzenia nie są już obsługiwane”.

**[OFF]**
Tego dnia sklep Chrome usunął rozszerzenia Manifest V2. Chrome wyłączył je już wcześniej, od wersji sto
trzydzieści dziewięć. Manifest V2 to interfejs, przez który rozszerzenie mogło zatrzymać żądanie sieciowe
zanim wyszło. Bez niego bloker reklam dostaje listę reguł o ograniczonej długości i nie widzi, co się dzieje.
uBlock Origin w pełnej wersji przestał w Chrome istnieć.

**[EKRAN]** Tabela porównawcza, cztery kolumny: Chrome, ungoogled-chromium, Brave, Plain Browser.
Wiersze: bez Google, wbudowany bloker, MV2, aktualizacje dla zwykłego użytkownika, bez własnego ekosystemu.
Wypełniana wiersz po wierszu równo z narracją.

**[OFF]**
Patrzyłem na to, co jest. ungoogled-chromium wycina z Chromium wszystko, co dzwoni do Google. Świetna
robota, ale to projekt dla ludzi, którzy sami ściągną paczkę i sami zaktualizują. Nie ma blokera, nie ma
aktualizacji.

Brave ma bloker w środku i trzyma MV2 przy życiu. Ma też portfel kryptowalut, własne reklamy, własne AI
i własny pomysł na to, czym jest przeglądarka. Niektórym to pasuje. Mnie nie.

Chciałem rzeczy, która nazywa się tak, jak wygląda. Plain. Chromium bez Google, bloker w środku, MV2
działa, aktualizacje przychodzą same, i nic więcej.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Cztery cele, i czwarty jest najważniejszy, choć brzmi najnudniej: mały zestaw patchy. Bo Chromium wydaje
nową wersję co cztery tygodnie, a każda zmiana, którą zrobię w jego kodzie, będę musiał zrobić jeszcze raz.
O tym, dlaczego to przesądza o całej architekturze, za chwilę. Najpierw pytanie, czy w ogóle Chromium.

## Segment 3. Silnik od zera? Nie (4:30–7:30)

**[EKRAN]** Cztery logotypy: Blink/Chromium, Gecko/Firefox, Servo, Ladybird.

**[OFF]**
Jeśli chcesz zrobić przeglądarkę, masz cztery silniki do wyboru i jedną opcję, której nie masz.

Opcja, której nie masz, to napisanie własnego. Silnik to parser HTML, CSS, układ strony, rendering, maszyna
JavaScriptu, stos sieciowy, piaskownica. Sam Blink i V8 to jedna trzecia czasu kompilacji Chromium. Ladybird
pisze silnik od zera od kilku lat, z zespołem i funduszami, i jest ciągle w fazie pre-alpha. Jedna osoba nie
ma na to czasu życia.

**[EKRAN]** Tabela: silnik, język, kto go utrzymuje, rozszerzenia, Windows. Wypełniana wiersz po wierszu.

**[OFF]**
Gecko, czyli Firefox. Dojrzały, ma MV2, ale rozszerzenia Chrome na nim nie działają, a narzędzia do budowania
na Windowsie są drugą kategorią. Servo, silnik w Ruście, przepiękny, bez stosu rozszerzeń i bez przeglądarki
wokół. Ladybird, za wcześnie.

Zostaje Chromium. Największy ekosystem rozszerzeń, oficjalne wsparcie Windowsa, i, co przesądziło,
ungoogled-chromium jako gotowa baza.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Tu jest rzecz, którą zrozumiałem dopiero po przeczytaniu ich repozytorium. ungoogled-chromium nie jest
forkiem. Nie trzyma kopii Chromium. To zestaw patchy, lista plików do wycięcia i lista domen do podmiany,
nakładane skryptem na oryginalną paczkę źródeł Google. Każda nowa wersja Chromium to ponowne nałożenie
tego samego zestawu.

**[EKRAN]** Diagram warstw (`video-assets/01-layers.svg`), budowany od dołu: Chromium, ungoogled-chromium,
ungoogled-chromium-windows, Plain Browser.

**[OFF]**
Więc zrobiłem dokładnie to samo, piętro wyżej. Na dole Chromium, przypięty tag. Na nim ungoogled-chromium.
Na nim ungoogled-chromium-windows, czyli skrypty, które to wszystko budują na Windowsie bez narzędzi Google.
I na samej górze moje repozytorium: patche, jeden katalog własnego kodu, ikony i skrypty.

**[EKRAN]** Terminal: `ls` w repo. `patches/`, `src/plainbrowser/`, `branding/`, `scripts/`, `versions.toml`.
`du -sh` pokazuje kilka megabajtów.

**[OFF]**
Całe repozytorium ma kilka megabajtów. Chromium, które z niego powstaje, ma po rozpakowaniu kilkadziesiąt
gigabajtów. To nie jest kopia. To jest przepis.

## Segment 4. Patche zamiast forka (7:30–12:00)

**[EKRAN]** `versions.toml` w edytorze. Jedna linia z tagiem `156.0.8078.47-1.1`.

**[OFF]**
Przepis zaczyna się od jednej liczby. Tag ungoogled-chromium-windows, a z niego wynika wersja Chromium.
Zmiana tej linii to przejście na nową wersję przeglądarki. Wszystko inne ma się do niej dostosować.

**[EKRAN]** `cat patches/series`. Lista plików z prefiksami 00, 10, 30, 40, 50, 60, 70, 80, 90.

**[OFF]**
Potem seria patchy. Format quilt, ten sam, który czyta narzędzie ungoogled, więc nie musiałem pisać własnego
mechanizmu nakładania. Katalogi mają numery i jedno znaczenie każdy. Zero zero, build: jedyne miejsce,
gdzie wpinam mój katalog do systemu budowania. Dziesięć, branding. Trzydzieści, adblock. Czterdzieści,
MV2. Siedemdziesiąt, Safe Browsing. Osiemdziesiąt, fingerprint. Dziewięćdziesiąt, bezpieczne domyślne
ustawienia.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
I dwie zasady, które trzymają to w ryzach.

Zasada pierwsza: jak najmniej zmian w plikach Chromium, jak najwięcej w moim katalogu. Patch w drzewie
Chromium to koszt przy każdym rebase, bo plik pod spodem się zmienia i patch przestaje pasować. Plik w moim
katalogu kompiluje się dalej, dopóki nie zmieni się interfejs, którego używa. Cały bloker, cała ochrona
fingerprintu, cały interfejs tarczy, to jest około sześćdziesięciu dodanych linii w plikach Chromium.
Reszta jest moja.

Zasada druga: każdy patch w jednej kategorii, z nagłówkiem, który mówi, po co jest. Żeby za pół roku
człowiek, albo skrypt, wiedział, co wolno wyrzucić.

**[EKRAN]** Otwarty patch z `00-build`: kilkanaście linii, dodanie `//plainbrowser` do zależności
w `chrome/browser/BUILD.gn` i `chrome/renderer/BUILD.gn`.

**[OFF]**
Tak wygląda całe wpięcie. Dwie linie w zależnościach, po jednej w procesie przeglądarki i w rendererze.
Od tego momentu GN, system budowania Chromium, widzi mój katalog jak każdy inny.

**[EKRAN]** `patches/upstream-revert.list`. Kilka wpisów dotyczących Safe Browsing.

**[OFF]**
Jest jeszcze trzeci plik, trochę przewrotny. Lista patchy ungoogled, których nie nakładam. ungoogled
wycina Safe Browsing w całości, bo rozmawia z Google. Ja go przywracam, bo chcę ostrzegać przed phishingiem,
tylko kieruję przez własne proxy. Więc zamiast pisać patch, który cofa ich patch, mówię skryptowi: tego
nie nakładaj.

**[EKRAN]** Terminal: `python scripts/check-patches.py --tag 156.0.8078.73`. Wynik: wszystkie patche
nakładają się na sucho, pobrano tylko dotknięte pliki.

**[OFF]**
I narzędzie, które z tego wszystkiego lubię najbardziej. Sprawdzenie patchy na sucho. Skrypt bierze dowolny
tag upstreamu, ściąga z GitHuba tylko te pliki, których dotykają moje patche, i próbuje je nałożyć. Bez
checkoutu Chromium, bez czterdziestu gigabajtów. Kilkanaście sekund. CI robi to przy każdej zmianie w
patchach i przy każdym nowym tagu upstreamu.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
To jest cały trik. Nie mam forka Chromium. Mam listę zmian, którą potrafię sprawdzić w kilkanaście sekund
na każdej wersji, zanim poświęcę siedem godzin na budowanie. A skoro o tym mowa.

## Segment 6. Branding (16:00–18:30)

**[EKRAN]** `chrome://version` z napisem „Chromium”. Cięcie na ten sam ekran z „Plain Browser”.

**[OFF]**
Skompilowany Chromium nazywa się Chromium. Żeby nazywał się inaczej, trzeba dotknąć zaskakująco wielu
miejsc.

**[EKRAN]** Lista pojawiająca się punkt po punkcie: ikony, stringi w interfejsie, GUID instalacji, klucze
rejestru, ProgID, katalog profilu, nazwa pliku exe, instalator, nowa karta, domyślna wyszukiwarka.

**[OFF]**
Ikony, oczywiście. Każdy string, w którym pojawia się słowo Chromium. Identyfikatory instalacji, żeby mój
browser nie nadpisał cudzego Chromium i żeby Windows wiedział, że to osobna aplikacja. Klucze rejestru.
Katalog profilu, u mnie LOCALAPPDATA, PlainBrowser. Nazwa exe. Instalator.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Stringi to pułapka, w którą wpadłem. Chromium ma tłumaczenia na kilkadziesiąt języków, a tłumaczenie jest
dopasowywane do angielskiego tekstu przez jego skrót. Zmienisz słowo Chromium na Plain Browser w angielskim
i każde tłumaczenie tego zdania przepada. Polski interfejs nagle mówi po angielsku.

**[EKRAN]** `scripts/apply-branding.py` w edytorze, docstring. Terminal: uruchomienie na drzewie, lista
przeniesionych tłumaczeń.

**[OFF]**
Więc skrypt liczy skrót przed zmianą i po, podmienia tekst i przenosi każde tłumaczenie pod nowy
identyfikator, z podmienioną nazwą. Z wyjątkiem „The Chromium Authors” i podobnych, które zostają, bo to
atrybucja, nie branding.

**[EKRAN]** Pasek zadań Windows z ikoną Plain Browser. Explorer: `%LOCALAPPDATA%\PlainBrowser`.

**[OFF]**
Nazwa produktu jest w jednym pliku, tym samym, co wersja. Skrypt ją czyta. Gdybym jutro zmienił zdanie
co do nazwy, zmieniam jedną linię.

**[DO KAMERY]**
Teraz rzecz, dla której to wszystko powstało.

## Segment 8. MV2, fingerprint, utm_ (24:00–28:00)

**[EKRAN]** `chrome://extensions`, tryb dewelopera. Rozpakowany katalog uBlock Origin 1.75 ładowany
z dysku. Pojawia się ikona uBO na pasku.

**[OFF]**
uBlock Origin, wersja dla Chromium, Manifest V2, z webRequestBlocking. Załadowany z pliku, bo sklep go już
nie ma. Działa. Żadnego panelu o nieobsługiwanych rozszerzeniach.

**[EKRAN]** Strona testowa z ośmioma żądaniami: cztery hosty z EasyList, dwa tylko z listy Petera Lowe'a,
dwa kontrolne. Wynik: sześć zablokowanych, dwa przeszły.

**[OFF]**
Test, który to sprawdza, celowo wyłącza mój wbudowany bloker i ładuje stronę z hostami, które są tylko na
listach uBO. Jeśli są zablokowane, to zrobiło to rozszerzenie przez webRequest. Są.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Jak to możliwe, skoro Google to usunęło? Odpowiedź: nie usunęło. Google wyłączyło. Implementacja
blokującego webRequest nadal jest w Chromium, bo Google obiecało ją firmom, które instalują rozszerzenia
przez polityki. Mój patch nie przywraca usuniętego kodu. Poszerza warunek, pod którym ten kod wolno użyć.

Ryzyko jest jasne. Od wersji około sto pięćdziesiątej Google zaczyna wycinać kod naprawdę. Wtedy to przestanie
być jeden patch, a stanie się utrzymywaniem martwego podsystemu. Brave zadeklarował, że będzie to robił.
Ich kod jest na MPL, więc będę portował ich patche. To stały koszt, nie jednorazowa flaga.

**[EKRAN]** Pasek adresu. Wklejony link z `?utm_source=newsletter&utm_campaign=x&fbclid=abc`. Enter.
Pasek pokazuje czysty adres. Historia też.

**[OFF]**
Druga rzecz: parametry śledzące. utm, fbclid, gclid. Throttle nawigacji zatrzymuje ją, pyta silnik
o przepisany adres, i jeśli się różni, anuluje nawigację i wykonuje ją od nowa z czystym adresem. Pasek
adresu i historia nigdy nie widzą tych parametrów. Serwer też nie.

**[EKRAN]** Link `l.facebook.com/l.php?u=https://example.org/...` kliknięty. Pasek od razu pokazuje
`example.org`. Log lokalnego serwera: brak wejścia na `/l.php`.

**[OFF]**
I przekierowania. Facebook, Google, Outlook Safe Links, Bing, LinkedIn, Reddit. Link przez ich serwer, który
zapisuje kliknięcie i dopiero odsyła dalej. Jeśli cel jest w adresie, przeglądarka idzie prosto do niego.
Serwer przekierowujący nigdy nie dowiaduje się o kliknięciu. Tylko wtedy, gdy cel jest w adresie: bit.ly
decyduje po stronie serwera, tego nie da się ominąć.

**[EKRAN]** Strona testowa fingerprintu: `hardwareConcurrency: 2`, renderer WebGL „Intel, ANGLE”, hash canvas.
Obok to samo w drugiej sesji: inny hash canvas.

**[OFF]**
Trzecia: fingerprint. Strona może cię rozpoznać bez ciasteczek po tym, jak twój komputer rysuje canvas,
jak brzmi audio, jaką masz kartę graficzną, ile rdzeni. Mój skrypt wchodzi do każdej strony przed jej
pierwszym skryptem i owija te interfejsy. Canvas dostaje szum w trzech procentach pikseli, ten sam dla tej
samej strony przez całą sesję, inny po restarcie. Karta graficzna to dla każdego ten sam Intel. Dwanaście
rdzeni to dwa.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Haczyk: bank. Bank używa fingerprintu do wykrywania oszustw i jeśli urządzenie wygląda inaczej przy każdym
logowaniu, dostajesz dodatkową weryfikację albo blokadę. Więc jest lista zgodności, na start polskie banki
i płatności, które dostają prawdziwe wartości. I ta sama tarcza, co przy blokerze: jeden przełącznik na
pasku wyłącza wszystkie trzy ochrony dla strony, która się psuje.

Pierwsza wersja działa jako skrypt na poziomie strony, bez patchy w Blinku. Ma to swoje granice, da się ją
wykryć. Wersja w silniku jest w planach.

## Segment 9. Testy i rebase co 4 tygodnie (28:00–31:00)

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Pora na mniej efektowną prawdę. Własna przeglądarka to w dziewięćdziesięciu procentach utrzymanie.
Nie pisanie funkcji. Sprawdzanie, czy to, co napisałem miesiąc temu, nadal działa na Chromium, którego
miesiąc temu nie było.

**[EKRAN]** Terminal na VM: `ls scripts/test-*.ps1`. Dwanaście plików. Uruchomienie `test-shields-ui.ps1`:
otwiera się okno, kursor sam klika tarczę, pojawia się bubble, zrzut ekranu.

**[OFF]**
Testy. Dwanaście skryptów na VM, każdy odpala przeglądarkę, podnosi lokalny serwer i sprawdza jedną rzecz.
Adblock, tarcza, parametry, fingerprint, Safe Browsing, aktualizacja list, MV2. Te od interfejsu naciskają
kontrolki przez UI Automation, czyli tak, jak robi to czytnik ekranu, a klik myszą tylko wtedy, gdy nie
ma innego wyjścia. Do tego testy jednostkowe C++ i testy crate'a w Ruście.

**[EKRAN]** Diagram rytmu rebase'u (`video-assets/04-rebase-cycle.svg`): tag upstreamu → check-patches →
bump versions.toml → build 7 h → testy → paczka. Pętla co 4 tygodnie.

**[OFF]**
Rytm wygląda tak. ungoogled-chromium-windows wydaje nowy tag. Akcja w CI zauważa go, nakłada moje patche
na sucho i wkleja wynik do issue. Jeśli pasuje, zmieniam linię w versions.toml. Siedem godzin builda.
Testy. Paczka. Jeśli nie pasuje, poprawiam patch, a check-patches mówi mi w kilkanaście sekund, czy już
pasuje, zanim dotknę builda.

**[EKRAN]** GitHub Actions: `patches-check.yml` zielone, `upstream-watch.yml`, issue z wynikiem.
`build-windows.yml` z łańcuchem etapów build-1 … build-10.

**[OFF]**
Build w GitHub Actions jest napisany, ale jeszcze nie uruchomiony. Runner ma cztery rdzenie i limit sześciu
godzin na zadanie, więc build jest pocięty na etapy, każdy zapisuje drzewo jako artefakt i następny je
podnosi. Dziesięć etapów. Na razie buduje VM.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
I skrypt, który uruchamiam z największym niepokojem: sprawdzenie, czy moja wersja nie jest za poprawkami
bezpieczeństwa Chromium. Bo przeglądarka, która nie dostaje poprawek, jest gorsza niż Chrome z Google.
To jest powód, dla którego dystrybucja wygląda tak, a nie inaczej.

## Segment 10. MSIX i Microsoft Store (31:00–33:00)

**[EKRAN]** Explorer: trzy pliki, `PlainBrowser_0.1.3-alpha.1_x64.zip`, instalator, `.msix`.

**[OFF]**
Z builda wychodzą trzy paczki. Zip przenośny, instalator i MSIX, format pakietów Windows. Publikuję tylko
ten trzeci.

**[CIĘCIE]** Kamera.

**[DO KAMERY]**
Dlaczego tylko Store? Bo ungoogled wycina mechanizm aktualizacji Google, a ja muszę dostarczać poprawki
co tydzień. Mam dwie drogi. Własny updater, własny serwer, własny certyfikat Authenticode, który kosztuje
kilkaset euro rocznie i wymaga klucza sprzętowego, i który przez pierwsze miesiące i tak będzie straszył
SmartScreenem. Albo Store, który podpisuje paczkę za mnie i dostarcza aktualizacje swoim kanałem.

Wybrałem Store. Tracę użytkowników, którzy nie chcą Store. Zyskuję to, że nie muszę pisać i zabezpieczać
updatera, który jest najbardziej atakowanym elementem każdej przeglądarki.

**[EKRAN]** `scripts/package-msix.py` w edytorze. Terminal: budowanie paczki, manifest z nazwą
i identyfikatorem.

**[OFF]**
MSIX składa skrypt w Pythonie: manifest, ikony, katalog z przeglądarką. Store wymaga, żeby aplikacja nie
zapisywała poza swoim profilem i nie dotykała rejestru w sposób, którego nie da się odinstalować. Chromium
w trybie użytkownika spełnia to z natury.

**[EKRAN]** Partner Center, szkic listingu (dane konta zamazane). Strona plain-browser.app z polityką
prywatności.

**[OFF]**
Do listingu trzeba tekstów, zrzutów i polityki prywatności pod publicznym adresem. Polityka jest krótka,
bo nie ma czego opisywać: nic nie wychodzi z komputera poza listami filtrów i Safe Browsing przez moje
proxy, oba opisane co do nagłówka.

## Segment 11. Co nie wyszło (33:00–34:30)

**[CIĘCIE]** Kamera, bliżej.

**[DO KAMERY]**
Stan na dziś, szczerze. Wersja zero jeden trzy alpha jeden. Build niepodpisany, listing w Store w
przygotowaniu. Nietestowane: interfejs na prawdziwych stronach poza testowymi, instalacja ze Store na
czystej maszynie.

Nie zrobione: kuratorowane repozytorium rozszerzeń MV2, bo skoro sklep ich nie ma, ktoś musi je hostować.
Szum fingerprintu w silniku zamiast skryptu. Cache builda. Build w CI zamiast na mojej wirtualce.

**[EKRAN]** `git log --oneline` z commitem „Revert crash: opt-in crash reports”.

**[DO KAMERY]**
Jedna rzecz, którą cofnąłem. Napisałem opcjonalne raporty o awariach, na mój serwer. I wycofałem to przed
wydaniem, bo nie miałem jeszcze polityki, która mówi, co w tym raporcie jest i jak długo go trzymam.
Przeglądarka, która obiecuje, że nic nie wysyła, nie może mieć wyjątków, których nie opisała.

Koszt: wirtualka z dwunastoma rdzeniami przez kilka miesięcy, po kilka godzin na każdy rebase, i jeden
miesiąc na przepisanie scriptletów, żeby licencje się zgadzały.

## Segment 12. Outro (34:30–35:00)

**[EKRAN]** Repozytorium na GitHubie, strona plain-browser.app, plik LICENSE.

**[OFF]**
Kod jest na MPL, patche na BSD, wszystko w repozytorium z opisu. Jeśli znajdziesz błąd bezpieczeństwa,
w SECURITY.md jest, jak go zgłosić.

**[DO KAMERY]**
Następny film: Chromium sto pięćdziesiąt siedem wychodzi za kilka tygodni. Zrobię rebase na żywo.
Zobaczymy, ile patchy pęknie.

**[EKRAN]** Plansza końcowa.

---

## Uwagi do nagrania

- Windows na nagrywanej maszynie aktywowany (bez znaku wodnego „Aktywuj system Windows” w rogu);
  szczegóły w `VIDEO_PLAN.md`, lista nagrań.
- Segment 3: `du -sh` repo nagrać bez `website/node_modules` i `dist/`.
- Segment 4: `check-patches.py` uruchomić na tagu nowszym niż pinowany (np. `.73`), żeby pokazać sens.
- Segment 8: test MV2 wymaga `--disable-features=PlainBrowserAdblock`, inaczej wbudowany bloker przejmie
  cztery z sześciu hostów i wynik nic nie udowodni.
- Segment 8: strona banku z listy zgodności tylko jako ekran logowania, bez wpisywania danych.
- Segment 10: Partner Center nagrywać z zamazanym identyfikatorem wydawcy i e-mailem.
- Segment 11: ton spokojny, bez usprawiedliwiania się; lista braków czytana z kartki jest w porządku.
