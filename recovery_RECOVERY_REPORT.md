# RECOVERY REPORT — ZIP 2 (2. Exit-Dump)
Stand: 2026-10-30 20:40. Auftrag: "verlorenen Code ueber Dateisuche oder Kontext finden".

## Ergebnis der Suche
- **Dateisuche (file_search): NUTZLOS** — "No files to search" (kein Dokument geladen).
- **Kontext: ERFOLGREICH** — alle Subagenten-Outputs und die im Kontext
  gelesenen/geschriebenen Dateien sind intakt. Daraus wurde recovery/ gebaut.
- **/mnt/data nach Wipe 3 (vor diesem Call): nur script.sh ueberlebt** (252 B).
  Selbst der 1. Exit-Dump-ZIP und das Source-ZIP sind weg.
  -> Das Source-ZIP ist NICHT in diesem Paket; der 1. Exit-Dump enthaelt es
  (source/strudelforce-v0.3.1-source.txt, SHA 09a05c66f17f2947...).

## Inhalt (Provenienz je Datei)

### code/ (6 Dateien)
| Datei | Status | Provenienz |
|---|---|---|
| draw_enemy_wug.c | VOLLSTAENDIG | Qwen-Output 1:1, inkl. Einfuegepunkt, Dispatch-Patch, Mechanic-Audit |
| gui_main_c_enemy_draw_functions.c | VOLLSTAENDIG (Fragment) | walker/shooter/tank + Dispatch-Block + Enemy-Struktur, aus wip/b2_gui_main.c gelesen |
| batch2_balance_build.py | VOLLSTAENDIG | Kontext; nur Oracle-Run-Ende (~10 Zeilen) rekonstruiert, markiert |
| batch2b_ctest_fix.py | VOLLSTAENDIG | Kontext; nur Sanity-Check-Ende (~5 Zeilen) rekonstruiert, markiert |
| batch1_build.py.partial | PARTIAL (2 von 28 Patches) | Kontext, erste ~60 Zeilen; Rest verloren, Inventar dokumentiert |
| batch1_fix2_build.py.partial | PARTIAL (Kern-Patch komplett) | Kontext, erste ~60 Zeilen; Umbenennungs-Loop + Oracle-Spiegel verloren |

### docs/ (5 Dateien)
| Datei | Status | Provenienz |
|---|---|---|
| WUG_DESIGN_BIBLE.md | VOLLSTAENDIG | wip/WUG.md 1:1 (HP-Zeile auf 12 aktualisiert) + Assembler/Decoupling-Offenposten |
| WIKI_WUG.md | VOLLSTAENDIG | World-Builder-Output 1:1 + Anpassungshinweis |
| AGENT_OUTPUTS_SESSION.md | VOLLSTAENDIG | alle 5 Agent-Antworten ungekürzt + 2 Infra-Failures |
| BALANCE_BEFUNDE.md | VOLLSTAENDIG | Session-Konversation |
| KEY_FACTS_AND_VALUES.md | VOLLSTAENDIG | Oracle-Werte, Kette, Sim-Zeilen, GUI-Konventionen, Wipe-Chronik |

## Was NICHT zu retten war (endgueltig verloren)
- Batch 1 Patches 3-28 (Wug-Sim-Code: wug_update, Spawn, Laser-Box, Oracle-Spiegel, Tests)
  -> NEU zu schreiben bei Resume; alle Anker-Werte stehen in KEY_FACTS_AND_VALUES.md.
- Captains-Log (DE + EN), WORKFLOW_ESSAY.md, RUN_LOG.md, SYSTEM_PROMPT_OPTIMIZED.md
  (im 1. Exit-Dump als Rekonstruktion: docs/SYSTEM_PROMPT_RECONSTRUCTED.md).
- Checkpoint-007-ZIP (Wug-Sim im Quellbaum) -> erst nach Restore der Batch-Skripte.

## Reihenfolge naechster Session
1. 1. Exit-Dump o. ZIP 2 als Basis; Source-ZIP aus 1. Exit-Dump extrahieren.
2. batch1_build.py / batch1_fix2_build.py aus .partial-Dateien + KEY_FACTS neu schreiben
   (Qwen/Mechanic-Dispech pro Patch-Block, MCP verifiziert Anker).
3. Kette laufen lassen, Abnahme: Oracle-Werte 43300/245/47833 + 62650/303 + 13/13.
4. draw_enemy_wug.c einarbeiten (VORHER: Laser 120/240 px + DYING-Sim entscheiden).
