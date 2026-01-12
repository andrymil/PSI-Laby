# Instrukcja Uruchomienia i Obsługi - Mini TLS

Poniższa instrukcja opisuje krok po kroku proces uruchomienia środowiska, podłączenia się do konsoli serwera i klienta oraz przeprowadzania symulacji komunikacji.

## 1. Uruchomienie Środowiska

Otwórz terminal w folderze głównym projektu i wykonaj komendę, która zbuduje obrazy i uruchomi kontenery w tle:

```bash
docker-compose up -d --build
```

## 2. Obsługa Serwera (Terminal 1)

Serwer działa w tle. Aby wydawać mu polecenia, musisz „wejść” do jego konsoli.

1. Wpisz komendę:

```bash
docker attach z34_tcp_server
```

2. **UWAGA:** Po wykonaniu tej komendy ekran może wydawać się pusty/zacięty. Naciśnij klawisz [ENTER], aby pojawił się znak zachęty `>`

3. Teraz możesz wpisywać komendy serwera:

- `list` – Wyświetla listę połączonych klientów.

- `kill <ip:port>` – Rozłącza konkretnego klienta (np. kill 172.18.0.3:48200).

- `exit` – Wyłącza serwer.

**Jak wyjść bez wyłączania serwera?** Aby odłączyć się od konsoli, ale zostawić serwer działający, naciśnij sekwencję klawiszy: Ctrl+P, a następnie Ctrl+Q.

## 3. Obsługa Klienta (Terminal 2)

Otwórz nowe, osobne okno terminala (lub nową kartę) i wykonaj analogiczne kroki dla klienta.

1. Wpisz komendę:

```bash
docker attach z34_tcp_client
```

2. Podobnie jak przy serwerze, naciśnij [ENTER], jeśli nie widzisz znaku `>`

3. Wpisuj komendy klienta:

- `connect` – Łączy z serwerem i wymienia klucze (Handshake). Powinieneś zobaczyć komunikat: `*** Secure Connection Established! ***`.

- `send <wiadomość>` – Wysyła zaszyfrowany tekst (np. send Tajne haslo).

- `quit` – Kończy połączenie i zamyka program klienta.

## 4. Przykładowy Scenariusz (Krok po kroku)

Aby przetestować działanie protokołu, wykonaj poniższą sekwencję:

1. Terminal 2 (Klient): Wpisz `connect`.

*Efekt*: Zobaczysz logi z wymiany kluczy (ClientHello / ServerHello).

2. Terminal 2 (Klient): Wpisz `send Witaj Serwerze`.

*Efekt*: Klient zaszyfruje wiadomość i wyśle ją do serwera.

3. Terminal 1 (Serwer): Spójrz na logi.

*Efekt*: Serwer powinien odebrać wiadomość, zweryfikować HMAC, odszyfrować ją i wyświetlić: `[IP:Port] Secure Message: Witaj Serwerze`.

4. Terminal 1 (Serwer): Wpisz `list`.

*Efekt*: Zobaczysz adres IP podłączonego klienta.

5. Terminal 2 (Klient): Wpisz `quit`.

*Efekt*: Klient wyśle żądanie EndSession i zakończy pracę.

## 5. Zatrzymanie Projektu
Po zakończeniu testów, aby wyłączyć i usunąć kontenery, wykonaj w dowolnym terminalu w folderze projektu:

```bash
docker-compose down
```