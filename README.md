#ChatTCP - Secure E2EE Communicator

Wielowątkowy komunikator sieciowy oparty na architekturze klient-serwer, wykorzystujący gniazda TCP (Winsock). Aplikacja kładzie silny nacisk na bezpieczeństwo – implementuje pełne szyfrowanie End-to-End (E2EE) przesyłanych wiadomości oraz bezpieczne przechowywanie danych logowania.

Główne funkcjonalności

Architektura Klient-Serwer: Asynchroniczna obsługa wielu klientów jednocześnie przy użyciu wątków (std::thread) i mechanizmów synchronizacji (std::mutex, std::condition_variable).

Szyfrowanie End-to-End (E2EE): Wykorzystanie biblioteki libsodium do bezpiecznej komunikacji. Wiadomości są szyfrowane lokalnie u nadawcy przy pomocy klucza publicznego odbiorcy (crypto_box_easy) i mogą być odszyfrowane wyłącznie przez niego. Serwer pośredniczy jedynie w przekazywaniu zaszyfrowanego ciągu znaków (Zero-Knowledge).

Bezpieczeństwo Haseł: Hasła użytkowników nie są przechowywane w formie jawnej. Użyto nowoczesnego algorytmu haszującego Argon2id (crypto_pwhash), zintegrowanego z libsodium.

Trwała Baza Danych: Serwer wykorzystuje wbudowaną bazę SQLite3 do trwałego przechowywania identyfikatorów, haszy haseł oraz kluczy publicznych użytkowników.

Pokrycie Testami: Logika biznesowa, struktury danych oraz algorytmy kryptograficzne zostały przetestowane przy użyciu frameworka Google Test (GTest).


Wymagania i Zależności:

Projekt został stworzony dla środowiska Windows. Do kompilacji i uruchomienia wymagane są:

Kompilator wspierający standard C++17.

Biblioteka libsodium (szyfrowanie).

Biblioteka SQLite3 (baza danych).

Framework Google Test (do uruchomienia testów jednostkowych).

Linkowanie z systemową biblioteką ws2_32.lib (Windows Sockets 2).

Kompilacja

Projekt zawiera gotowy skrypt wsadowy .bat dla kompilatora g++ (MinGW). Skrypt kompiluje zarówno serwer, klienta, jak i zestaw testów jednostkowych, dołączając framework GTest w locie.

@echo off
echo Kompilacja serwera...
g++ -DSODIUM_STATIC -g server.cpp -o server.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo Kompilacja klienta...
g++ -DSODIUM_STATIC -g client.cpp -o client.exe -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread

echo Kompilacja testow...
g++ -DSODIUM_STATIC -DRUNNING_TESTS -g tests.cpp client.cpp server.cpp googletest/src/gtest-all.cc -o run_tests.exe -Igoogletest/include -Igoogletest -Ilibsodium-win64/include -Llibsodium-win64/lib -lsodium -lws2_32 -lsqlite3 -lpthread


💻Użycie

Uruchomienie serwera: Zawsze uruchamiaj serwer jako pierwszy. Wygeneruje on (jeśli nie istnieje) plik bazy danych users.db i zacznie nasłuchiwać na porcie 8080.

server.exe


Uruchomienie klienta: Uruchom jedną lub więcej instancji klienta w osobnych oknach terminala.

client.exe


Dostępne komendy w kliencie:

register [nick] [hasło] - Rejestracja nowego użytkownika. Generuje parę kluczy kryptograficznych i zapisuje klucz prywatny lokalnie w folderze %APPDATA%\ChatTCP\.

login [nick] [hasło] - Logowanie istniejącego użytkownika.

msg [nick_odbiorcy] [treść_wiadomości] - Wysyłanie zaszyfrowanej wiadomości prywatnej do wskazanego użytkownika. Klient automatycznie pobierze klucz publiczny odbiorcy z serwera.

logout - Wylogowanie i bezpieczne zamknięcie programu.

Testy

Aby uruchomić testy weryfikujące struktury danych, ograniczenia walidacyjne oraz poprawność działania szyfrowania w bibliotece libsodium, wykonaj skompilowany plik testowy:

run_tests.exe


Struktura Projektu

server.cpp - Główna pętla serwera, obsługa gniazd, integracja z bazą danych i zarządzanie połączonymi użytkownikami.

client.cpp - Aplikacja konsolowa użytkownika z asynchronicznym odbieraniem wiadomości oraz szyfrowaniem/deszyfrowaniem.

header.h - Współdzielone struktury danych (np. klasa Message) używane do parsowania pakietów sieciowych.

tests.cpp - Kompletny zestaw testów jednostkowych (GTest) weryfikujących najważniejsze składowe architektury.
