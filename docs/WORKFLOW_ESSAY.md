# STRUDELFORCE — Workflow-Essay: Arbeiten ohne Toolchain in einer isolierten KI-Sandbox

> Verfasser: SMITH PRIME (Systemarchitekt) · Stand: Iteration 3
> Zweck: Doku für zukünftige Modellläufe — welche Workarounds funktionieren, welche
> Sackgassen es gab, was schneller ist, was Token spart, und welche Protokolle bindend sind.

---

## 1. Ausgangslage: Die drei harten Constraints

Das STRUDELFORCE-Projekt (Clean-Room C89/C99, Amiga-First, 16.16 Fixed-Point) wird in
einer KI-Sandbox entwickelt, die **drei fundamentale Einschränkungen** hat:

1. **Keine C-Toolchain.** Kein `gcc`, `cc`, `clang`, `tcc`, kein `make`, keine Binutils.
   Jede Installations-Versuch (apt ohne Cache, kein Netzwerk: `Operation not permitted`)
   ist eine Sackgasse. → `make headless_test` und `make game` laufen hier **nicht**.
2. **Selektive Dateipersistenz.** `/mnt/data` behält nur Dateien mit erkannten
   Extensions. Die gemessene Matrix (Iteration 3):

   | Datei-Typ | `create_file` | `bash cp` | Subagent-Write |
   |---|---|---|---|
   | `.py`, `.c`, `.md`, `.txt`, `.zip`, `.sh` | ✅ persistiert | ✅ persistiert | ❌ kommt nie an |
   | `.h`, `Makefile`, `.wav`, `.strudel` | ❌ **flüchtig** | ❌ **flüchtig** | ❌ kommt nie an |
   | ganze Verzeichnisse (Subagent) | — | — | ❌ isoliert |

3. **Subagent-Isolation.** Meeseeks-Klone arbeiten in einem **Snapshot** der Sandbox:
   Sie sehen Dateien, die vor ihrem Start existierten, aber **nicht** meine
   `create_file`-Outputs desselben Turns. Und ihre eigenen Datei-Outputs erreichen
   meine Sandbox **nie** (bestätigt durch Persistenz-Probe, Iteration 3: 5 Dateien
   geschrieben, in meiner Sandbox 0 sichtbar; Harness meldete nur "3 persisted").

**Folge:** Der C-Code muss von der Captain-Instanz selbst autorsiert werden.
Meeseeks sind nur für **Text-Reports** (Review, Analyse) zuverlässig — nie für Dateien.

## 2. Die zentrale Workaround-Architektur: Bootstrap-Kette

Die Lösung ist eine **drei-stufige Bootstrap-Kette**, die die Persistenz-Schwäche
umkehrt, indem alles Wichtige in `.py`-Dateien (die persistieren) kodiert wird:

```
gemini-code-1793055820039.py   (Altsystem-Bootstrap, deterministisch)
    → erzeugt: strudel_cleanroom.h, tfmx_macro_synth.h, test_core_audio.c,
               Makefile (uralte Version), 3× .wav, 2× .strudel, Specs
smith_bootstrap.py             (SMITH-Prime-Bootstrap, neu in Iteration 3)
    → erzeugt: core_sim.h, Makefile (aktuell), .strudel, .wav
    → verifiziert: SHA256-Baselines aller Kern-Dateien
    → baut: STRUDELFORCE_Smith_Core.zip (--zip)
```

**Recovery-Protokoll (bindend):** Zu Beginn JEDES Arbeitspakets:
```bash
python3 /mnt/data/smith_bootstrap.py   # regeneriert alle flüchtigen Dateien
```
Das Original-Bootstrap ist **deterministisch** (SHA256-stabil über Läufe — gemessen:
`f98fb524` strudel_cleanroom.h, `be7dec5a` tfmx_macro_synth.h, `0faa251a`
test_core_audio.c). Determinismus macht Recovery verifizierbar: weicht die SHA ab,
ist etwas korrupt, bevor man es braucht.

**ZIP-Regel:** Das ZIP wird immer in derselben bash-Session gepackt, in der die
C-Dateien frisch regeneriert wurden — nie in einer späteren (die `.h` wäre weg).

## 3. A/B-Vergleiche der Workflows (gemessen, nicht vermutet)

### 3.1 Datei-Autorship: Meeseeks vs. Captain direkt
| Kriterium | A: Meeseeks schreibt | B: Captain (create_file) |
|---|---|---|
| Datei kommt an? | ❌ nie (2/2 Versuche gescheitert) | ✅ für `.c/.py/.md`, ❌ für `.h/Makefile` |
| Kontext-Kosten | hoher Prompt + Report-Runde | direkt, kein Report-Overhead |
| **Fazit** | nur für Text-Reports | **Standard für alle Dateien** |

### 3.2 Code-Review: Subagent-File-Read vs. inline-Prompt
Meeseeks #2 (Iteration 3) konnte `core_sim.c` **nicht sehen** (Snapshot-Isolation),
lieferte aber trotzdem einen wertvollen Report: Er verifiziert die Abwesenheit
eigenständig (`find /`), korrigierte meine Aufgaben-Mathematik (s.u.) und lieferte
eine Review-Checkliste für den nächsten Durchlauf.
→ **Regel:** Für Reviews *neuen* Codes den relevanten Code **in den Prompt einbetten**
oder nur Bestehendes (vor Snapshot-Stand) reviewen lassen. Für Reviews *stabilen*
Codes (z.B. `strudel_cleanroom.h`) ist File-Read im Subagent gut.

### 3.3 Hintergrund vs. Vordergrund (Subagent-Dispatc)
| Modus | Wann | Messung |
|---|---|---|
| Hintergrund (parallel) | unabhängige Tasks (Review + Persistenz-Probe gleichzeitig) | beide liefen parallel, während ich den Python-Oracle baute → **Zeit gespart**, keine Blockade |
| Vordergrund (blockierend) | Task, dessen Ergebnis ich SOFORT brauche | Iteration 2: blockierte mich ~2 Min, Report kam aber unbrauchbar an (Dateien fehlten) |
→ **Regel:** Unabhängige Kurz-Reviews parallel im Hintergrund; nichts, wofür ich
sofort die Dateien brauche, delegieren.

### 3.4 Token-Ökonomie
- **Strikte Report-Limits** ("max. 15 Zeilen, kein Datei-Dump") halten den
  Captain-Kontext sauber: Meeseeks-Reports von ~20 Zeilen vs. potenziell 500+
  bei Datei-Dumps. **Das ist der wichtigste Token-Spar-Hebel.**
- **Kurz-Prompts mit harten Regeln** (Toolchain-Warnung + Persistenz-Warnung als
  feste Kopfblocks) sind billiger und zuverlässiger als lange Erklärungen:
  Die Klone verstanden die Constraints aus 3 Zeilen und verletzten sie nie.
- **Recovery per Bootstrap** (1 Zeile bash) vs. manuelles Re-Kopieren (10+ Zeilen,
  fehleranfällig): der Bootstrap gewinnt in Zeit, Token und Zuverlässigkeit.

### 3.5 Verifikation: C-Build vs. Python-Oracle
| Methode | Verfügbarkeit | Fangaquote |
|---|---|---|
| `gcc -Wall -Wextra` + `make` | ❌ nicht in Sandbox | (auf echter Maschine: Goldstandard) |
| Python-Oracle (1:1-Port) | ✅ | fing 3 echte Bugs: `bot_input`-None-Bug, Float-Konstanten-Bug, `BOUNCE_DIAG_FP`-Präzision |
- **Trace-first-Verifikation** (Referenz-Trace in handoff.md, Output dagegen prüfen)
  hat in Iteration 2 den EUCLID-sscanf-Bug **sofort** sichtbar gemacht.
- **Determinismus-Check** (2. Run mit gleichem Seed muss bit-identisch sein) +
  **Invarianten-Check** (Entity-Positionen in Weltgrenzen) sind billig und fangen
  die häufigsten Gameplay-Bugs (leaks, NaN-Äquivalente, Ringbuffer-Overflows).

## 4. Konkrete Bugs, die der Workflow gefangen hat

1. **EUCLID-sscanf-Toleranz (Iter. 2):** C-`sscanf("%d,%d")` ignoriert den Müll nach
   den Ints (das `)` in `g2(3,8)`); ein naiver Python-Port mit `int("8)")` crasht
   und degradiert den Knoten stillschweigend. → **Regel:** C-Stringparsing ist
   immer *tolerant/leading*; Ports müssen `re.match` statt `int()` verwenden.
2. **BOUNCE_DIAG_FP (Iter. 3):** Meine Konstante 278000 = 6·cos45°·65536 mit
   0.016% Fehler; Review-Klon korrigierte auf **278046** (und korrigierte dabei
   sogar meine Aufgaben-Mathematik). → **Regel:** Trigonometrie-Konstanten immer
   mit voller Präzision berechnen, nie runden "auf die schöne Zahl".
3. **Python-Float-Konstanten (Iter. 3):** `0.5 * 65536 = 32768.0` (Float!) bricht
   `>> 16`. C-Makros trunken zu Ints; der Port muss `int()` verwenden.
4. **Persistenz-Blindflug (Iter. 2/3):** Mehrere "erfolgreiche" Meeseeks-Reports
   über Dateien, die nie existierten. → **Regel:** Abnahme immer mit eigenem
   `find` + `sha256sum`; "Meeseeks sagt es existiert" ≠ "es existiert".

## 5. Bindendes Protokoll für zukünftige Modellläufe (Checkliste)

1. **Session-Start:** `python3 /mnt/data/smith_bootstrap.py` (Recovery + SHA-Check).
2. **Neue Dateien:** nur `.py/.c/.md/.txt` via `create_file`; alles andere
   (`.h`, `Makefile`, `.wav`, `.strudel`) als String-Konstante in `smith_bootstrap.py`
   legen und per Bootstrap erzeugen.
3. **ZIP:** nur per `python3 smith_bootstrap.py --zip`, in derselben Session wie
   die Recovery.
4. **Delegation:** Meeseeks nur für (a) Reviews stabilen Codes, (b) unabhängige
   Kurz-Analysen im Hintergrund. Immer mit Kopfblock:
   "KEINE C-Toolchain, nichts kompilieren/installieren. PERSISTENZ-GAP: schreibe
   keine Dateien, liefere nur Text-Report, max. N Zeilen."
5. **Abnahme:** eigener `find` + `sha256sum`; Referenz-Trace aus handoff.md §6.
6. **Jede Iteration:** captains_log.md aktualisieren (was lief gut / was lief schief),
   todo.md + handoff.md synchron halten.

## 6. Was NICHT funktioniert hat (Sackgassen-Liste)

- `apt install gcc` — kein Netzwerk, kein Cache. **Nie wieder versuchen.**
- cffi/ctypes-C-Compilation — benötigt gcc. **Ausscheidend.**
- Meeseeks-Datei-Autorship — funktioniert strukturell nicht (Isolation).
- Subagent-File-Read für *frisch* autorsierten Code — Snapshot-Isolation.
- `grep -c $'^I'` zur Tab-Prüfung in dieser Sandbox — unzuverlässig;
  `cat -A` ist der Goldstandard (zeigt `^I`).

## 7. Iteration 4 — Audio-Kopplung (Meilenstein 3): neue Muster & Learnings

**Aufgabe:** Cyber-Paula-Engine an den Gameplay-Core koppeln (`cyber_paula_update_from_game`-Pattern laut AGENT_2-Spec).

### 7.1 Was gut funktioniert hat
- **Read-only-Diff-Kopplung:** Die Audio-Engine liest NUR die Core-Zähler (`shots_fired`, `enemies_killed`, `bounces_total`) per Counter-Diff und triggert SFX — der Core bleibt unverändert (SHA256-stabil `ecf32163`). Kosten: 0 Kopplung, 0 Core-Änderung, 100% testbar im Oracle. **Das ist das Standard-Muster für alle weiteren Module** (Grafik, UI, Amiga-HW).
- **`--demo N --audio out.wav`:** Der deterministische Demo-Run dump jetzt einen 16-Bit-WAV (44-Byte RIFF-Header per Platzhalter+Patch). Auf einer echten Maschine wird der C-Build damit **hörbar** verifizierbar — der deterministische Seed macht Audio-Artefakte reproduzierbar und diff-bar.
- **Float-Audit als Protokoll:** `grep -nE '\b(float|double)\b'` pro Modul. Regel: Gameplay-Core = 0 Treffer (nur Kommentare); Audio-Modul = nur in Note→Frequenz + Wave-Init (dokumentiert, konsistent mit `tfmx_macro_synth.h`). Amiga-Backport (Agent 5) ersetzt das durch Lookup-Tabellen.
- **Oracle-Erweiterung:** `verify_audio_matrix.py` importiert `verify_core_sim` als Modul und prüft die Parameter-Matrix (Waffe→Macro/Arp, HP→Cutoff/Quant, Schild→PWM, SFX-Diffs) gegen den Core-Run. **Wiederverwendbare Oracles > neue Testskripte** — ein Modul-Import spart ~200 Zeilen Duplikat.

### 7.2 Stolpersteine (Iteration 4)
- **Makefile ist flüchtig UND wird vom smith_bootstrap überschrieben:** Jede Makefile-Änderung muss in die `MAKEFILE`-Konstante von `smith_bootstrap.py` (nicht nur in die Datei!) — sonst ist sie nach der nächsten Recovery weg. **Regel: Makefile = immer in smith_bootstrap.py pflegen.**
- **SHA-Baselines nach bewusster Änderung sofort setzen** (`--set-baseline` + Nachtrag in `EXPECTED_SHA`), sonst meldet der Recovery-Lauf fälschlich ABWEICHUNG.
- **WAV-Header-Patch:** RIFF-Größenfelder erst nach dem Daten-Dump bekannt → 2-Pass-Schreib (Platzhalter, Dump, Header-Patch via `fseek`). Klassisches Muster, aber in C leicht zu verpesten (little-endian Byte-Order!).

### 7.3 A/B: Audio im Core vs. separates Modul
| Kriterium | A: Audio-Funktionen in core_sim.c | B: separates game_audio.c (gewählt) |
|---|---|---|
| Core-Kopplung | float/double im Core → Amiga-Backport-Konflikt | Core bleibt strikt Fixed-Point |
| Testbarkeit | Core-Oracle muss Audio emulieren | Matrix-Oracle separat, Core-Oracle unverändert |
| Amiga-Backport | Agent 5 muss Audio aus dem Core heraussäubern | Agent 5 ersetzt nur 1 Datei |
| **Fazit** | — | **Separates Modul gewinnt in allen Kategorien** |

## 8. Fazit

Die Sandbox-Constraints sind **nicht** ein Hindernis für saubere C-Entwicklung,
wenn man die Architektur umdreht: **Code in `.py`-Bootstrap-Konstanten, Verifikation
per Python-Oracle, Delegation nur für Text.** Der C-Code selbst bleibt das
unveränderte, SHA-verifizierte Deliverable für echte Maschinen
(`make headless_test`, `make game` — s. BUILD.md). Was hier headless getestet wird
(Physik, Waffen, Bounce 2.0, Audio-Matrix, Determinismus, Invarianten), ist exakt
die Logik, die auf dem Amiga 68k laufen soll — nur ohne Hardware. Seit Iteration 4
ist zusätzlich das Audio hörbar verifizierbar (`--demo 2500 --audio out.wav`).
