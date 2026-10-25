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
- **Nie łączy się z usługami Google.** Poprawki ungoogled-chromium usuwają je (logowanie Google, Safe Browsing,
  tłumaczenie, Chrome Web Store i inne).
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
