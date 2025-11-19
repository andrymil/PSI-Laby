## Laboratorium PSI

- Miłosz Andryszczuk
- Aleksander Paliwoda
- Maksymilian Zieliński

### Struktura repozytorium

Kolejne zadania znajdują się w osobnych, odpowiednio nazwanych folderach. Każdy folder zawiera:

- Sprawozdanie w formacie PDF.

- Implementację serwera i klienta w dedykowanych podfolderach.

- Plik docker-compose.yml.

### Sposób uruchomienia

Aby uruchomić dowolne zadanie należy:

1. Sklonować repozytorium na serwer bigubu,
2. Przejść do folderu wybranego zadania, np.:

    ```cd PSI-Laby/zad1_1```
3. Uruchomić środowisko poleceniem

    ```docker compose up --build```

Przed uruchomieniem powyższego polecenia warto wykonać `docker compose down`, aby upewnić się, że poprzednie kontenery zostały poprawnie wyłączone i usunięte.

Po zbudowaniu obrazów, kontenery klienta i serwera wystartują automatycznie, wyświetlając swoje logi w konsoli.
