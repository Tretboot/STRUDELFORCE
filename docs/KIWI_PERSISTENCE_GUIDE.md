# Kiwi Connect – Datenverlust vermeiden (Learnings STRUDELFORCE v3)

## Beobachtete Probleme (reproduziert)
1. **Dateityp-Filter:** Zwischen zwei Tool-Aufrufen bleiben nur `.c .py .md .txt .json .zip .png` erhalten.
   Verloren gehen: `.h`, `Makefile` (ohne Endung), `.strudel`, `.mod`, `.tfx`, `.sam`, `.wav`, `.raw`, `.adf`.
2. **Ganze Ordner verschwunden**, nachdem in einem Aufruf sehr viele Dateien entstanden waren (18-MB-ZIP komplett entpackt → "No space left on device").
3. **Race Condition bei parallelen Aufrufen:** `create_file` und ein davon abhängiger `bash`-Aufruf im selben Parallel-Block → bash lief, bevor die Datei existierte.
4. **Fehlgeschlagene Aufrufe speichern nichts.** Alles, was darin geschrieben wurde, ist weg.
5. **Ausführung zeitweise komplett nicht verfügbar** ("temporarily unavailable"), auch über mehrere Aufrufe hinweg.
6. **Auslieferungs-Limit:** Bei vielen Dateien werden einzelne als "omitted from delivery" übersprungen.

## Regeln
- **Nur die ZIP zählt.** Erzeugen, testen, packen und prüfen in **einem** Aufruf (`tools/build_handoff.py`).
- Nicht erhaltene Dateitypen (`.h`, Makefile, `.strudel`, `.vscode`) als Text in einem `.py`-Generator (`tools/gen_sources.py`) ablegen und erst direkt vor dem Packen erzeugen.
- Jede ZIP enthält `MANIFEST.json` mit sha256-Prüfsummen. Der Empfänger prüft mit `python3 tools/kiwi_handoff.py verify <zip>`.
- Große Asset-Bibliotheken nie komplett entpacken, sondern nur benötigte Dateien gezielt mit `zipfile.read` lesen.
- Abhängige Tool-Aufrufe **nacheinander** ausführen, nie im selben Parallel-Block.
- Zu Beginn jeder Session `kiwi_handoff.py probe` ausführen, im nächsten Aufruf `probe-check`, und das Ergebnis im Captains Log festhalten.
- **Zwischenstände früh als ZIP sichern**, nicht erst am Ende, da die Ausführung jederzeit ausfallen kann.

## Nächste Schritte lokal (VS Code)
1. `python3 tools/kiwi_handoff.py verify STRUDELFORCE_v3.zip`
2. `make verify` (Python-Referenzplayer)
3. `make c89_check` und `make amiga_lint`
4. `make test_audio_mix && make c_vs_oracle` (Ziel: 0 Abweichungen zwischen C und Python)
5. Hörtest: `./test_audio_mix --tfmx "<datei>.tfx" --mod "<datei>.mod" --wav mix.wav`
6. `python3 tools/strudel_bridge.py sgc <SGC-Command-Checkout>` → sfsong-JSON für den nächsten Kiwi-Handoff
7. Compiler- und Emulator-Befunde in den Captains Log schreiben und für den nächsten Kiwi-Handoff mitgeben
