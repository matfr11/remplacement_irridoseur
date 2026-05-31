---
type: claude-context
updated: 2026-05-31
---

# Architecture — Irrifrance ESP32

## Schéma ASCII architecture complète

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        ESP32 Quad MOS Switch Module                         │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         app_main()                                   │   │
│  │  config_nvs_init → batterie_init → gpio_handler_init                │   │
│  │  → state_machine_init → webserver_init → ota_init → telemetry_init  │   │
│  │  → xTaskCreate(state_machine_task, prio=10)                         │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────┐     ┌──────────────────────────────────────┐   │
│  │  state_machine_task     │     │  telemetry_task                      │   │
│  │  Priorité 10, 8KB stack │     │  Priorité 5, 8KB stack               │   │
│  │  Tick 100ms             │     │  Tick 500ms                          │   │
│  │                         │     │                                      │   │
│  │  securites_watchdog()   │     │  webserver_broadcast_status()        │   │
│  │  gpio_handler_lire_*()  │     │  → state_machine_get_status()        │   │
│  │  [3-tap check]          │     │  → status_to_json()                  │   │
│  │  tick_state_machine()   │     │  → httpd_ws_send_frame() × clients   │   │
│  └─────────┬───────────────┘     └──────────────────┬───────────────────┘   │
│            │ mutex récursif                          │                       │
│            ▼                                        │                       │
│  ┌─────────────────────────┐                        │                       │
│  │  state_machine.c        │◄───────────────────────┘                       │
│  │  s_etat, s_status       │                                                │
│  │  s_cfg_machine/prog     │◄── webserver.c (WebSocket cmds)               │
│  │  s_longueur_enroulee    │                                                │
│  └──────┬──────────────────┘                                                │
│         │ appelle                                                            │
│   ┌─────┼──────────────────────────────┐                                    │
│   ▼     ▼                              ▼                                    │
│ securites.c  regulation.c     calculs_hydraulique.c                         │
│ SEC-1/SEC-2  feedforward+Kp   lookup_vitesse_cible()                        │
│              dist_par_cycle   interpolation abaque                          │
│                               ▼                                             │
│                        calculs_mecanique.c                                  │
│                        rayon_etage, etage_courant                           │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  gpio_handler.c                                                      │   │
│  │  ISR vitesse (PIN 34) — fenêtre glissante — gpio_get_vitesse_m_h()  │   │
│  │  gpio_ev_canon_set() / gpio_ev_poumon_set() / gpio_all_ev_off()     │   │
│  │  gpio_handler_lire_entrees() — snapshot atomique                    │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  config_nvs.c  — 4 namespaces NVS flash                             │   │
│  │  batterie.c    — ADC1 GPIO 36, moyenne 16 samples                   │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  webserver.c — WiFi AP + httpd + WebSocket                          │   │
│  │  index.html  — UI embarquée 3 onglets (EMBED_FILES)                 │   │
│  │  ota.c       — POST /ota/update                                     │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Tâches FreeRTOS

| Nom tâche | Priorité | Stack | Période | Rôle |
|---|---|---|---|---|
| `state_machine` | 10 | 8192 | 100 ms | Tick machine d'états + sécurités |
| `telemetry` | 5 | 8192 | 500 ms | Broadcast JSON WebSocket |
| `tiT` / `tiR` | (WiFi/LwIP) | — | async | Stack réseau ESP-IDF |
| `httpd` | (httpd) | — | async | Serveur HTTP + WebSocket |

Note : le watchdog sécurités (`securites_watchdog()`) n'est pas une tâche séparée — il est appelé
EN PREMIER dans `tick_state_machine()`, avant la lecture des entrées et le traitement d'état.

## Fichiers et rôles

| Fichier | Rôle |
|---|---|
| `main/main.c` | Point d'entrée, init séquencé, création tâche state_machine |
| `main/state_machine.c` | Machine d'états 10 états, régulation, bilan session, API commandes |
| `main/state_machine.h` | Types `etat_machine_t`, `machine_status_t`, `session_summary_t`, API |
| `main/securites.c` | Watchdog SEC-1/SEC-2, bypass spires |
| `main/securites.h` | `securites_watchdog()`, `securites_set_bypass_spires()` |
| `main/gpio_handler.c` | ISR vitesse, fenêtre glissante, snapshot entrées, sorties EV |
| `main/gpio_handler.h` | `entrees_t`, API gpio |
| `main/gpio_config.h` | Tous les `#define PIN_*`, `NB_PASTILLES`, `DEBOUNCE_VITESSE_MS` |
| `main/regulation.c` | Feedforward T_attente, correction Kp, buffer dist/cycle |
| `main/regulation.h` | API calculs régulation |
| `main/calculs_hydraulique.c` | Lookup vitesse cible (double interpolation abaque) |
| `main/calculs_hydraulique.h` | `lookup_vitesse_cible()`, `calcul_surface_m2()`, `calcul_dose_inst_mm()` |
| `main/calculs_mecanique.c` | Rayon étage, dist pulse/cycle, étage courant, étalonnage |
| `main/calculs_mecanique.h` | API géométrie bobine |
| `main/config_nvs.c` | Lecture/écriture NVS (machine, programmes, state, stats, batterie) |
| `main/config_nvs.h` | `config_machine_t`, `config_programme_t`, `config_stats_t`, API |
| `main/webserver.c` | WiFi AP, httpd, WebSocket, sérialisation JSON, routes HTTP |
| `main/webserver.h` | `webserver_init()`, `webserver_broadcast_status()`, `webserver_broadcast_raw()` |
| `main/telemetry.c` | Tâche broadcast 500ms + `telemetry_envoyer_bilan()` |
| `main/telemetry.h` | `telemetry_init()`, `telemetry_envoyer_bilan()` |
| `main/batterie.c` | ADC1 GPIO 36, calibration, seuils configurables, pourcentage |
| `main/batterie.h` | `batt_status_t`, `batt_etat_t`, API batterie |
| `main/ota.c` | Handler POST /ota/update, partition swap, reboot |
| `main/ota.h` | `ota_init()`, `ota_register_handler()` |
| `main/webui/index.html` | UI web 3 onglets (embarquée via EMBED_FILES) |
| `main/webui.h` | Symboles `_binary_index_html_start/end` |
| `main/machines/machines.h` | `machine_profile_t`, `MACHINES_LISTE[]`, `machine_get()` |
| `main/machines/machines.c` | Tableau profils + `machine_resoudre_double_entree()` |
| `main/machines/st1bis_82_330.c` | Profil Irrifrance ST1 Bis Ø82-330m |
| `main/abaques/abaques.h` | `canon_abaque_t`, `canon_entry_t`, `ABAQUES_LISTE[]` |
| `main/abaques/abaques.c` | Tableau abaques + `abaque_get()` |
| `main/abaques/sr150c.c` | Abaque Nelson SR 150C (13 entrées) |
| `main/simulator/simulator.c` | Simulation GPIO (CONFIG_IRRI_TEST_MODE) |
| `main/simulator/simulator_ws.c` | Routes WebSocket simulateur + parsing JSON injection |
| `main/simulator/simulator_ui.h` | Symboles UI simulateur embarquée |
| `test/host/` | Tests unitaires PC (Unity, stubs ESP-IDF) |

## Dépendances entre modules

```
main.c
  ├── config_nvs.c       (NVS flash)
  ├── batterie.c         (ADC1)
  ├── gpio_handler.c     (GPIO, ISR)
  ├── state_machine.c
  │     ├── securites.c  → gpio_handler.c, state_machine.c
  │     ├── regulation.c
  │     ├── calculs_hydraulique.c → abaques/
  │     ├── calculs_mecanique.c   → machines/
  │     ├── config_nvs.c
  │     ├── gpio_handler.c
  │     └── batterie.c
  ├── webserver.c        → state_machine.c (get_status), ota.c
  ├── telemetry.c        → webserver.c, state_machine.c
  └── ota.c
```

## GPIO — tableau complet

| GPIO | Direction | Signal | Logique | Conditionnement |
|---|---|---|---|---|
| **25** | SORTIE | EV_CANON (⚠️ provisoire) | HIGH = ouvert | Canal MOSFET Quad MOS OUT1 |
| **26** | SORTIE | EV_POUMON (⚠️ provisoire) | HIGH = ouvert | Canal MOSFET Quad MOS OUT2 |
| **27** | ENTRÉE | Pressostat | LOW = pression OK, HIGH = absent | Pull-up 10kΩ externe, NC |
| **32** | ENTRÉE | Sécurité spires (débordement) | LOW = normal, HIGH = SEC-2 | Pull-up 10kΩ externe, NC |
| **33** | ENTRÉE | Contact poumon plein | LOW = en cours, HIGH = plein | Pull-up 10kΩ externe, NC |
| **34** | ENTRÉE | Capteur vitesse (impulsions) | flancs montants ISR | Diviseur 10kΩ/3.3kΩ (12V→3V) |
| **35** | ENTRÉE | Fin de course canon | LOW = canon dehors, HIGH = rentré (SEC-1) | Pull-up 10kΩ externe, NC |
| **36** | ENTRÉE | Tension batterie (ADC1) | analogique 0..3.3V | Diviseur R1=100kΩ / R2=27kΩ |
| **2** | SORTIE | Heartbeat circuit RC (optionnel) | toggle 1Hz si `heartbeat_rc_on=true` | LED bleue intégrée ESP32 — défaut OFF |

**Logique universelle NC** : LOW = repos/normal, HIGH = danger/actif.
Fil coupé → HIGH → sécurité active → fail-safe.

## NVS — namespaces et clés

### Namespace `irri_machine`

| Clé | Type | Valeur défaut | Rôle |
|---|---|---|---|
| `machine_active` | i32 | 0 | Index profil machine |
| `t_vidange` | float (blob) | 0.0 | Durée vidange mécanique (s) ⚠️ à mesurer |
| `facteur_cor` | float | 1.0 | Étalonnage longueur |
| `dist_cycle` | float | 0.0 | Dernière dist/cycle connue (m) |
| `kp` | float | 0.1 | Gain correction vitesse |
| `n_calib` | i32 | 3 | Cycles avant activation correction |
| `fenetre_v` | i32 | 5 | Fenêtre vitesse (nb impulsions) |
| `max_cycles` | i32 | 15 | Cycles sans impulsion → v=0 |
| `mode_deg_b` | u8 | 0 | Mode dégradé contact poumon |
| `mode_deg_s` | u8 | 0 | Bypass SEC-2 spires |
| `t_rempl_fix` | float | 0.0 | Durée remplissage fixe (mode dégradé B) |
| `denivele` | float | 0.0 | Dénivelé terrain (m) |
| `cycles_tour` | float | 0.0 | Cycles poumon par tour de bobine |
| `batt_warn_v` | float | 11.5 | Seuil alerte batterie (V) |
| `batt_crit_v` | float | 11.0 | Seuil critique batterie (V) |
| `hb_rc` | u8 (bool) | 0 | Heartbeat GPIO 2 pour circuit RC (défaut OFF) |

### Namespace `irri_prog0` .. `irri_prog4`

| Clé | Type | Rôle |
|---|---|---|
| `nom` | str (21) | Nom programme |
| `dose_mm` | float | Dose cible (mm) |
| `largeur_m` | float | Largeur position (m) |
| `buse_mm` | i32 | Diamètre buse (mm) |
| `pression` | float | Pression enrouleur (bar) |
| `tdep_on` | u8 | Tempo départ activée |
| `tdep_s` | i32 | Tempo départ (s) |
| `tarr_on` | u8 | Tempo arrivée activée |
| `tarr_s` | i32 | Tempo arrivée (s) |

### Namespace `irri_state`

| Clé | Type | Rôle |
|---|---|---|
| `urgence` | str (64) | Raison dernier arrêt urgence ("" = pas d'urgence) |
| `longueur_m` | float | Position absolue interne (survit au reboot) |
| `deroule_m` | float | Longueur déployée terrain (survit au reboot) |
| `prog_actif` | i32 | Index programme actif |
| `session_act` | u8 | 1 = session en cours au moment du reboot (détection coupure) |

### Namespace `irri_stats`

| Clé | Type | Rôle |
|---|---|---|
| `camp` | blob (`config_stats_t`) | Stats campagne cumulatives |

## Format JSON WebSocket (ESP32 → navigateur)

Broadcast toutes les 500ms depuis `telemetry_task`. Taille max ~3072 bytes (JSON_BUF_SIZE).

```json
{
  "etat": "EN_COURS",
  "etat_code": 4,
  "sous_etat": 2,
  "raison_arret": "",
  "prog_nom": "Programme 1",
  "machine_nom": "ST1 Bis Ø82-330m",
  "abaque_nom": "SR 150C",
  "prog_dose_mm": 25.0,
  "prog_largeur_m": 66.0,
  "prog_buse_mm": 20,
  "prog_pression_bar": 6.5,
  "prog_tempo_depart_on": false,
  "prog_tempo_depart_s": 0,
  "prog_tempo_arrivee_on": false,
  "prog_tempo_arrivee_s": 0,
  "longueur_deroulee_m": 330.0,
  "longueur_enroulee_m": 45.2,
  "vitesse_m_h": 31.4,
  "vitesse_cible_m_h": 32.1,
  "duree_s": 5220,
  "heure_arrivee_unix": 1748606400,
  "heure_arrivee_relative_s": 4200,
  "heure_synchro": true,
  "surface_m2": 2983,
  "dose_inst_mm": 24.8,
  "debit_m3h": 31.9,
  "p_enrouleur_bar": 6.5,
  "p_buse_bar": 4.0,
  "etage_courant": 2,
  "nb_etages": 4,
  "ev_canon": true,
  "ev_poumon": false,
  "pression_ok": true,
  "fin_course": false,
  "secu_spires": false,
  "poumon_plein": true,
  "mesure_deroule_m": 0.0,
  "t_remplissage_ms": 4200,
  "t_attente_ms": 8100,
  "dist_par_cycle_m": 0.341,
  "cycles_par_min_cible": 1.57,
  "cycles_par_min_reel": 1.52,
  "cycles_total": 132,
  "alerte_pression_insuff": false,
  "mode_degrade_poumon": false,
  "mode_degrade_spires": false,
  "facteur_correction": 1.0,
  "camp_surface_ha": 1.24,
  "camp_volume_m3": 310.0,
  "camp_dose_moy_mm": 25.0,
  "camp_vitesse_moy_m_h": 31.0,
  "camp_nb_sessions": 5,
  "camp_duree_h": 12.3,
  "batterie_v": 12.5,
  "batterie_pct": 83,
  "batterie_etat": 2,
  "cfg_batt_warn_v": 11.5,
  "cfg_batt_crit_v": 11.0,
  "cfg_valide": true,
  "cfg_t_vidange_s": 0.0,
  "cfg_kp_regulation": 0.1,
  "cfg_n_cycles_calib": 3,
  "cfg_fenetre_vitesse": 5,
  "cfg_max_cycles_si": 15,
  "cfg_t_rempl_fixe_s": 0.0,
  "cfg_denivele_m": 0.0,
  "cfg_machine_active": 0,
  "cfg_cycles_par_tour": 40.0
}
```

### Codes `etat_code` (entier)

| Code | Nom | Description |
|---|---|---|
| 0 | `VEILLE` | Attente démarrage |
| 1 | `OUVERTURE_CANON` | EV_CANON ON, attend pression 3s |
| 2 | `TEMPO_DEPART` | Arrosage sur place avant enroulement |
| 3 | `REMPLISSAGE_POUMON` | EV_POUMON ON, attend poumon plein |
| 4 | `EN_COURS` | Régulation active |
| 5 | `PAUSE_PRESSION` | Pression perdue, attente retour |
| 6 | `TEMPO_ARRIVEE` | Arrosage final après fin de course |
| 7 | `ARRET_FINAL` | Session terminée normalement |
| 8 | `ARRET_URGENCE` | Incident matériel |
| 9 | `DEROULE` | Mesure longueur déployée |

### Codes `sous_etat` (entier, valide uniquement en EN_COURS)

| Code | Nom | Description |
|---|---|---|
| 0 | `SOUS_VIDANGE` | EV_POUMON OFF, ressort rétracte cliquet |
| 1 | `SOUS_ATTENTE` | EV_POUMON OFF, pause régulation |
| 2 | `SOUS_REMPLISSAGE` | EV_POUMON ON, bobine avance |

### Codes `batterie_etat` (entier)

| Code | Nom | Tension |
|---|---|---|
| 0 | `BATT_ETAT_CHARGE` | > 13.5V |
| 1 | `BATT_ETAT_PLEINE` | 12.4..13.5V |
| 2 | `BATT_ETAT_CORRECTE` | 11.8..12.4V |
| 3 | `BATT_ETAT_FAIBLE` | seuil_warn..11.8V |
| 4 | `BATT_ETAT_CRITIQUE` | < seuil_crit |

## Format JSON WebSocket (navigateur → ESP32)

```json
{"cmd": "start"}
{"cmd": "stop"}
{"cmd": "reset"}
{"cmd": "resume"}
{"cmd": "set_time", "ts": 1748606400}
{"cmd": "ev_canon", "actif": true}
{"cmd": "ev_poumon", "actif": true}
{"cmd": "start_deroule"}
{"cmd": "select_programme", "idx": 2}
{"cmd": "etalonner", "longueur_m": 285.0}
{"cmd": "set_longueur", "longueur_m": 247.0}
{"cmd": "save_programme", "idx": 0, "nom": "...", "dose_mm": 25.0, ...}
{"cmd": "save_machine", "t_vidange_s": 1.2, "kp": 0.1, ...}
{"cmd": "reset_campagne"}
```

## Endpoints HTTP

| Méthode | URL | Description |
|---|---|---|
| GET | `/` | Web UI (index.html embarqué) |
| GET | `/ws` | WebSocket (upgrade HTTP→WS) |
| POST | `/ota/update` | Upload firmware binaire (multipart/form-data) |
| GET | `/api/vitesse?p=X&b=Y&d=Z` | Preview vitesse cible → `{"vitesse_m_h":X,"debit_m3h":Y,"p_buse_bar":Z}` |
| GET | `/test` | UI simulateur (CONFIG_IRRI_TEST_MODE uniquement) |
| GET | `/ws_test` | WebSocket simulateur (CONFIG_IRRI_TEST_MODE uniquement) |
