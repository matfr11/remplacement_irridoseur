# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [PR-XX] — date — description

---

## [PR-04] — 2026-05-28 — Calculs mécaniques : suite de tests complete

### test/test_calculs_mecanique.c — 16 tests (etait 6)

- `calcul_dist_pulse_m` : tests etage 1 (≈0.4593m) et etage 4 (≈0.6139m)
- `calcul_longueur_etage_m` : valeur etage 1 (≈61.78m) + cumul 4 etages (≈288.68m)
- `calcul_etage_courant` : 70m → etage 2, 330m → clamp etage 4
- `calcul_facteur_etalonnage` : couverture complete des 4 conditions de refus (C1 impulsions < 50, C2 facteur < 0.5, C3 ecart > 30%, C4 longueur theorique < 1m)

---

## [PR-03] — 2026-05-28 — Calculs hydrauliques — double interpolation abaque

### calculs_hydraulique.h

- `lookup_vitesse_cible()` : ajout paramètre `p_buse_out` (NULL si non souhaité) — interpolé avec les mêmes poids IDW que le débit

### calculs_hydraulique.c

**Correction UB pointeur struct**
- `interpoler_dose()` : remplacement de `const float *v = &e->D40; v[d]` (UB) par array local explicite `{e->D40, e->D30, e->D25, e->D20, e->D15}`

**Ranges de normalisation dynamiques**
- Suppression des constantes hardcodées `CANON_P_RANGE=4.6` / `CANON_BUSE_RANGE=8.1`
- `calc_ranges()` calcule min/max p et buse depuis les données de l'abaque à chaque appel
- Compatible avec tout abaque futur sans modification du code

**p_buse interpolé**
- Ajout du calcul IDW sur `p_buse` en parallèle du débit

### state_machine.c

- Call site `lookup_vitesse_cible(... NULL)` → `... NULL, NULL` (nouveau paramètre p_buse_out)

### test/test_calculs_hydraulique.c — 16 tests

| # | Test | Ce qui est vérifié |
|---|---|---|
| 1 | `exact_entry0_d25` | Exact match + débit renvoyé |
| 2 | `exact_entry6_d20` | Exact match entry intermédiaire |
| 3 | `exact_entry11_d40` | Exact match D40 |
| 4 | `interp_dose_22.5` | Interpolation linéaire entre D25 et D20 |
| 5 | `clamp_dose_haute` | dose>40 → D40 (vitesse la plus lente) |
| 6 | `clamp_dose_basse` | dose<15 → D15 (vitesse la plus rapide) |
| 7 | `nearest_neighbor` | p/buse légèrement décalés → convergence vers entry0 |
| 8 | `interp_deux_voisins` | Interpolation IDW entre entry2 et entry3 |
| 9 | `p_buse_out_entry0` | p_buse=3.5 sur exact match |
| 10 | `p_buse_out_entry6` | p_buse=4.0 sur exact match |
| 11 | `abaque_null` | NULL → retourne 0, pas de crash |
| 12 | `surface_100x60` | 100×60=6000 m² |
| 13 | `surface_zero` | longueur=0 → 0 |
| 14 | `dose_inst_25_15_60` | 25/(15×60)×1000=27.78mm |
| 15 | `dose_inst_vitesse_zero` | vitesse=0 → 0 (pas de div/0) |
| 16 | `dose_inst_largeur_zero` | largeur=0 → 0 (pas de div/0) |

**Build** : 0 erreur, 0 nouveau warning — 89% flash libre

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
