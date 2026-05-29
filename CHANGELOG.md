# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [PR-XX] — date — description

---

## [PR-09] — 2026-05-29 — Web UI mobile embarquée — 3 onglets

### main/webui/index.html (nouveau)
- HTML/CSS/JS inline en un seul fichier (~38 KB) — zéro dépendance externe
- Dark theme, responsive mobile 390px, boutons/textes min 48px
- **Onglet Accueil** : badge état coloré (9 états), tuiles longueur/vitesse/durée,
  heure d'arrivée estimée, états GPIO en temps réel, boutons DÉMARRER/ARRÊT/RESET,
  modal étalonnage (`{"cmd":"etalonner","longueur_m":x}`), alertes modes dégradés
- **Onglet Stats** : 13 métriques temps réel (surface, dose, débit, pression, étage,
  cycles, timings poumon, facteur correction) — lecture seule, mise à jour 500ms
- **Onglet Config** — 5 sections dépliables :
  - Programme actif : 5 programmes, dropdown + renommer, tous les champs,
    `{"cmd":"save_programme",...}`
  - Profil machine : dropdowns machine/abaque, t_vidange, Kp, cycles calibration,
    reset étalonnage, `{"cmd":"save_machine",...}`
  - Modes dégradés : checkboxes mode_deg_vitesse/poumon, t_rempl_fixe_s conditionnel
  - Mise à jour firmware : upload .bin `POST /ota/update`, barre de progression XHR
  - IRRITESTEUR : boutons EV_CANON/EV_POUMON ON/OFF + lecture entrées temps réel
    (visible uniquement à ETAT_VEILLE)

### main/webui.h
- Correction symboles asm : `_binary_index_html_start/end`
  (nom généré par ESP-IDF depuis le nom de fichier `index.html`)

### main/CMakeLists.txt
- Ajout `EMBED_FILES "webui/index.html"` dans `idf_component_register`

### main/webserver.c
- `root_handler()` : retourne le HTML embarqué via `webui_html_start/end`
  (remplace le placeholder texte de PR-08)
- Ajout `#include "webui.h"`

### Taille firmware
- 0xd7d70 bytes (~863 KB) — **55% flash libre** (+38 KB vs PR-08 pour l'UI)

---

## [PR-08] — 2026-05-29 — WiFi AP + WebSocket + OTA

### webserver.c — Implémentation complète
- WiFi AP : SSID "Irrifrance-ESP32", password "irrigation", IP 192.168.4.1, canal 6, WPA2
- Serveur HTTP/WebSocket : `httpd_start()`, handler WS `/ws`, HTTP GET `/` (placeholder PR-09)
- `webserver_broadcast_status()` : sérialisation `machine_status_t` → JSON 35 champs (snprintf),
  broadcast via `httpd_queue_work` + `httpd_ws_send_frame_async` (safe depuis tâche externe)
- Parsing commandes WebSocket sans bibliothèque JSON :
  `start`, `stop`, `reset`, `set_time`, `ev_canon`, `ev_poumon`,
  `select_programme`, `save_programme`, `save_machine`, `etalonner`

### ota.c / ota.h — Implémentation complète
- `ota_register_handler(httpd_handle_t)` : enregistre `POST /ota/update` sur serveur existant
- Bloque si cycle en cours (retourne 409)
- `esp_ota_begin/write/end/set_boot_partition` + reboot après 3s
- Rollback implicite (partition active inchangée en cas d'erreur)

### sdkconfig.defaults
- Ajout `CONFIG_HTTPD_WS_SUPPORT=y` (requis pour l'API WebSocket d'esp_http_server)

### Taille firmware
- 0xce9e0 bytes — 57% flash libre (WiFi + WebSocket stack : +~280KB vs PR-07)

---

## [PR-07] — 2026-05-28 — NVS config — 5 programmes — tests

### config_nvs.c
- Fix bug namespace : `prog_actif` lu/écrit dans `irri_state` (était `irri_machine` par erreur,
  SPECS_FINAL_v3 §11 place cette clé dans `irri_state`)

### test/test_config_nvs.c — 10 tests unitaires (nouveau fichier)
- T01-T03 : `config_programme_est_valide()` — vide/complet/buse_zero
- T04 : roundtrip config_machine_t (facteur_correction, kp_regulation, fenetre_vitesse, mode_deg)
- T05-T06 : roundtrip programme idx=0 et idx=4 (nom, dose, largeur, buse, tempo)
- T07-T08 : indices invalides (-1, 5) → ESP_ERR_INVALID_ARG
- T09 : roundtrip prog_actif
- T10 : roundtrip raison urgence

### main.c
- Déplacement de `config_nvs_init()` avant le bloc `CONFIG_IRRI_ENABLE_TESTS`
  (les tests NVS nécessitent que NVS flash soit initialisé)
- Ajout `extern void test_config_nvs_run(void)` et appel dans le bloc tests

### CMakeLists.txt
- Ajout `"test/test_config_nvs.c"` dans le bloc conditionnel `IRRI_ENABLE_TESTS`

---

## [PR-06] — 2026-05-28 — Régulation feedforward — modes dégradés — tests

### state_machine.c
- Ajout mise à jour `s_cfg_machine.dist_cycle_nvs` en mémoire à chaque cycle
  (conserve la dernière distance/cycle mesurée pour le mode dégradé A)
- Ajout estimation vitesse mode dégradé A dans `tick_state_machine()` :
  calcul depuis `dist_cycle_nvs` et durée cycle, appel `gpio_handler_set_vitesse_estimee()`
- Ajout `config_nvs_sauver_machine()` en fin de session (`ETAT_ARRET_FINAL` et `cmd_reset`)

### main.c
- Câblage de tous les tests sous `CONFIG_IRRI_ENABLE_TESTS` :
  `test_calculs_hydraulique_run`, `test_calculs_mecanique_run`, `test_gpio_run`,
  `test_regulation_run`, `test_state_machine_run`

### CMakeLists.txt
- Ajout `test/test_regulation.c` dans le bloc conditionnel `IRRI_ENABLE_TESTS`

### test/test_regulation.c — 10 tests unitaires (nouveau fichier)
- T_attente nominal, T_att < 0 (alerte=true), T_att > 300s, inputs nuls
- correction_vitesse : positive, clamp à 0, négative, v_cible=0
- regulation_update_dist_par_cycle : moyenne glissante sur 5 valeurs
- regulation_get_nb_cycles + regulation_reset_calibration

---

## [PR-05] — 2026-05-28 — Machine d'états : stubs test + 11 tests simulation

### state_machine.c

- Variables de simulation statiques (s_sim_active/pression/fin_course/secu_spires) sous CONFIG_IRRI_ENABLE_TESTS
- `state_machine_test_injecter_etat()` : active la simulation avec valeurs sures par defaut
- `state_machine_test_set_pression/fin_course/secu_spires()` : implementes (etaient vides)
- Override des entrees GPIO dans tick_state_machine quand sim active

### test/test_state_machine.c — 11 tests (etait 3)

- Injection directe de chaque etat principal (EN_COURS, OUVERTURE_CANON, TEMPO_ARRIVEE, PAUSE_PRESSION)
- Urgence depuis EN_COURS et PAUSE_PRESSION → ARRET_URGENCE → reset → VEILLE
- cmd_stop depuis EN_COURS → ARRET_FINAL
- cmd_stop noop depuis ARRET_URGENCE (reste en urgence)

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
