# Polityka prywatności Plain Browser

Ostatnia aktualizacja: 19 listopada 2026 · [English](PRIVACY.md)

Plain Browser to przeglądarka zbudowana z Chromium z zestawem poprawek ungoogled-chromium. Ta polityka opisuje, co
przeglądarka robi z Twoimi danymi. Dotyczy wersji 0.2.4-beta.2 i jest aktualizowana przy każdym wydaniu, które to zmienia.

## W skrócie

- Plain Browser **nie ma telemetrii, statystyk użycia ani raportów o awariach**, które trafiałyby do nas lub do
  kogokolwiek innego.
- Nie ma **konta, logowania ani synchronizacji** i nie prowadzimy **żadnego serwera, który odbiera dane o
  odwiedzanych stronach**. Nie widzimy, co przeglądasz. Trzy nasze usługi, z którymi łączy się przeglądarka (proxy
  Safe Browsing, serwer list filtrów i repozytorium rozszerzeń, opisane niżej), są skonfigurowane tak, by nie
  zapisywać logów, i nie dostają nic o odwiedzanych stronach. Cloudflare, który je obsługuje, widzi jednak Twój
  adres IP przy połączeniu.
- **Nie łączy się bezpośrednio z Google.** Poprawki ungoogled-chromium usuwają logowanie Google, tłumaczenie,
  Chrome Web Store i inne usługi. Wyjątkiem jest Safe Browsing, który przechodzi przez nasze anonimizujące proxy:
  Google dostaje od proxy prefiksy hashy adresów, nigdy Twojego IP. Można to wyłączyć.
- Dane o przeglądaniu zostają **na Twoim urządzeniu**.

## Co zbieramy

Nic. Nie otrzymujemy, nie zbieramy, nie przechowujemy ani nie sprzedajemy danych osobowych. W przeglądarce nie ma
analityki, identyfikatora reklamowego ani kodu śledzącego.

Zmierzyliśmy wcześniejszą wersję: na świeżym profilu, bez otwartej strony, przez 100 sekund nie wysłała żadnych
własnych zapytań sieciowych poza automatycznym wykrywaniem proxy Windows (WPAD), które wykonuje każda aplikacja
używająca systemowych ustawień proxy. Obecna wersja wysyła pierwsze własne zapytania po kilku minutach od startu:
sprawdzenie list filtrów (około dwóch minut po starcie), aktualizacje list Safe Browsing przez nasze proxy oraz
zapytania DNS, szyfrowane, gdy Twój resolver to obsługuje. Pierwsze dwie rzeczy możesz wyłączyć w ustawieniach.

## Dane przechowywane na urządzeniu

Jak każda przeglądarka, Plain Browser zapisuje w profilu użytkownika Windows historię, ciasteczka, pamięć podręczną,
zapisane hasła, zakładki, pobrane pliki i ustawienia. Te dane nie są do nas wysyłane. Możesz je usunąć w ustawieniach
przeglądarki (Wyczyść dane przeglądania) albo całkowicie, odinstalowując przeglądarkę. W wersji z Microsoft Store
Windows trzyma profil w prywatnej pamięci aplikacji i usuwa go przy odinstalowaniu.

Bloker reklam przechowuje też na Twoim urządzeniu: Twoje własne filtry (pole „My filters” na chrome://adblock,
zapisane w pliku ustawień przeglądarki), listę witryn, dla których wyłączyłeś ochronę, pobrane listy filtrów (w
folderze w Twoim profilu) oraz najwyższy numer manifestu, jaki przyjął, żeby nie dało się podstawić starszej kopii.
Nic z tego nie jest wysyłane do nas. Wyłączenie aktualizacji list usuwa pobrane listy; „Wyczyść dane przeglądania”
nie usuwa własnych filtrów ani wyjątków dla witryn, które edytujesz w ustawieniach i na chrome://adblock.

## Dane wysyłane do odwiedzanych stron

Gdy otwierasz stronę, przeglądarka wysyła jej to, co potrzebne do załadowania, w tym Twój adres IP i standardowe
nagłówki żądań. Tak działa sieć, a do tych danych stosuje się polityka prywatności danej strony.

Przeglądarka informuje też każdą stronę, że nie zgadzasz się na sprzedaż ani udostępnianie Twoich danych, sygnałem
**Global Privacy Control** (nagłówek `Sec-GPC: 1` i `navigator.globalPrivacyControl`). Według przepisów takich jak
kalifornijska CCPA strony muszą go respektować; gdzie indziej jest prośbą. Jest domyślnie włączony; można go
wyłączyć w Ustawieniach > Prywatność i bezpieczeństwo, na stronie plików cookie innych firm (przełącznik prośby do
witryn o rezygnację z wykorzystania danych).

Plain Browser zmienia też to, czego niektóre strony mogłyby się o Tobie dowiedzieć, wyłącznie na Twoim urządzeniu.
Usuwa znane parametry śledzące (np. `utm_*`, `fbclid`, `gclid`) z otwieranych adresów, więc strona ich nie dostaje,
i pomija znane przekierowania śledzące (np. `l.facebook.com/l.php`), przechodząc od razu do właściwej strony. Ochrona
fingerprintu dodaje niewielkie wariacje, osobne dla witryny i sesji, do wyników canvas, audio i WebGL oraz kilku
wartości przeglądarki, żeby skrypty nie mogły ich użyć do rozpoznania Cię między witrynami. Obie funkcje można
wyłączyć dla witryny, która przestaje działać, przyciskiem tarczy na pasku. Ochrona fingerprintu to pierwsza wersja:
strona może ją wykryć i obejść, i nie obejmuje każdej metody.

Wbudowany bloker reklam działa w całości na Twoim urządzeniu. Listy filtrów (EasyList, EasyPrivacy, EasyList
Polish, EasyList Cookie List, a po włączeniu także Fanboy's Annoyance List i uBlock filters) są dołączone do
przeglądarki i, o ile nie wyłączysz aktualizacji, odświeżane z naszego serwera (następna sekcja). Bloker nikomu nie
zgłasza, co blokuje. Jego dziennik żądań na chrome://adblock zapisuje tylko wtedy, gdy ta strona jest otwarta,
jest przechowywany wyłącznie w pamięci i czyszczony po jej zamknięciu.

## Bezpieczny DNS

Żeby znaleźć stronę, przeglądarka pyta serwer DNS o jej adres. Plain Browser korzysta z serwera DNS, którego już
używa Twoja sieć albo Windows. Jeśli ten serwer udostępnia też szyfrowany punkt DNS-over-HTTPS (na przykład
Cloudflare albo Quad9), przeglądarka wysyła tam zapytania zaszyfrowane zamiast jawnym tekstem; w przeciwnym razie
pyta serwer tak jak wcześniej. Nie dochodzi żaden inny dostawca DNS, a my nie dostajemy Twoich zapytań. Możesz to
wyłączyć albo wybrać dostawcę w Ustawienia > Prywatność i bezpieczeństwo > Bezpieczeństwo > „Używaj bezpiecznego
DNS”.

## Aktualizacje list filtrów

Od wersji 0.1.3-alpha.1 przeglądarka odświeża listy filtrów między wydaniami:

- Raz na dobę, kilka minut po uruchomieniu, pyta **nasz serwer lists.plain-browser.app** (Cloudflare Worker) o
  krótki manifest z sumą kontrolną każdej listy i pobiera listę tylko wtedy, gdy się zmieniła (a listę opcjonalną
  tylko wtedy, gdy jest włączona). Nigdy nie łączy się
  z autorami list; to serwer pobiera listy od nich.
- Żądania **nie zawierają plików cookie ani niczego o odwiedzanych stronach**, tylko numer wersji przeglądarki
  (`X-Client: PlainBrowser/<major.minor.patch>`, ten sam nagłówek, który dostaje proxy Safe Browsing). Przed
  użyciem przeglądarka sprawdza rozmiar i skrót SHA-256 każdej pobranej listy z manifestem. Odrzuca też manifest
  starszy niż ten, który już przyjęła, albo po terminie ważności, i zostaje przy aktualnych listach.
- **Serwer nie prowadzi logów** i przechowuje tylko listy. Połączenie obsługuje Cloudflare, który widzi Twój adres
  IP, tak jak przy każdej obsługiwanej przez siebie stronie.
- **Można to wyłączyć**: Ustawienia > Prywatność i bezpieczeństwo > Aktualizuj listy filtrów codziennie. Po
  wyłączeniu przeglądarka nie wysyła tych żądań, usuwa pobrane listy i używa list z dnia wydania.

## Safe Browsing

Safe Browsing ostrzega przed otwarciem strony znanej ze złośliwego oprogramowania lub phishingu. Plain Browser
oferuje to bez przekazywania Google czegokolwiek o Tobie:

- **Aktywne od wersji 0.1.2-alpha.1**, wyłącznie w ochronie standardowej. Przeglądarka wysyła żądania Safe Browsing
  do naszego proxy i nigdzie indziej; nigdy nie łączy się bezpośrednio z Google.
- Przeglądarka trzyma **lokalną bazę prefiksów hashy** (tzw. Update API, ochrona standardowa). Odwiedzane adresy są
  sprawdzane z nią na Twoim urządzeniu. Dopiero gdy prefiks pasuje, przeglądarka pyta o pełne hashe tego prefiksu.
  Zapytanie zawiera krótki prefiks hasha, nie adres, ale prefiks zawęża krąg możliwych adresów, więc to częściowa
  informacja, a nie brak informacji.
- Te zapytania trafiają do **naszego proxy pod adresem sb.plain-browser.app** (Cloudflare Worker), nigdy
  bezpośrednio do Google. Proxy przekazuje je do Google z naszym kluczem API i usuwa wszystko, co Cię
  identyfikuje: adres IP, ciasteczka, `Referer`, `Accept-Language` i nagłówki Cloudflare. Google widzi adres IP
  Cloudflare, stały `User-Agent` i prefiksy hashy.
- Jedyne, co przeglądarka dodaje, to nagłówek `X-Client: PlainBrowser/<wersja>`. Nie jest wysyłany żaden
  identyfikator instalacji, użytkownika ani numer kompilacji Chromium.
- **Proxy nie zapisuje żądań.** Rejestrowanie żądań i obserwowalność są wyłączone, a proxy nie ma żadnej pamięci
  trwałej. Worker przetwarza jednak prefiksy hashy w locie, w pamięci, żeby przekazać je do Google, więc to
  stwierdzenie o tym, jak jest skonfigurowany i napisany (kod jest w `infra/safebrowsing-proxy`), a nie coś, co
  możesz sprawdzić z zewnątrz. Połączenie obsługuje też Cloudflare i widzi Twój adres IP tak, jak host każdej
  strony; stosuje się do tego jego polityka prywatności.
- Ochrona rozszerzona, sprawdzanie adresów w czasie rzeczywistym i rozszerzone raportowanie, które wysyłałyby pełne
  adresy i treść stron, są ukryte w ustawieniach i nie mają działającego punktu docelowego: nasze proxy obsługuje
  tylko listy prefiksów hashy, a pozostałe adresy są wyłączone. Można je nadal włączyć przez politykę
  przedsiębiorstwa albo plik preferencji, ale z założenia nic nie trafiłoby do Google (nie było to testowane).
- Ochrona obejmuje **adresy stron** (złośliwe oprogramowanie, wyłudzanie danych, niechciane oprogramowanie). Nie
  sprawdza pobieranych plików ani rozszerzeń: list dla nich nie da się pobrać z publicznym kluczem API, więc nie są
  pobierane.
- Safe Browsing można wyłączyć w ustawieniach (Ty i Google → Usługi Google albo Prywatność i bezpieczeństwo →
  Bezpieczeństwo); wtedy przeglądarka nie wysyła do naszego proxy niczego.

## Rozszerzenia

Rozszerzenia, które instalujesz, mogą odczytywać i wysyłać dane zgodnie z nadanymi im uprawnieniami. Odpowiadają za
to ich autorzy, a nie Plain Browser, i to oni mają własne polityki prywatności.

Strona **chrome://recommended-extensions** oferuje uBlock Origin, AdGuard i NoScript, które udostępniamy
na **ext.plain-browser.app** (Cloudflare Worker), bo Chrome Web Store nie jest częścią przeglądarki:

- Nic nie jest pobierane, dopóki nie klikniesz Zainstaluj (lub Przełącz). Pobranie nie zawiera plików cookie,
  tylko numer wersji przeglądarki (`X-Client`).
- Po instalacji mechanizm aktualizacji rozszerzeń Chromium co kilka godzin sprawdza na ext.plain-browser.app, czy
  jest nowa wersja, tak jak Chrome sprawdza Web Store. Zapytanie zawiera to, co Chromium umieszcza w każdym
  sprawdzeniu aktualizacji: identyfikatory i wersje tych rozszerzeń, wersję przeglądarki, system operacyjny i jego
  architekturę oraz język przeglądarki. Nie zawiera nic o odwiedzanych stronach. Po ich usunięciu sprawdzanie ustaje.
- **Repozytorium nie prowadzi logów** i przechowuje tylko pliki rozszerzeń. Cloudflare widzi Twój adres IP, jak
  przy każdej obsługiwanej przez siebie stronie.

## Microsoft Store

Jeśli instalujesz Plain Browser z Microsoft Store, Microsoft przetwarza informacje o instalacji i aktualizacjach
zgodnie z [oświadczeniem o ochronie prywatności Microsoft](https://privacy.microsoft.com/privacystatement). Od
Microsoftu otrzymujemy wyłącznie zbiorcze liczby z Partner Center (np. liczbę pobrań), a nie informacje o Tobie.

## Dzieci

Plain Browser jest przeglądarką ogólnego przeznaczenia. Nie zbiera żadnych danych osobowych, także od dzieci.
Nie filtruje treści stron, więc kontrolę rodzicielską należy ustawić w Windows.

## Zmiany i kontakt

Jeśli polityka się zmieni, nowy tekst zostanie tu opublikowany z nową datą. Pytania i prośby:
https://github.com/plain-browser/plain-browser/issues
