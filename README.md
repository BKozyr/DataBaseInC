# SQLite Clone in C

Prosta relacyjna baza danych napisana całkowicie od zera w języku C. Projekt jest implementacją silnika bazy danych inspirowanego architekturą SQLite. Pozwala na uruchomienie interaktywnego środowiska (REPL), wprowadzanie i odczytywanie wierszy oraz trwały zapis danych na dysku z wykorzystaniem struktury B-Drzewa (B-Tree).

## 🚀 Główne funkcjonalności

<img width="899" height="989" alt="image" src="https://github.com/user-attachments/assets/fd636ede-520c-45eb-a4e9-42ac0798fb28" />


* **REPL (Read-Execute-Print-Loop):** Interaktywny interfejs wiersza poleceń do komunikacji z bazą.
* **Kompilator i Wirtualna Maszyna:** Przetwarzanie wejścia tekstowego na instrukcje rozumiane przez silnik.
* **Struktura B-Drzewa (B-Tree):** Wiersze są przechowywane i sortowane w węzłach drzewa (Leaf Nodes i Internal Nodes) z wykorzystaniem algorytmu wyszukiwania binarnego (Binary Search).
* **Zarządzanie pamięcią (Pager):** Dzielenie pliku bazy na 4-kilobajtowe strony (Pages) i ładowanie ich do pamięci RAM (Cache) tylko wtedy, gdy są potrzebne.
* **Trwałość danych (Persistence):** Stan bazy jest trwale zapisywany na dysku po zamknięciu programu.
* **Testy E2E (Pytest):** Zestaw zautomatyzowanych testów weryfikujących poprawność operacji bazodanowych i limity wielkości.

## 🧠 Architektura

Baza danych składa się z kilku warstw (od frontend'u do backend'u):

1.  **Input Buffer & Tokenizer:** Pobiera wejście od użytkownika i dzieli je na tokeny.
2.  **Parser (Statement Preparation):** Rozpoznaje zapytania SQL (`insert`, `select`) oraz metakomendy (`.exit`, `.btree`). Weryfikuje poprawność składniową.
3.  **Virtual Machine (Statement Execution):** Decyduje, jakie operacje wykonać na strukturze danych.
4.  **B-Tree:** Główna struktura danych odpowiadająca za szybkie wstawianie (z podziałem węzłów) i wyszukiwanie rekordów.
5.  **Pager:** Bezpośrednio komunikuje się z systemem operacyjnym (I/O). Odczytuje i zapisuje strony do pliku `.db`.

## 🛠️ Obsługiwany schemat tabeli

Obecnie baza jest na sztywno zakodowana do obsługi jednej tabeli z następującym schematem:

| Kolumna | Typ | Maksymalny rozmiar |
| :--- | :--- | :--- |
| `id` | `uint32_t` | 4 bajty |
| `username` | `char[33]` | 32 bajty + znak `\0` |
| `email` | `char[256]` | 255 bajtów + znak `\0` |

## 💻 Jak uruchomić?

### Kompilacja silnika
Aby skompilować kod źródłowy, upewnij się, że masz zainstalowany kompilator `gcc` i uruchom poniższą komendę:

```bash
gcc main.c -o main
