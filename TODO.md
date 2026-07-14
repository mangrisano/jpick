# minijson — TODO / roadmap

Un mini `jq` scritto in C++20 per imparare il linguaggio.
Regola del gioco: **il codice C++ lo scrivo io (l'utente)**, il mentore (Copilot)
mi guida, mi spiega i concetti e rilegge il codice. Il mentore NON scrive il tool.

Build: CMake + clang++, standard C++20, warning `-Wall -Wextra -Wpedantic`.

---

## Comandi utili

```zsh
cd ~/Coding/stuff/minijson
cmake -S . -B build          # configura (una volta sola, o dopo modifiche al CMakeLists)
cmake --build build          # compila
echo '{"hello": "world"}' | ./build/minijson   # esegui
```

---

## Roadmap (milestone piccole)

### [x] M0 — Scheletro che compila

Obiettivo: un "cat" minimale. Impari: build CMake, I/O da stdin.

- [x] Creare `src/main.cpp`.
- [x] `main` legge TUTTO lo stdin e lo ristampa identico su stdout.
- [x] Compila senza warning.
- [x] `echo '{"hello":"world"}' | ./build/minijson` ristampa l'input.

Indizi:

- Includi `<iostream>` e `<string>`.
- Per leggere tutto lo stdin: costruisci una `std::string` da
  `std::istreambuf_iterator<char>(std::cin)` (inizio) a
  `std::istreambuf_iterator<char>()` (fine, default).
- Stampa con `std::cout`. `return 0;` alla fine.

### [x] M1 — Modello dati JSON

Obiettivo: un tipo che rappresenta un valore JSON. Impari: `std::variant`, tipi ricorsivi.

- [x] Un valore JSON può essere: null, bool, numero, stringa, array, oggetto.
- [x] Array = lista di valori; oggetto = mappa stringa→valore (ricorsivo!).

### [x] M2 — Lexer

Obiettivo: dal testo ai token. Impari: parsing, enum, gestione stringhe.

- [x] Token: `{ } [ ] : ,`, stringa, numero, true/false/null.
- [x] Salta gli spazi bianchi.

### [x] M3 — Parser

Obiettivo: dai token costruisci l'albero JSON. Impari: ricorsione, ownership.

- [x] Parser ricorsivo discendente (parse_value, parse_object, parse_array).
- [x] Errori chiari su input malformato (try/catch nel main → stderr + exit 1).

### [x] M4 — Query `.campo`

Obiettivo: applica un percorso al valore. Impari: navigazione strutture.

- [x] `.nome` estrae il campo da un oggetto.
- [x] `.a.b.c` percorsi annidati.

### [x] M5 — Stampa

Obiettivo: serializza di nuovo in JSON. Chiude il cerchio.

- [x] Stampa il valore risultante in JSON valido (serializer.hpp: serialize + escape_string + to_chars).

---

## Estensioni future (dopo M5)

- [x] Indici array `[0]` — split_path/query_index/query_path con PathStep = variant<string, size_t>.
- [x] Pretty-print con indentazione — serialize_pretty(value, depth), flag --pretty/-p.
- [x] Parsing argomenti CLI (CLI11) — path/file posizionali + -p, lettura da file o stdin, --help.
- [x] Test (doctest) — tests/test_minijson.cpp, CTest via `ctest --test-dir build`.
