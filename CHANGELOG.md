# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [PR-XX] — date — description

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
