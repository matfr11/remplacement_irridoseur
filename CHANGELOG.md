# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [Keep a Changelog](https://keepachangelog.com/fr/1.0.0/)

---

## [refonte/ev-bistables] — 2026-06-13 — EVs bistables à impulsion + capteur 2 fils 2V/8V

### ⚠️ Rupture de compatibilité matérielle

Le firmware supposait des **électrovannes monostables** (courant continu) avec failover par
relais SPDT. Le matériel réel est fondamentalement différent.

### Changed — Architecture matérielle

- **EVs bistables à impulsion 6V/5W** : une brève impulsion 100 ms suffit pour changer d'état
  (OUVRIR ou FERMER) ; la vanne mémorise sa position mécaniquement sans courant permanent
- **4 sorties MOSFET, 2 par EV** : OUT1/GPIO16 = EV_CANON_OUVRIR, OUT3/GPIO26 = EV_CANON_FERMER,
  OUT2/GPIO17 = EV_POUMON_OUVRIR, OUT4/GPIO27 = EV_POUMON_FERMER
- **Module LM2596** : abaisse le 12V batterie à 6V pour alimenter les bobines EV
- **Capteur vitesse 2 fils 2V/8V** : sans pastille → 2V, avec pastille → 8V — remplace
  l'ancienne conception 3 fils NPN 12V
- **Diviseur capteur** : R2 = 3,3 kΩ → **5,6 kΩ** (8V → 2,87V ✅ / 2V → 0,72V ✅ pour l'ESP32)

### Removed

- **Module `mosfet_surveillance`** entièrement supprimé : les EVs bistables n'ont pas de courant
  permanent à mesurer, et 4 bobines + batterie dépassent les 3 canaux disponibles de l'INA3221
- **Relais SPDT** (GPIO2, GPIO4) : le failover par relais est impossible avec des EVs bistables
  (état mécanique inconnu à la reprise)
- **INA3221 CH1/CH2** : débranchés ; le module INA ne surveille plus que la batterie (CH3)
- Champs `mosfet_canon_secours`, `mosfet_poumon_secours`, `mosfet_canon_etat`, `mosfet_poumon_etat`
  retirés de `machine_status_t` et du JSON WebSocket

### Changed — Firmware

- `gpio_config.h` : nouveaux noms `PIN_EV_CANON_OUVRIR/FERMER`, `PIN_EV_POUMON_OUVRIR/FERMER`,
  `DUREE_IMPULSION_EV_MS = 100` ; suppression `PIN_RELAIS_*`, `PIN_MOSFET_SECOURS_*`,
  `SEUIL_TENSION_EV_V`, `SEUIL_COURANT_EV_MA`, `INA3221_CH_EV_*`
- `gpio_handler.c` : `gpio_ev_canon_set()` / `gpio_ev_poumon_set()` envoient une impulsion
  (HIGH 100ms puis LOW) au lieu d'un niveau continu ; état mémorisé en RAM
  (`s_ev_canon_ouverte`, `s_ev_poumon_ouverte`) — pas de lecture GPIO possible
- `gpio_ev_canon_get()` / `gpio_ev_poumon_get()` : retournent l'état RAM mémorisé
- `gpio_all_ev_off()` : pulse simultanée FERMER sur les deux EVs (100ms total)
- Boot : `gpio_handler_init()` appelle `gpio_all_ev_off()` pour établir un état connu
- Commande redondante ignorée (idempotent) : si l'EV est déjà dans l'état voulu, aucune
  impulsion n'est envoyée

### Comportement au reset watchdog

Après un reset TPL5010 (~5,3s timeout + ~2s boot), les EVs restent dans leur état mécanique
~7s avant que le boot envoie les impulsions FERMER. Comportement jugé acceptable.

### Tests

- `test_mosfet_surveillance.c` supprimé (embarqué + host)
- `test_gpio.c` (embarqué) : 4 nouveaux tests EVs bistables (ouverture, fermeture, idempotent,
  all_off) ; macro `ASSERT_EVS` utilise les getters d'état mémorisé

### Docs

- `docs/dev/SCHEMA_CABLAGE.md` : synoptique, bornier, capteur vitesse et chaîne EV mis à jour
- `docs/dev/HARDWARE.md` : EVs bistables, LM2596, nouveau diviseur, suppression section relais

---

## [fix/materiel-absent] — 2026-06-11 — INA absent ≠ batterie 12,5 V, pressostat débranché ≠ pression OK

Constats terrain v1.2.10 (banc sans INA3221 ni pressostat, sans pull-ups externes) :
la batterie affichait 12,5 V « Correcte » et la pression était « OK ».

### Fixed
- **Batterie — état « Inconnue » si aucune lecture INA valide** : le fallback
  dernière-valeur-valide (fix PR-19 bug 7) était initialisé en dur à 12,5 V ;
  sans INA il inventait donc une tension à vie. Tant qu'aucune lecture valide
  n'a eu lieu depuis le boot → `BATT_ETAT_INCONNUE` (gris, 0 V, 0 %). Le
  fallback reste actif pour les lectures transitoires aberrantes *après* une
  première lecture valide (`main/batterie.c`, enum dans `batterie.h`)

### Décision câblage — pull-ups externes uniquement
- Pressostat « OK » à 0 V : broche flottante sans pull-up externe (banc).
  **Règle unifiée actée : aucune pull interne au firmware**, toutes les entrées
  (35/32/33/25) exigent leur 10 kΩ externe — y compris sur banc de test, sinon
  fil coupé/capteur absent peut lire « tout va bien ». Commentaire dans
  `gpio_handler_init()`, avertissement dans le README (tableau GPIO + fail-safe)

### Tests
- `test_batterie.c` : +2 tests (160 au total) — INA absent → Inconnue/0 V/0 % ;
  INA débranché après une lecture valide → retour dernière valeur valide

### Docs
- README : pull-ups externes **obligatoires** sur les 4 entrées contacts,
  note INA absent → « Inconnue », correction ligne « fil coupé » du contact
  poumon (= « plein » permanent → remplissage écourté, pas un timeout)
- **Nouveau `docs/dev/SCHEMA_CABLAGE.md`** : synoptique ASCII complet, bornier,
  conditionnement des entrées, chaîne de puissance EV, I2C, ordre de montage,
  checklist de mise sous tension (incl. test fonctionnel « fil coupé »)
- **Bornier 12 voies redéfini** (rien n'était encore câblé — les layouts README
  et HARDWARE.md se contredisaient) : puissance à gauche (1-6 : 12V/GND/EV±),
  signaux à droite (7-12 : vitesse alim+signal, 4 contacts), retour commun des
  contacts sur la borne 2. README + HARDWARE.md alignés

---

## [tests/renforcement-couverture] — 2026-06-11 — Renforcement massif des tests host (71 → 158)

Suite à la revue 2026-06 : 11 bugs corrigés en 2 jours alors que les 71 tests étaient
verts. Autopsie → 4 failles structurelles, toutes comblées. **Chacun des 11 bugs a
désormais un test qui l'aurait détecté.**

### Couverture (gcovr, périmètre `main/` compilé sur host)
| | Avant | Après |
|---|---|---|
| Lignes | 78,4 % (1012/1290) | **90,8 % (1360/1497)** |
| Fonctions | 84,1 % | **96,3 % (131/136)** |
| Branches | 52,2 % | **69,9 %** |

Le périmètre a grossi de 207 lignes : `batterie.c`, `mosfet_surveillance.c` et
`json_utils.c` sont désormais compilés en réel sur host (plus des mocks vides).

### Added
- **Modules orphelins** : `test_batterie.c` (17), `test_mosfet_surveillance.c` (10),
  `test_json_utils.c` (17) — modules réels via nouveau `mock_ina3221`
  (`mock_batterie.c` et `mock_mosfet_surveillance.c` supprimés)
- **Assertions résultats** : `scenario_bilan_session.c` (5) — volume/dose du bilan,
  durée figée après ARRET_FINAL, NVS remis à zéro, stats campagne, pause exclue
- **Paramètre → effet** : `scenario_parametres.c` (7) — t_vidange, t_rempl_fixe,
  fin_course_seuil, facteur_correction, abaque_idx, cycles_par_tour (fallback profil),
  heartbeat_rc_on. Règle : tout paramètre machine doit prouver un effet observable
- **Commandes API WebSocket** : `scenario_commandes_api.c` (14) — cmd_start,
  cmd_ev_*, cmd_start_deroule, cmd_etalonner (garde-fous), cmd_reset_campagne,
  set_time, calc_vitesse, get_vitesse_max, programme_preview (3 cas dont warning
  vitesse limite)
- **Branches d'erreur** : `test_config_nvs.c` host (7) — NVS vierge, blob d'une
  ancienne version firmware (migration), seuils batterie hors plage ignorés,
  reset stats, `machine_get` hors bornes, `machine_resoudre_double_entree`
- **Hydraulique** : +10 tests — `valider_params_programme` (6 warnings),
  `calcul_esp_nominal_m`, interpolation IDW hors points exacts, garde-fous
- **GPIO physiques dans les scénarios** : macro `ASSERT_EVS` (test_helpers.h) —
  niveaux EV canon/poumon vérifiés à chaque état clé (cycle normal, pause
  pression, urgences : tout coupé), relais secours jamais commutés en nominal
- `json_utils.c/h` : parsing/escape JSON extraits de `webserver.c` (testables host)
- Option CMake `COVERAGE` (`cmake -B build_cov -DCOVERAGE=ON` + gcovr)

### Docs
- `README.md` (compteur tests, commande couverture), `docs/dev/CONTRIBUER.md`
  (convention `suite_*` réelle — l'ancienne section décrivait une API disparue)

---

## [cleanup/vitesse-impulsions] — 2026-06-11 — Retrait du calcul de vitesse par impulsions (legacy)

Le calcul de vitesse par fenêtre glissante d'impulsions était hérité d'une ancienne version
et **mort en production** : `dist_pulse_m` n'était jamais alimenté hors tests, le chemin
retournait toujours 0. La vitesse vient exclusivement du timing des cycles poumon depuis
l'introduction de `cycles_par_tour`. Les impulsions ne servent plus qu'à la **mesure de
longueur** (déroulé + progression par cycle).

### Removed
- `gpio_handler.c/h` : fenêtre glissante (`s_ts_pulses`, head/count, timeout cycles),
  `gpio_handler_set_params()`, `gpio_handler_tick_cycle()`, `gpio_handler_set_dist_pulse_m()` ;
  ISR réduite à anti-rebond + comptage ; `gpio_get_vitesse_m_h()` sans paramètre
- `config_nvs.h` : champs `fenetre_vitesse` et `max_cycles_si` retirés de `config_machine_t`
- `state_machine.c/h` : appels associés et champs `cfg_fenetre_vitesse`/`cfg_max_cycles_si` du statut
- `webserver.c` + UI : champs retirés du JSON statut, de `save_machine` et de l'onglet Config
  (« Impulsions fenêtre vitesse », « Max cycles sans impulsion »)

### ⚠️ Migration
- La taille du blob NVS `config_machine_t` change → au premier boot la config machine
  revient aux **valeurs par défaut** (détection gracieuse `ESP_ERR_NVS_INVALID_LENGTH`).
  **Re-saisir les paramètres machine** (machine active, t_vidange, kp, cycles_par_tour…)
  après flash. Les programmes, stats campagne et seuils batterie ne sont pas affectés
  (clés séparées).

### Tests
- `test_gpio.c` (embarqué) : 6 tests fenêtre/dist_pulse retirés, remplacés par 3 tests
  vitesse cycles poumon (injectée / mode désactivé / reset) + compteur impulsions conservé
- 71 tests host, 0 échec — build ESP-IDF complet sans warning

### Docs
- `API_WEBSOCKET.md`, `docs/claude/ARCHITECTURE.md`, `docs/claude/REGULATION.md`,
  `docs/dev/ARCHITECTURE.md`, `docs/dev/TROUBLESHOOTING.md`, `README.md` mis à jour
  (au passage : pressostat GPIO 27 → 25 dans TROUBLESHOOTING)

---

## [fix/code-review-2] — 2026-06-11 — Corrections revue générale (11 findings)

### Fixed
- **#1** `state_machine.c` : `s_nb_tentatives` n'est plus remis à 0 dans SOUS_ATTENTE — le compteur de timeouts poumon consécutifs n'est remis à 0 que sur remplissage réussi (sinon la limite « 2 tentatives → urgence » était inopérante en cycle)
- **#2** `mosfet_surveillance.c` : `mosfet_verifier_post_tick()` sort immédiatement sans INA3221 — supprime 20 ms de délai inutile à chaque tick sur les machines sans module de mesure
- **#3** `gpio_handler.c/h` : nouveaux getters `gpio_ev_canon_get()` / `gpio_ev_poumon_get()` lisant le pin réellement actif (principal ou secours OUT3/OUT4 après basculement MOSFET) — remplace 3 lectures directes `gpio_get_level(PIN_EV_*)` dans state_machine qui voyaient l'EV « fermée » après basculement
- **#4** `state_machine.c` : reprise après PAUSE_PRESSION repasse par OUVERTURE_CANON (re-stabilisation pression 30 ticks) ; la tempo départ interrompue **reprend pour le temps restant** (`s_tempo_depart_ecoulee_ms`, cumul multi-coupures), sautée uniquement si déjà effectuée en entier dans la session
- **#5** `state_machine.c` : bilan de session — `volume_m3` et `dose_moy_mm` calculés depuis `débit abaque × durée effective (hors pauses)` au lieu de `dose_inst_mm` (qui vaut 0 à l'arrêt, plus d'impulsions) — les stats de campagne NVS ne sont plus contaminées par des valeurs nulles
- **#6** `state_machine.c` : timeout du remplissage initial = `t_rempl_fixe_s` configurable (était 20 s codé en dur) ; chrono armé à l'ouverture réelle de l'EV — la 2ᵉ tentative dispose de son temps plein (l'attente vidange ne compte plus) ; le mode dégradé poumon fonctionne avec des temps > 20 s
- **#7** `state_machine.c` : `s_t_session_debut_ms = 0` à ARRET_FINAL et cmd_reset — `duree_s` se fige sur la durée finale au lieu de continuer à compter en VEILLE
- **#8** `state_machine.c` : clé NVS `deroule_m` remise à 0 en fin de session (ARRET_FINAL + les 2 branches de cmd_reset) — plus de faux log « Reboot mid-session » ni de déroulé fantôme après reboot
- **#9** commentaires heartbeat « GPIO 2 » → PIN_HEARTBEAT (GPIO 23, le GPIO 2 est désormais PIN_RELAIS_CANON) ; log init mosfet avec les macros PIN_* au lieu de numéros en dur
- **#10** `batterie.c/h` : le seuil d'alerte configurable `batt_warn_v` a maintenant un effet — frontière CORRECTE/FAIBLE (il était mort : branches dupliquées donnant le même état) ; pourcentage calculé entre `s_crit_v` (0 %) et `BATT_V_PCT_100` 12,6 V (était 11,0–12,6 codé en dur) ; `BATT_V_CORRECTE_MIN` supprimé
- **#11** `webserver.c` : `json_parse_string()` tolère les espaces après `:` (cohérent avec les parseurs float/int/bool) — les clients tiers type Python `json.dumps()` ne voyaient pas leurs champs string

### Tests
- `test_reprise_depuis_pause`, `scenario_perte_pression` ×2 : flux de reprise mis à jour (OUVERTURE_CANON + 30 ticks avant REMPLISSAGE_POUMON)
- `test_scenario_timeout_poumon_2tentatives` : 2ᵉ tentative = 50 ticks vidange + 200 ticks remplissage
- 71 tests host, 0 échec — build ESP-IDF complet sans warning

---

## [PR-20] — 2026-06-10 — Watchdog matériel TPL5010DDCR

### Added
- `tpl5010.c/h` : driver watchdog TPL5010DDCR
  - `tpl5010_init()` : configure GPIO13 en sortie LOW
  - `tpl5010_done_pulse()` : impulsion HIGH/LOW toutes les 2s depuis `state_machine_task()`
  - Si la tâche state_machine se bloque, le TPL5010 active RESET après ~5,3s → reboot complet ESP32 via EN
- `Kconfig.projbuild` : option `CONFIG_IRRI_TPL5010` (défaut OFF) — à activer une fois le composant câblé
- `gpio_config.h` : `PIN_TPL5010_DONE = GPIO13`
- `main.c` : `tpl5010_init()` au boot + `tpl5010_done_pulse()` dans la boucle tick (tout sous `#ifdef CONFIG_IRRI_TPL5010`)

### Hardware
- Composant : TPL5010DDCR SOT-23-6
- Câblage : GPIO13 → DONE, TPL5010 RESET → ESP32 EN, Rext = 3,3 MΩ (entre REXT et GND)
- Timeout : ~5,3 s (formule : t ≈ 1,35 × Rext × C_int, C_int ≈ 1,1 nF)
- Pulse logiciel : 100ms HIGH toutes les 2s (bien sous le timeout de 5,3s)

---

## [fix/code-review] — 2026-06-10 — Corrections revue complète codebase (10 bugs)

### Fixed
- **C1 (CRITIQUE)** `state_machine_declencher_urgence()` : migration vers mutex récursif — la fonction prenait le mutex nulle part et était appelée depuis `mosfet_verifier_post_tick()` (hors mutex) en lecture/écriture directe sur `s_etat`/`s_status`
- **C3** `ota.c` : validation taille firmware avant écriture (rejet si ≤ 0 ou > 2 Mo)
- **C5** `calculs_hydraulique.c` : guard `buse_mm > 0 && p_buse > 0` avant `powf()` dans `interpoler_p_buse()` — `powf(x, y)` retourne NaN pour x ≤ 0 avec y non-entier
- **C6** `calculs_hydraulique.c` : guard `p_buse > 0` avant `sqrtf()` et `dose_mm ≥ 1mm` avant division dans `lookup_vitesse_cible()`
- **M1** `state_machine.c` : null-termination explicite après chaque `strncpy()` sur les champs `prog_nom`, `machine_nom`, `abaque_nom`, `raison_arret`, `mosfet_canon_etat`, `mosfet_poumon_etat`
- **M4** `telemetry.c` : stack tâche telemetry 8192 → 10240 octets (marge insuffisante pour pics)
- **M7** `config_nvs.c` : validation `isfinite()` + plage physique sur les floats lus depuis NVS (`batt_warn_v`, `batt_crit_v`, `t_rempl_min`) — rejette NaN/Inf issus d'une flash corrompue
- **M8** `calculs_mecanique.c` : `d_tuyau_ext_m` négatif clampé à 0 dans `calcul_rayon_etage()` — rayon ne peut pas être inférieur au rayon du tambour vide
- **M9** `calculs_mecanique.c` : `spires ≤ 0` → retour 0 dans `calcul_longueur_etage_m()` — évite une longueur d'étage négative ou nulle propagée aux calculs de régulation

### Tests
- `test/host/mock/mock_freertos.h` : stubs `xSemaphoreCreateRecursiveMutex`, `xSemaphoreTakeRecursive`, `xSemaphoreGiveRecursive` ajoutés
- 71/71 tests passent

---

## [fix/PR-19] — 2026-06-10 — Corrections surveillance MOSFET (8 bugs)

### Fixed
- **Bug 1 (CRITIQUE)** `gpio_ev_*_set()` : lecture de `etat_actuel` depuis le pin actif (secours si basculé) au lieu du principal ; `pin_reel` recalculé après `mosfet_verifier_avant` car un basculement peut survenir entre les deux
- **Bug 2** `mosfet_verifier_apres()` déplacé hors mutex dans `mosfet_verifier_post_tick()` — évite de bloquer le mutex 100ms à chaque commande EV ; délai réduit 100ms → 20ms
- **Bug 3** Guard `secours_actif` ajouté dans `verifier_coherence()` — l'INA3221 étant câblé sur le fil EV commun après le relais SPDT, il mesurerait le secours après basculement ; la détection est inhibée pour éviter une fausse double-panne sur anomalie transitoire
- **Bug 4** `basculer_sur_secours()` retourne `bool` — l'ancienne vérification via `etat_secours != MOSFET_OK` ne pouvait jamais détecter l'échec du premier basculement
- **Bug 5** OUT3/OUT4 retirés du masque de `gpio_handler_init()` — responsabilité exclusive de `mosfet_surveillance_init()`
- **Bug 6** `gpio_all_ev_off()` remet les relais à 0 ; `mosfet_reset_etat()` ajouté et appelé depuis `state_machine_cmd_reset()`
- **Bug 7** `batterie_lire_voltage()` retourne la dernière valeur valide sur lecture aberrante ou nulle (évite urgence CRITIQUE sur timeout I2C transitoire)
- **Bug 8** Test dynamique au boot supprimé (ouvrait l'EV 100ms) ; `MOSFET_HS_OUVERT` détecté à la première utilisation réelle via `mosfet_verifier_post_tick()`

---

## [PR-19] — 2026-06-09 — Surveillance MOSFETs INA3221 + basculement relais secours

### Added
- `ina3221.c/h` : driver I2C INA3221 3 canaux (legacy `driver/i2c.h`, I2C_NUM_0, 400 kHz, GPIO21 SDA / GPIO22 SCL)
  - CH1 = EV_CANON, CH2 = EV_POUMON, CH3 = Batterie 12V
  - `ina3221_init()`, `ina3221_lire_canal(canal)` → `{tension_v, courant_ma}`, `ina3221_lire_tension(canal)`, `ina3221_est_ok()`
  - Shunt R = 0,1 Ω → courant_mA = shunt_µV / 100
- `mosfet_surveillance.c/h` : surveillance MOSFET + basculement automatique sur relais secours
  - Détection `MOSFET_GRILLE_CC` : tension > 6 V alors que GPIO = LOW
  - Détection `MOSFET_HS_OUVERT` : tension < 1 V alors que GPIO = HIGH
  - Détection `MOSFET_EV_DEBRANCHEE` : courant < 50 mA alors que GPIO = HIGH et tension OK
  - `mosfet_surveillance_init()`, `mosfet_test_demarrage()`, `mosfet_verifier_avant()`, `mosfet_verifier_apres()`
  - `basculer_sur_secours()` : synchronise OUT3/OUT4 avec l'état courant AVANT d'activer le relais (anti-glitch)
  - Double panne (principal + secours) → `state_machine_declencher_urgence()`
- `test/test_mosfet_surveillance.c` : 9 tests unitaires embarqués sous `CONFIG_IRRI_ENABLE_TESTS`
  - Simulation INA3221 via `mosfet_sim_set_mesure()` / `mosfet_sim_enable()`
- Champs `mosfet_canon_secours`, `mosfet_poumon_secours`, `mosfet_canon_etat`, `mosfet_poumon_etat` dans `machine_status_t` et JSON WebSocket

### Changed
- `batterie.c/h` : mesure tension batterie migrée ADC1 GPIO36 → INA3221 CH3 ; `batterie_init()` devient no-op si INA3221 déjà initialisé
- `gpio_handler.c` : `gpio_ev_canon_set()` / `gpio_ev_poumon_set()` intègrent le routage secours MOSFET (`mosfet_verifier_avant/apres()`)
- `gpio_handler_init()` : init GPIO26/27 (OUT3/OUT4 secours) en OUTPUT LOW
- `gpio_all_ev_off()` : coupe également GPIO26/27 (arrêt urgence inconditionnel)
- `gpio_config.h` : ajout PIN_RELAIS_CANON (GPIO2), PIN_RELAIS_POUMON (GPIO4), PIN_MOSFET_SECOURS_CANON (GPIO26), PIN_MOSFET_SECOURS_POUMON (GPIO27), I2C_SDA_PIN (GPIO21), I2C_SCL_PIN (GPIO22), seuils de détection ; suppression PIN_BATT_ADC 36
- `CMakeLists.txt` : ajout `ina3221.c`, `mosfet_surveillance.c`, `test/test_mosfet_surveillance.c` ; suppression composant `esp_adc`

### Hardware
- Deux modules relais SPDT 12V/3,3V signal ajoutés : LOW = repos = NC = MOSFET principal, HIGH = basculé = NO = secours
- Cavalier physique sur les modules relais configuré sur déclenchement niveau HAUT
- Bornier réduit de 14 à 12 voies (GPIO36 ADC supprimé)

---

## [PR-18] — 2026-06-01 — Calculs hydrauliques analytiques + validation programme

### Changed
- `lookup_vitesse_cible()` : remplace la double interpolation (colonnes D40/D30/D25/D20/D15) par la formule analytique de Torricelli
  - `Q (m³/h) = k_q × buse_mm² × √p_buse` — loi physique, continue pour toute dose
  - `V (m/h) = Q × 1000 / (largeur_m × dose_mm)` — pas de clamping dose
  - `p_buse` reste interpolé depuis l'abaque (2 voisins IDW sur p_enrouleur × buse_mm)
  - `largeur_m` désormais **obligatoire** (< 0.1 → LOGE + return 0)
- `canon_abaque_t` : 5 nouvelles constantes empiriques per-canon (`k_q`, `k_portee`, `portee_exp_buse`, `portee_exp_p`, `esp_factor`)
- SR 150C : `k_q = 0.039` (erreur < 1.5 % sur les 13 entrées), `k_portee = 7.06`, `esp_factor = 1.55`
- `/api/vitesse` : réponse étendue — débit en L/s, portée, esp_nominal, bornes min/max, warnings hydrauliques
- UI formulaire programme : label "Espacement entre positions", hints `[min–max]` sous chaque champ, débit en L/s, alertes jaunes non-bloquantes

### Added
- `calcul_esp_nominal_m()` : espacement recommandé = portée × esp_factor
- `valider_params_programme()` : retourne `hydro_warnings_t` (pression/buse/dose hors plage ±25 %, espacement trop serré/large, vitesse au-dessus du max théorique)
- `state_machine_programme_preview()` : préview complet pour `/api/vitesse` — vitesse, débit, portée, bornes, warnings en un seul appel
- `state_machine_get_vitesse_max()` : V_max = dist_cycle / (t_rempl_min + t_vidange) × 3600
- `t_rempl_min_s` : temps de remplissage minimum historique — clé NVS séparée `t_rempl_min` (namespace irri_machine), défaut 5 s, mis à jour uniquement si nouvelle valeur inférieure (limite l'usure flash), remis à 5 s sur `cmd_reset()`
- `programme_preview_t` dans `state_machine.h`
- `hydro_warnings_t` dans `calculs_hydraulique.h`
- `config_nvs_lire_t_rempl_min()` / `config_nvs_sauver_t_rempl_min()` dans `config_nvs.c`

### Fixed
- Tests unitaires `test_calculs_hydraulique.c` : valeurs attendues recalculées avec la formule analytique ; test `larg=0 → erreur` ajouté

**Taille firmware** : 0xe3680 bytes (~933 KB) — 53% flash libre

---

## [PR-17] — 2026-05-31 — Sécurité longueur SEC-L

### Added
- `state_machine_longueur_sec_depassee()` : retourne true si `longueur_session > longueur_deroule + fin_course_seuil_m`
- SEC-L dans `securites_watchdog()` : déclenche ARRET_URGENCE `"Securite longueur - enroule > deroule"` si EN_COURS et longueur dépassée
- Garde contre : capteur fin de course défaillant + comptage longueur qui continue
- N'agit pas si `longueur_deroule = 0` (déploiement non mesuré)
- Seuil partagé avec SEC-1 (`fin_course_seuil_m`, configurable, défaut 10m)

---

## [PR-16] — 2026-05-31 — Alerte dose trop basse — vitesse max et dose corrigée

### Added
- `vitesse_max_m_h` : vitesse max physique atteignable quand T_attente < 0 (cycle pneumatique saturé)
- `dose_corrigee_mm` : dose réellement délivrée à `vitesse_max_m_h`
- Les deux champs exposés dans `machine_status_t` et JSON WebSocket

### Changed
- Alerte `alert-pression` renommée : `"Dose configuree trop basse — vitesse max : X m/h — dose corrigee : Y mm"` (était : "Pression insuffisante" — message imprecis)
- `lookup_vitesse_cible()` : capture `debit_tmp` pour le calcul de dose corrigée

---

## [PR-15] — 2026-05-31 — Correction fin de course — arrêt normal vs SEC-1

### Fixed
- **Bug** : `fin_course` pendant `EN_COURS` déclenchait toujours ARRET_URGENCE (SEC-1 interceptait avant la machine d'états) — le code de fin normale dans EN_COURS était du code mort
- SEC-1 désormais conditionné à `!state_machine_fin_course_est_normale()` : si `longueur_restante <= fin_course_seuil_m`, la machine d'états gère → ARRET_FINAL propre
- `ETAT_REMPLISSAGE_POUMON` : détection `fin_course` ajoutée **après** `poumon_ok` (évite de couper EV_POUMON en cours de remplissage → rétraction cliquet → boucle infinie)
- `ETAT_EN_COURS` : guard `s_sous_etat != SOUS_REMPLISSAGE` pour la même raison

### Added
- `fin_course_seuil_m` (float, défaut 10m) dans `config_machine_t` + NVS `irri_machine`
- `state_machine_fin_course_est_normale()` — fonction sans mutex, appellable depuis `securites_watchdog()`
- Champ `cfg_fin_course_seuil_m` dans `machine_status_t` et JSON WebSocket
- Champ UI "Seuil fin de course normale (m)" dans Config → Machine (section Modes dégradés)

**Taille firmware** : 0xe1c70 bytes (~921 KB) — 53% flash libre

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
