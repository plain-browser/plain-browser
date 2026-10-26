# Polityka prywatności Plain Browser

Ostatnia aktualizacja: 25 października 2026 · [English](PRIVACY.md)

Plain Browser to przeglądarka zbudowana z Chromium z zestawem poprawek ungoogled-chromium. Ta polityka opisuje, co
przeglądarka robi z Twoimi danymi. Dotyczy wersji 0.1.0-alpha.1 i jest aktualizowana przy każdym wydaniu, które to
zmienia.

## W skrócie

- Plain Browser **nie ma telemetrii, statystyk użycia ani raportów o awariach**, które trafiałyby do nas lub do
  kogokolwiek innego.
- Nie ma **konta, logowania ani synchronizacji** i nie prowadzimy **żadnego serwera**, który odbierałby dane z
  przeglądarki. Nie widzimy, co przeglądasz.
- **Nie łączy się z usługami Google.** Poprawki ungoogled-chromium usuwają je (logowanie Google, tłumaczenie,
  Chrome Web Store i inne). Jedynym planowanym wyjątkiem jest Safe Browsing przez nasze własne anonimizujące
  proxy, opisane niżej; w tej wersji nie jest aktywne.
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
Polish) są dołączone do przeglądarki. Bloker niczego nie pobiera i nikomu nie zgłasza, co blokuje.

## Safe Browsing

Safe Browsing ostrzega przed otwarciem strony znanej ze złośliwego oprogramowania lub phishingu. Plain Browser
jest zaprojektowany tak, by oferować to bez przekazywania Google czegokolwiek o Tobie:

- **Nieaktywne w wersji 0.1.0-alpha.1.** Safe Browsing jest wykompilowany z tej wersji, więc przeglądarka nie
  wysyła żadnych żądań Safe Browsing. Poniższe punkty opisują projekt, który trafi do przyszłej wersji; polityka
  zostanie wtedy zaktualizowana.
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
- Safe Browsing będzie można wyłączyć w ustawieniach; wtedy przeglądarka nie wysyła do naszego proxy niczego.

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
