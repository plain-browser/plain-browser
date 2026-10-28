# Polityka prywatności Plain Browser

Ostatnia aktualizacja: 28 października 2026 · [English](PRIVACY.md)

Plain Browser to przeglądarka zbudowana z Chromium z zestawem poprawek ungoogled-chromium. Ta polityka opisuje, co
przeglądarka robi z Twoimi danymi. Dotyczy wersji 0.1.3-alpha.1 i jest aktualizowana przy każdym wydaniu, które to
zmienia.

## W skrócie

- Plain Browser **nie ma telemetrii, statystyk użycia ani raportów o awariach**, które trafiałyby do nas lub do
  kogokolwiek innego.
- Nie ma **konta, logowania ani synchronizacji** i nie prowadzimy **żadnego serwera**, który odbierałby z
  przeglądarki dane o Tobie. Nie widzimy, co przeglądasz. Dwie nasze usługi, z którymi łączy się przeglądarka
  (proxy Safe Browsing i serwer list filtrów, opisane niżej), nie prowadzą logów i nie dostają nic o odwiedzanych
  stronach.
- **Nie łączy się z usługami Google.** Poprawki ungoogled-chromium usuwają je (logowanie Google, tłumaczenie,
  Chrome Web Store i inne). Jedynym wyjątkiem jest Safe Browsing przez nasze własne anonimizujące
  proxy, opisane niżej; można go wyłączyć.
- Dane o przeglądaniu zostają **na Twoim urządzeniu**.

## Co zbieramy

Nic. Nie otrzymujemy, nie zbieramy, nie przechowujemy ani nie sprzedajemy danych osobowych. W przeglądarce nie ma
analityki, identyfikatora reklamowego ani kodu śledzącego.

Sprawdziliśmy to dla pierwszego uruchomienia: przy świeżym profilu i bez otwartej strony Plain Browser przez 100
sekund nie wysłał żadnych własnych żądań sieciowych. Jedyną aktywnością było automatyczne wykrywanie serwera proxy
w Windows (WPAD), które wykonuje każda aplikacja korzystająca z systemowych ustawień proxy.

## Dane przechowywane na urządzeniu

Jak każda przeglądarka, Plain Browser zapisuje w profilu użytkownika Windows historię, ciasteczka, pamięć podręczną,
zapisane hasła, zakładki, pobrane pliki i ustawienia. Te dane nie są do nas wysyłane. Możesz je usunąć w ustawieniach
przeglądarki (Wyczyść dane przeglądania) albo całkowicie, odinstalowując przeglądarkę. W wersji z Microsoft Store
Windows trzyma profil w prywatnej pamięci aplikacji i usuwa go przy odinstalowaniu.

## Dane wysyłane do odwiedzanych stron

Gdy otwierasz stronę, przeglądarka wysyła jej to, co potrzebne do załadowania, w tym Twój adres IP i standardowe
nagłówki żądań. Tak działa sieć, a do tych danych stosuje się polityka prywatności danej strony.

Wbudowany bloker reklam działa w całości na Twoim urządzeniu. Listy filtrów (EasyList, EasyPrivacy, EasyList
Polish) są dołączone do przeglądarki i, o ile nie wyłączysz aktualizacji, odświeżane z naszego serwera (następna
sekcja). Bloker nikomu nie zgłasza, co blokuje.

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
  krótki manifest z sumą kontrolną każdej listy i pobiera listę tylko wtedy, gdy się zmieniła. Nigdy nie łączy się
  z autorami list; to serwer pobiera listy od nich.
- Żądania **nie zawierają plików cookie ani niczego o odwiedzanych stronach**, tylko numer wersji przeglądarki
  (`X-Client: PlainBrowser/<major.minor.patch>`, ten sam nagłówek, który dostaje proxy Safe Browsing). Przed
  użyciem przeglądarka sprawdza rozmiar i skrót SHA-256 każdej pobranej listy z manifestem.
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
  sprawdzane z nią na Twoim urządzeniu. Dopiero gdy prefiks pasuje, przeglądarka pyta o pełne hashe tego prefiksu,
  co nie zdradza adresu strony.
- Te zapytania trafiają do **naszego proxy pod adresem sb.plain-browser.app** (Cloudflare Worker), nigdy
  bezpośrednio do Google. Proxy przekazuje je do Google z naszym kluczem API i usuwa wszystko, co Cię
  identyfikuje: adres IP, ciasteczka, `Referer`, `Accept-Language` i nagłówki Cloudflare. Google widzi adres IP
  Cloudflare, stały `User-Agent` i prefiksy hashy.
- Jedyne, co przeglądarka dodaje, to nagłówek `X-Client: PlainBrowser/<wersja>`. Nie jest wysyłany żaden
  identyfikator instalacji, użytkownika ani numer kompilacji Chromium.
- **Proxy nie prowadzi logów.** Rejestrowanie żądań i obserwowalność są wyłączone, a proxy nie ma żadnej pamięci.
  Nie widzimy, o jakie prefiksy pytasz. Połączenie obsługuje jednak Cloudflare i widzi Twój adres IP tak, jak host
  każdej strony; stosuje się do tego jego polityka prywatności.
- Ochrona rozszerzona, sprawdzanie adresów w czasie rzeczywistym i rozszerzone raportowanie, które wysyłałyby pełne
  adresy i treść stron, są wyłączone i nie da się ich włączyć.
- Safe Browsing można wyłączyć w ustawieniach (Ty i Google → Usługi Google albo Prywatność i bezpieczeństwo →
  Bezpieczeństwo); wtedy przeglądarka nie wysyła do naszego proxy niczego.

## Rozszerzenia

Rozszerzenia, które instalujesz, mogą odczytywać i wysyłać dane zgodnie z nadanymi im uprawnieniami. Odpowiadają za
to ich autorzy, a nie Plain Browser, i to oni mają własne polityki prywatności.

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
