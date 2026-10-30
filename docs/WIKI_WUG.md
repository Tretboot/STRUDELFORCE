# Wug (Wiki-Artikel, World Builder, Exit-Session)
[Faktencheck Playtester Fast 6/6 gegen game_logic.c. MCP-Hinweis: letzter Trivia-Satz
technisch ungenau — rotes Gluehen = Palettenindex-Wechsel (Design-Bibel), nicht
"1-Pixel-Rotation". Vor Repo-Import diesen Satz anpassen.]

# Wug

Kleiner, runder Feind mit duenne Beinen, rundlichem Koerper, spitzem Schnabel und Schwanz-Zipfel.
Watschelt auf den Spieler zu, stoppt mit rot gluehenden Augen und feuert einen massiven
horizontalen Laser. Wird der Telegraph-Treffer gelandet, bricht der Schuss ab — der Wug
ist danach doppelt verwundbar.

## Verhalten

* **WADDLE:** 120-240 Ticks, Geschwindigkeit 0,5 Einheiten pro Tick
* **STOP:** 30 Ticks Wartezeit vor dem Angriff
* **CHARGE:** 60 Ticks Telegraph (Augen leuchten rot, Laser wird geladen)
* **FIRE:** 40 Ticks Laser-Abfeuern
* **COOLDOWN:** 50 Ticks Regeneration, empfaengt 2x Schaden
* **FLEE:** Flucht-Verhalten nach Erstverwundung
* **DYING:** Todes-Animation, platzt, nur Beine stehen noch
* Treffer waehrend CHARGE bricht den Laser sofort ab

## Werte

* **HP:** 12
* **Score:** 150
* **XP:** 20
* **Laser-Schaden:** 25 + Knockback
* **Max. gleichzeitig:** 2
* **Spawn-Bedingung:** Ab Welle 2

## Trivia

Der Wug wartet nicht. Der Wug laedt. Seine Existenz bestaetigt den Berko-Test von 1958 im
digitalen Raum: Ein Wort wird zu einem Wesen, ein Wesen zum Projektil. [Anpassen:]
Auf dem Amiga 500 wechselt der rote Glueheffekt den Palettenindex.
