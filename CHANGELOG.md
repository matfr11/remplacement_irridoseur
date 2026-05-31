# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [Keep a Changelog](https://keepachangelog.com/fr/1.0.0/)

---

## [PR-14] — 2026-05-31 — Robustesse — TWDT + détection coupure + heartbeat RC

### Added
- `CONFIG_ESP_TASK_WDT_*` dans `sdkconfig.defaults` : reboot automatique si `state_machine_task` bloquée > 3s
- `esp_task_wdt_add()` + `esp_task_wdt_reset()` dans `state_machine_task` (`main.c`)
- Clé NVS `session_act` (uint8) dans `irri_state` : distingue coupure de courant d'un arrêt propre
- `config_nvs_sauver_session_active()` / `config_nvs_lire_session_active()` dans `config_nvs.c`
- `s_coupure_detectee` : flag firmware → alerte UI si session interrompue sans urgence au reboot
- Alerte UI coupure (fond gris, longueur sauvée affichée) + boutons Reprendre/Reset dans ETAT_VEILLE
- Heartbeat GPIO 2 (toggle 1Hz) pour circuit RC fail-safe — conditionnel, défaut OFF
- `heartbeat_rc_on` dans `config_machine_t` (NVS `irri_machine`) + toggle UI Config → Machine
- `PIN_HEARTBEAT = 2` dans `gpio_config.h` (LED bleue ESP32)
- `coupure_detectee` et `cfg_heartbeat_rc_on` dans `machine_status_t` et JSON WebSocket

### Changed
- `state_machine_init()` : lit `session_active`, détecte coupure, bloque `s_demarrage_autorise` si coupure
- `state_machine_cmd_reset()` / ETAT_ARRET_FINAL : efface le flag `session_active` à l'arrêt propre
- `state_machine_cmd_resume()` : efface `s_coupure_detectee`
- Entrée ETAT_OUVERTURE_CANON depuis VEILLE : pose `session_active = true`

**Taille firmware** : 0xe16a0 bytes (~925 KB) — 53% flash libre

---

## [PR-13] — 2026-05-30 — Reprendre après sécurité débordement + 3-tap reprendre

### Added
- Alerte orange "Debordement bobine actif" dans l'UI (visible si urgence debordement ET `secu_spires` encore actif)
- Alerte verte "Debordement resolu" dans l'UI (visible si urgence debordement ET capteur libéré)

### Changed
- **3-tap poumon_plein en ARRET_URGENCE** : longueurs préservées désormais (comportement "reprendre" au lieu de "reset complet")
- Bouton REPRENDRE SESSION masqué tant que débordement bobine est encore actif
- `state_machine_cmd_resume()` : refusée si raison="Debordement bobine" ET `secu_spires` actif (garde firmware)
- 3-tap poumon_plein bloqué si débordement encore actif (SEC-2 re-déclencherait immédiatement)

**Taille firmware** : 0xe0fe0 bytes (~900 KB) — 53% flash libre

---

## [PR-12] — 2026-05-29 — Mesure tension batterie

### Added
- `batterie.c/h` : mesure ADC1 GPIO 36, diviseur R1=100kΩ/R2=27kΩ, moyenne 16 samples
- Seuils configurables NVS (`batt_warn_v`, `batt_crit_v`) via Config → Machine
- Barre de niveau batterie dans l'UI Accueil (tension V, état texte, pourcentage indicatif)
- 5 états batterie : CHARGE (>13.5V), PLEINE, CORRECTE, FAIBLE, CRITIQUE
- Curseur tension batterie dans le simulateur web (`batterie_sim_set_voltage()`)
- `config_nvs_lire_batt_seuils()` / `config_nvs_sauver_batt_seuils()`

### Changed
- `machine_status_t` : ajout `batterie_v`, `batterie_pct`, `batterie_etat`, `cfg_batt_warn_v`, `cfg_batt_crit_v`
- `status_to_json()` : ajout 5 champs batterie
- `state_machine_init()` : appel `batterie_init()`, chargement seuils NVS, `batterie_set_seuils()`
- Tick state_machine : lecture batterie toutes les 300 itérations (30s)

---

## [PR-11] — 2026-05-29 — Tests unitaires PC + simulateur web

### Added
- `test/host/` : infrastructure CMake pour tests unitaires PC (Unity framework, sans matériel)
- Stubs ESP-IDF : GPIO, NVS, FreeRTOS, log, timer haute résolution
- Helpers : config NVS valide/invalide, injection GPIO
- Scénarios d'intégration : cycle complet, urgences SEC-1/SEC-2, modes dégradés
- 47 tests unitaires (tous verts en CI GitHub Actions)
- `simulator/simulator.c` : simulation GPIO via macro `READ_GPIO` (`sim_gpio_get_level`)
- `simulator/simulator_ws.c` : routes `/test` et `/ws_test`, parsing JSON injection capteurs
- Génération automatique de pulses vitesse (`sim_set_vitesse_auto`)
- `CONFIG_IRRI_TEST_MODE` : sdkconfig + menuconfig

### Changed
- `gpio_handler.c` : macro `READ_GPIO(pin)` (remplace `gpio_get_level(pin)` directement)
- `state_machine.c` : stubs de simulation séparés du code de production

---

## [PR-10] — 2026-05-29 — Intégration complète + stats campagne + ETAT_DEROULE

### Added
- `ETAT_DEROULE` (état 9) : mesure longueur déployée via pastilles pendant traction tracteur
- Estimation heure d'arrivée (`heure_arrivee_unix`, `heure_arrivee_relative_s`)
- Preview vitesse live dans l'UI Config (`GET /api/vitesse?p=X&b=Y&d=Z`)
- Stats campagne cumulatives NVS (`irri_stats`, blob `config_stats_t`)
- Mode dégradé bypass spires (`mode_deg_spires`, `securites_set_bypass_spires()`)
- Calcul `dist_par_cycle_m` depuis géométrie (si `cycles_par_tour > 0`)
- `cmd_resume` : reprise session depuis ARRET_URGENCE/ARRET_FINAL, longueurs préservées
- `state_machine_cmd_reset_campagne()` + commande WebSocket `reset_campagne`
- `state_machine_recharger_config()` : relit NVS sans redémarrage (VEILLE uniquement)
- `state_machine_get_session_summary()` : expose bilan session pour télémétrie
- Bilan session JSON `{"type":"bilan",...}` diffusé à entrée ARRET_FINAL
- `webserver_broadcast_raw(const char *json)`
- 8 champs `cfg_*` dans `machine_status_t` (pour initialiser l'UI Config depuis statut)
- `state_machine_set_longueur()` : correction manuelle longueur déployée

### Changed
- `machine_status_t` : ajout nombreux champs (stats session, campagne, estimation arrivée, cfg_*)
- `JSON_BUF_SIZE` porté à 3072
- `config_nvs.h` : ajout `config_stats_t`, `config_nvs_lire/sauver_stats()`, `config_nvs_reset_stats()`
- Vitesse m/h corrigée (×3600 manquant)
- `loadConfigFromStatus()` dans l'UI complétée (peuple tous les champs Config depuis JSON)

### Fixed
- Vitesse affichée 0 en cours de session (facteur 3600 absent dans calcul)

---

## [PR-09] — 2026-05-29 — Web UI mobile embarquée — 3 onglets

### Added
- `main/webui/index.html` : UI HTML/CSS/JS inline (~38 KB), zéro dépendance externe
- Dark theme, responsive mobile 390px, boutons min 48px tactile
- Onglet **Accueil** : badge état coloré, tuiles longueur/vitesse/durée, GPIO temps réel, boutons opérateur, alertes
- Onglet **Stats** : 13 métriques session temps réel
- Onglet **Config** : 5 sections dépliables (Programme, Machine, Modes dégradés, OTA, IRRITESTEUR)
- `EMBED_FILES "webui/index.html"` dans CMakeLists.txt

### Changed
- `root_handler()` dans webserver.c retourne le HTML embarqué (était placeholder texte)

**Taille firmware** : 0xd7d70 bytes (~863 KB) — 55% flash libre

---

## [PR-08] — 2026-05-29 — WiFi AP + WebSocket + OTA

### Added
- WiFi AP : SSID "IRRIDOSEUR-XXXX" (4 derniers octets MAC), IP fixe 192.168.4.1
- Serveur HTTP + WebSocket (`/ws`) via `esp_http_server`
- `webserver_broadcast_status()` : sérialise `machine_status_t` → JSON, broadcast tous clients
- Parsing commandes WebSocket sans cJSON : `start`, `stop`, `reset`, `set_time`, `ev_canon`, `ev_poumon`, `select_programme`, `save_programme`, `save_machine`, `etalonner`
- `ota.c` : `POST /ota/update`, partition swap, reboot après 3s, rollback implicite
- `telemetry_task` : tâche FreeRTOS prio 5, broadcast 500ms
- `sdkconfig.defaults` : `CONFIG_HTTPD_WS_SUPPORT=y`
- Endpoint `GET /api/vitesse` (preview vitesse sans session)

**Taille firmware** : 0xce9e0 bytes — 57% flash libre

---

## [PR-07] — 2026-05-28 — Config NVS — 5 programmes — tests

### Added
- `config_nvs.c/h` : 4 namespaces NVS (`irri_machine`, `irri_prog0..4`, `irri_state`)
- `config_machine_t` : 13 champs (profil, régulation, modes dégradés, cycles_par_tour)
- `config_programme_t` : 9 champs (nom, dose, largeur, buse, pression, temporisations)
- `config_programme_est_valide()` : validation programme (dose/largeur/buse/pression > 0)
- `config_nvs_sauver/lire_longueur()`, `config_nvs_sauver/lire_urgence()`
- 10 tests unitaires `test_config_nvs.c`

### Fixed
- Namespace `prog_actif` : corrigé dans `irri_state` (était `irri_machine` par erreur)

---

## [PR-06] — 2026-05-28 — Régulation feedforward + modes dégradés

### Added
- `regulation.c/h` : `calcul_t_attente_s()`, `correction_vitesse()`, `regulation_update_dist_par_cycle()`
- Buffer circulaire CALIB_WINDOW=5 pour moyenne dist/cycle
- `calcul_cycles_par_min()`, `regulation_get_nb_cycles()`, `regulation_reset_calibration()`
- Mode dégradé B : remplissage temporisé `t_rempl_fixe_s` si contact poumon HS
- Mode dégradé A : vitesse estimée depuis cycles poumon (`gpio_handler_set_vitesse_estimee()`)
- 10 tests unitaires `test_regulation.c`

### Changed
- `tick_state_machine()` : estimation vitesse mode dégradé A calculée chaque tick
- `config_nvs_sauver_machine()` appelé en fin de session (sauvegarde `dist_cycle_nvs`)

---

## [PR-05] — 2026-05-28 — Machine d'états complète + sécurités + tests simulation

### Added
- `securites.c/h` : `securites_watchdog()` — SEC-1 (fin_course) et SEC-2 (secu_spires)
- SEC-2 priorité absolue, tout état — `gpio_all_ev_off()` avant urgence
- SEC-1 filtrée par `sec1_applicable` (exclut TEMPO_ARRIVEE, ARRET_*)
- `state_machine_declencher_urgence(const char *raison)` : NVS + transition ARRET_URGENCE
- ETAT_PAUSE_PRESSION : pause/reprise auto sur pressostat
- ETAT_TEMPO_ARRIVEE : arrosage final après fin de course
- Réarmement physique 3-tap poumon_plein → VEILLE depuis ARRET_URGENCE
- Stubs de simulation sous `CONFIG_IRRI_ENABLE_TESTS`
- 11 tests `test_state_machine.c`

---

## [PR-04] — 2026-05-28 — Calculs mécaniques — profils machine — étalonnage

### Added
- `calculs_mecanique.c/h` : `calcul_rayon_etage()`, `calcul_dist_pulse_m()`, `calcul_dist_cycle_m()`
- `calcul_etage_courant()` : accumulation longueurs étages jusqu'à position courante
- `calcul_facteur_etalonnage()` : 4 conditions de validation (nb_impulsions, plage, delta)
- `machines/machines.h` : `machine_profile_t`, `MACHINES_LISTE[]`, `machine_resoudre_double_entree()`
- `machines/st1bis_82_330.c` : profil Irrifrance Structure 1 bis Ø82mm — 330m
- 16 tests `test_calculs_mecanique.c`

---

## [PR-03] — 2026-05-28 — Calculs hydrauliques — double interpolation abaque

### Added
- `calculs_hydraulique.c/h` : `lookup_vitesse_cible()` — double interpolation IDW
- Interpolation linéaire entre colonnes D40/D30/D25/D20/D15 (dose)
- Paramètre `p_buse_out` : pression effective buse interpolée
- `calcul_surface_m2()`, `calcul_dose_inst_mm()`
- `abaques/abaques.h` : `canon_abaque_t`, `canon_entry_t`, `ABAQUES_LISTE[]`
- `abaques/sr150c.c` : abaque Nelson SR 150C (13 entrées)
- 16 tests `test_calculs_hydraulique.c`

### Fixed
- `interpoler_dose()` : UB pointeur struct corrigé (array local explicite)
- Ranges de normalisation calculés dynamiquement depuis l'abaque (n'étaient pas hardcodés)

---

## [PR-02] — 2026-05-28 — GPIO handler complet + ISR vitesse robuste

### Added
- `gpio_handler.c/h` : ISR vitesse anti-rebond 50ms, fenêtre glissante N impulsions
- `gpio_get_vitesse_m_h(float facteur_correction)` : calcul complet avec timeout cycles
- `gpio_handler_lire_entrees()` : snapshot atomique toutes les entrées NC
- `gpio_ev_canon_set()`, `gpio_ev_poumon_set()`, `gpio_all_ev_off()` (fail-safe)
- `gpio_handler_set_dist_pulse_m()`, `gpio_handler_set_params()`, `gpio_handler_set_mode_degrade_a()`
- `gpio_handler_tick_cycle()` : incrémente compteur cycles sans impulsion
- Hooks test : `gpio_handler_test_injecter_pulse()`, `gpio_handler_test_reset()`
- 9 tests `test_gpio.c`

### Fixed
- Race condition `s_cycles_sans_impulsion` : lecture déplacée dans section critique
- Double `portENTER_CRITICAL` consolidé

---

## [PR-01] — 2026-05-28 — Structure projet + squelettes

### Added
- Structure CMakeLists.txt : partitions OTA, EMBED_FILES, bloc `IRRI_ENABLE_TESTS`
- `gpio_config.h` : tous les `#define PIN_*`, `NB_PASTILLES=10`, `DEBOUNCE_VITESSE_MS=50`
- `main.c` : `app_main()`, séquence init, `state_machine_task` prio 10
- Squelettes de tous les modules (`.h` avec API complète, `.c` vides)
- `sdkconfig.defaults` : target esp32, 4MB flash, partitions OTA
- `sdkconfig.defaults` : `CONFIG_FREERTOS_UNICORE=n` (dual core actif)
