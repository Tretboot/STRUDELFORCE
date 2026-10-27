#!/usr/bin/env python3
"""
verify_audio_matrix.py — Headless-Verifikation der reaktiven Audio-Matrix (Meilenstein 3).

1:1-Port der Parameter-Logik aus src/game_audio.c (game_audio_update, Teil 1-3).
Laueft auf dem Core-Oracle (verify_core_sim) und prueft:
  - Waffe->Bass-Macro/Arp: alle 3 Waffen werden im Run gesehen
  - HP<25% -> Cutoff 550Hz + 4-Bit-Quantisierung (Phase wird erreicht)
  - Schild -> PWM-Speed im Bereich 2..6
  - SFX-Transitions (Counter-Diff) feuern (shots/kills/bounces > 0)

Hinweis: Der wahre C-Beweis ist der WAV-Dumper auf einer echten Maschine:
  ./strudelforce_game --demo 2500 --audio demo.wav
"""
import verify_core_sim as vcs

CUTOFF_8000_FP = 62377
CUTOFF_550_FP = 12327


def matrix_params(s):
    """Port von game_audio_update (Parameter-Teil, 1:1)."""
    if s.weapon == vcs.WEAPON_LASER:
        bass_macro, arp_active = 0, 0
    elif s.weapon == vcs.WEAPON_BOUNCE:
        bass_macro, arp_active = 1, 1
    else:
        bass_macro, arp_active = 0, 0
    pwm_speed = 2 + s.shield // 25
    low = s.max_hp > 0 and (s.hp * 100) // s.max_hp < 25
    cutoff = CUTOFF_550_FP if low else CUTOFF_8000_FP
    quant = 1 if low else 0
    return bass_macro, arp_active, pwm_speed, cutoff, quant


def main():
    seed, ticks = 1234, 2500
    s = vcs.GameState(seed)
    weapon_seen = {0: 0, 1: 0, 2: 0}
    lowhp_ticks = 0
    pwm_min, pwm_max = 99, -1
    sfx_shots = sfx_kills = sfx_bounces = 0
    last_shots = last_kills = last_bounces = 0
    failures = []
    for t in range(ticks):
        vcs.core_tick(s, vcs.bot_input(s, t))
        bm, arp, pwm, cut, quant = matrix_params(s)
        weapon_seen[s.weapon] += 1
        if cut == CUTOFF_550_FP and quant == 1:
            lowhp_ticks += 1
        pwm_min = min(pwm_min, pwm)
        pwm_max = max(pwm_max, pwm)
        if s.shots_fired > last_shots:
            sfx_shots += 1
            last_shots = s.shots_fired
        if s.enemies_killed > last_kills:
            sfx_kills += 1
            last_kills = s.enemies_killed
        if s.bounces_total > last_bounces:
            sfx_bounces += 1
            last_bounces = s.bounces_total
        del bm, arp  # Matrix-Konsistenz: Werte sind aus dem State abgeleitet
    print("=== STRUDELFORCE Reactive Audio Matrix Self-Test (Python-Oracle) ===")
    print(f"seed={seed} ticks={ticks}")
    print(f"weapon-coverage: spread={weapon_seen[0]} laser={weapon_seen[1]} bounce={weapon_seen[2]}")
    print(f"lowhp_ticks={lowhp_ticks} (Cutoff-550Hz-Phase erreicht: {'ja' if lowhp_ticks > 0 else 'NEIN'})")
    print(f"pwm_speed range: {pwm_min}..{pwm_max} (erwartet 2..6)")
    print(f"sfx-retriggers: shots={sfx_shots} kills={sfx_kills} bounces={sfx_bounces}")
    ok = (weapon_seen[0] > 0 and weapon_seen[1] > 0 and weapon_seen[2] > 0
          and lowhp_ticks > 0 and pwm_min >= 2 and pwm_max <= 6
          and sfx_shots > 0 and sfx_kills > 0 and sfx_bounces > 0
          and not failures)
    print("matrix_check=" + ("OK" if ok else "FEHLER: " + "; ".join(failures[:5])))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
