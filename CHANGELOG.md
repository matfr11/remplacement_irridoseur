# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [PR-XX] — date — description

---

## [PR-02] — 2026-05-28 — GPIO handler complet + ISR vitesse robuste + mode dégradé A

### gpio_handler.c/h

**Calcul vitesse complet**
- `gpio_get_vitesse_m_h(float facteur_correction)` : implémenté — fenêtre glissante N impulsions
  - Formule : `vitesse_m_h = (N × dist_pulse_m × facteur_correction) / dt_s × 3600`
  - Retourne 0 si fenêtre < 2 pulses, si dist_pulse non initialisée, ou si timeout cycles atteint

**Nouveaux setters runtime**
- `gpio_handler_set_dist_pulse_m(float)` — mise à jour par state_machine à chaque changement d'étage
- `gpio_handler_set_params(int fenetre, int max_cycles_si)` — depuis NVS à l'init
- `gpio_handler_set_mode_degrade_a(bool)` — activation mode dégradé A
- `gpio_handler_set_vitesse_estimee(float)` — vitesse fournie par state_machine en mode A

**Mode dégradé A**
- Quand activé, `gpio_get_vitesse_m_h()` retourne la vitesse estimée depuis cycles poumon
- Alimentation par state_machine chaque tick : `dist_par_cycle_nvs × cycles_par_min × 60`

**Hooks test**
- `gpio_handler_test_injecter_pulse(int64_t)` et `gpio_handler_test_reset()` — `#ifdef CONFIG_IRRI_ENABLE_TESTS`

**Corrections**
- Lecture de `s_cycles_sans_impulsion` déplacée dans la section critique (évite race condition)
- Double `portENTER_CRITICAL` consolidé en un seul appel dans `gpio_get_vitesse_m_h`

### test/test_gpio.c — 9 tests unitaires

| # | Test | Ce qui est vérifié |
|---|---|---|
| 1 | `vitesse_nominale` | Calcul vitesse correct (5 pulses × 1m, dt=4s → 4500 m/h) |
| 2 | `facteur_correction` | Facteur 0.5 divise la vitesse par 2 |
| 3 | `vitesse_sans_dist_pulse` | Retourne 0 si dist_pulse non initialisée |
| 4 | `fenetre_insuffisante` | Retourne 0 si < 2 pulses en mémoire |
| 5 | `timeout_cycles` | Retourne 0 après max_cycles_si ticks sans pulse |
| 6 | `mode_degrade_a` | Retourne la vitesse estimée injectée |
| 7 | `mode_degrade_a_desactive` | Retour au calcul ISR après désactivation mode A |
| 8 | `compteur_impulsions` | Comptage brut + reset |
| 9 | `lire_entrees_no_crash` | Appel sans crash (valeurs matériel) |

**Build** : 0 erreur, 0 nouveau warning — `irrifrance-esp32.bin` ~214 Ko — **89% flash libre**

---

## [Rewrite v3] — 2026-05-28 — Repartition de zéro, SPECS_FINAL_v3

Réécriture complète depuis zéro selon `SPECS_FINAL_v3.md`.
L'ancienne architecture (4 PRs) est remplacée intégralement.

### Changements majeurs

**Matériel**
- Carte : ESP32 DevKit C + module relais → **ESP32 Quad MOS Switch Module** (4 MOSFET DC 5-60V, buck 12V intégré)
- Contacts : NO/NC configurable → **NC fixe fail-safe** (fil coupé = danger)

**Architecture**
- Ajout `gpio_config.h` — affectation GPIO centralisée (PIN_EV_CANON/POUMON provisoires GPIO 25/26)
- Ajout `main/machines/` — profils machine (machine_profile_t, double entrée spires/largeur)
- Ajout `main/abaques/` — abaques constructeur (canon_abaque_t, 13 entrées SR 150C)
- Ajout `securites.c/h` — watchdog SEC-1/SEC-2 priorité absolue, appelé en premier dans tick
- Ajout `regulation.c/h` — feedforward T_attente, correction Kp, moyenne dist_cycle
- Ajout `ota.c/h` — stub OTA (implémenté PR-08)

**Données réelles**
- `main/machines/st1bis_82_330.c` — Irrifrance ST1 Bis Ø82-330m (r_tambour=0.690m, spires=13.45)
- `main/abaques/sr150c.c` — Nelson SR 150C, 13 entrées, colonnes D40/D30/D25/D20/D15

**Machine d'états**
- 9 états : ajout `ETAT_OUVERTURE_CANON` (stabilisation pression 3s) et `ETAT_PAUSE_PRESSION` (top-level)
- Sous-états poumon : SOUS_REMPLISSAGE / SOUS_VIDANGE / SOUS_ATTENTE
- Impulsions capteur comptées pendant SOUS_REMPLISSAGE (bobine avance pendant remplissage)
- ARRET_FINAL : retour VEILLE automatique quand fin_de_course disparaît
- ARRET_URGENCE : raison sauvegardée en NVS, persiste après redémarrage

**Calculs hydraulique**
- Double interpolation : 2 lignes les plus proches (p_enrouleur + buse_mm) + dose
- Doses {40, 30, 25, 20, 15} mm — ordre décroissant — vitesse diminue quand dose augmente

**Build**
- `partitions.csv` : table OTA custom (app0 + app1 + spiffs, flash 4MB)
- `sdkconfig.defaults` : flash 4MB, partition custom, FreeRTOS 1kHz
- Build vérifié : `irrifrance-esp32.bin` 214 Ko — **89% flash libre**

---

## [PR-04] — 2026-05-27 — Calculs mécaniques (ancienne architecture — remplacé)

Remplacé par le rewrite v3.

---

## [PR-03] — 2026-05-27 — Table constructeur (ancienne architecture — remplacé)

Remplacé par le rewrite v3.

---

## [PR-02] — 2026-05-26 — GPIO handler (ancienne architecture — remplacé)

Remplacé par le rewrite v3.

---

## [PR-01] — 2026-05-25 — Structure projet (ancienne architecture — remplacé)

Remplacé par le rewrite v3.
