# Irrifrance ESP32 — Remplacement régulation Irridoseur 3

> Remplacement de la carte de régulation **Irridoseur 3** (en panne) d'un enrouleur d'irrigation **Irrifrance Structure 1 bis** par un **ESP32 Quad MOS Switch Module**.
>
> Projet open source destiné à la communauté agricole francophone.

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.2-blue)
![Plateforme](https://img.shields.io/badge/plateforme-ESP32-red)
![Licence](https://img.shields.io/badge/licence-MIT-green)
![Statut](https://img.shields.io/badge/statut-en%20développement-orange)
![Tests PC](https://github.com/matfr11/remplacement_irridoseur/actions/workflows/tests.yml/badge.svg)

---

## Table des matières

- [Contexte](#contexte)
- [Principe de fonctionnement](#principe-de-fonctionnement)
- [Matériel nécessaire](#matériel-nécessaire)
- [Câblage](#câblage)
- [Architecture logicielle](#architecture-logicielle)
- [Interface web](#interface-web)
- [Installation et compilation](#installation-et-compilation)
- [Configuration initiale](#configuration-initiale)
- [Plan de développement](#plan-de-développement)
- [Contribuer](#contribuer)

---

## Contexte

L'**Irridoseur 3** est la carte de régulation d'origine d'un enrouleur d'irrigation Irrifrance. Elle régule la vitesse d'enroulement du tuyau PE pour délivrer une dose d'eau précise sur la parcelle.

Suite à une panne irréparable, ce projet remplace la carte d'origine par un **ESP32** programmé sous **ESP-IDF v5.x**, en reproduisant fidèlement la logique de régulation originale et en y ajoutant une **interface web mobile** accessible depuis un smartphone sur le terrain.

### Machine concernée

| Paramètre | Valeur |
|---|---|
| Modèle | Irrifrance Structure 1 bis |
| Canon | Nelson SR 100C |
| Tuyau PE | Ø82mm extérieur — épaisseur 6mm — 330m |
| Rayon tambour nu | 690mm (calculé) |
| Nombre d'étages de spires | 5 couches |
| Spires par étage (étages 1-4) | 13,45 (constructeur) |
| Spires dernier étage (5) | 6 (mesuré terrain) |
| Capteur vitesse | 10 pastilles / tour |
| Alimentation | Batterie 12V / 24Ah + panneau solaire |

---

## Principe de fonctionnement

L'enroulement est **hydraulique** : un **poumon** (vérin pneumatique) actionne un cliquet qui fait tourner la bobine. La vitesse d'enroulement est régulée par la fréquence des cycles du poumon.

```
Cycle poumon :
  Phase 1 — REMPLISSAGE  EV_POUMON=ON
    Le poumon pousse le cliquet → la bobine avance
    Le capteur vitesse compte les impulsions ICI
    Fin : contact poumon plein détecté

  Phase 2 — VIDANGE  EV_POUMON=OFF
    Le ressort rappelle le poumon — bobine IMMOBILE

  Phase 3 — ATTENTE  EV_POUMON=OFF
    Pause calculée par feedforward pour atteindre la vitesse cible
    T_attente = T_cycle_cible - T_remplissage_mesuré - T_vidange
```

### Machine d'états

```
VEILLE → OUVERTURE_CANON → TEMPO_DEPART → REMPLISSAGE_POUMON → EN_COURS → TEMPO_ARRIVEE → ARRET_FINAL
  ↑                                                                  ↓               ↓              ↓
  └──────── cmd_resume (session préservée) ─────────────────── PAUSE_PRESSION   ARRET_URGENCE ──────┘
DEROULE ← flanc fin_course (mesure longueur déployée tracteur)   (reprise auto)  (sécurité spires, timeout)
```

**Sous-états EN_COURS** : `SOUS_VIDANGE → SOUS_ATTENTE → SOUS_REMPLISSAGE` (la bobine avance pendant SOUS_REMPLISSAGE via cliquet).

---

## Matériel nécessaire

| Composant | Détail | Qté |
|---|---|---|
| **ESP32 Quad MOS Switch Module** | ESP32-32E intégré, 4 canaux MOSFET 5-60V DC, buck 12V intégré | 1 |
| Boîtier étanche IP65 | ~200×150×80mm | 1 |
| Bornier DIN rail 12 voies | Connexions terrain | 1 |
| Rail DIN 15cm | Fixé dans boîtier | 1 |
| Résistances 10 kΩ × 4 | Pull-up contacts NC — soudées sur fils + gaine thermo | 4 |
| Diviseur 10 kΩ + 3,3 kΩ | Capteur vitesse 12V → 3V — soudé sur fil + gaine thermo | 1 set |
| **Module INA3221 3 canaux I2C** | MCU-3221, shunts 0,1 Ω intégrés, CH1=EV_CANON CH2=EV_POUMON CH3=Batterie | 1 |
| **Module relais 1 canal 12V × 2** | Optocouplé, NC/NO/COM, cavalier HIGH level trigger, bobine 12V | 2 |

> ⚠️ **GPIO EV_CANON et EV_POUMON** (canaux MOSFET OUT1/OUT2) sont à identifier sur le schéma technique de la carte Quad MOS avant mise sous tension. Voir `main/gpio_config.h`.
>
> Les **modules relais** doivent être configurés en **HIGH level trigger** (cavalier physique sur le module). VCC signal : 3,3V — JD-VCC bobine : 12V (alimentation séparée).

---

## Câblage

### Affectation GPIO

| GPIO | Direction | Signal | Conditionnement |
|---|---|---|---|
| **34** | INPUT | Capteur vitesse bobine | Diviseur 10 kΩ/3,3 kΩ — 12V→3V — pas de pull-up interne |
| **35** | INPUT | Fin de course canon | Pull-up 10 kΩ — contact NC |
| **32** | INPUT | Sécurité spires | Pull-up 10 kΩ — contact NC |
| **33** | INPUT | Contact poumon plein | Pull-up 10 kΩ — contact NC |
| **25** | INPUT | Pressostat | Pull-up 10 kΩ — contact NC |
| **0** | INPUT | Bouton physique carte | Bouton intégré carte Quad MOS |
| **16** | OUTPUT | EV_CANON 12V — MOSFET principal | QMOS OUT1 carte Quad MOS |
| **17** | OUTPUT | EV_POUMON 12V — MOSFET principal | QMOS OUT2 carte Quad MOS |
| **26** | OUTPUT | EV_CANON — MOSFET secours | QMOS OUT3 — activé si OUT1 défaillant |
| **27** | OUTPUT | EV_POUMON — MOSFET secours | QMOS OUT4 — activé si OUT2 défaillant |
| **2** | OUTPUT | Relais basculement EV_CANON | LOW=principal (NC), HIGH=secours (NO) |
| **4** | OUTPUT | Relais basculement EV_POUMON | LOW=principal (NC), HIGH=secours (NO) |
| **21** | I2C SDA | INA3221 | Bus I2C partageable |
| **22** | I2C SCL | INA3221 | Bus I2C partageable |
| **23** | OUTPUT | Heartbeat RC fail-safe (LED carte) | Toggle 1 Hz — activable depuis Config → Machine |

### Logique des signaux — contacts NC (Normalement Fermés)

> **Règle fail-safe** : fil coupé ou capteur HS → GPIO HIGH → sécurité déclenchée.

| Signal | Repos LOW | Activé HIGH | Fil coupé |
|---|---|---|---|
| Fin de course | Canon déroulé ✅ | Canon rentré → SEC-1 | → SEC-1 ✅ |
| Sécurité spires | Normal ✅ | Débordement → SEC-2 | → SEC-2 ✅ |
| Contact poumon | En cours ✅ | Poumon plein | → Timeout → mode dégradé |
| Pressostat | Pression OK ✅ | Pas de pression | → Pause/attente |

### Bornier 12 voies

| Borne | Signal |
|---|---|
| 1 | 12V batterie |
| 2 | GND |
| 3 | Capteur vitesse → GPIO 34 via diviseur |
| 4 | Fin de course → GPIO 35 via pull-up |
| 5 | Sécurité spires → GPIO 32 via pull-up |
| 6 | Contact poumon plein → GPIO 33 via pull-up |
| 7 | Pressostat → GPIO 25 via pull-up |
| 8 | GND capteurs |
| 9-10 | EV_CANON + / − → COM relais 1 → (NC→OUT1 / NO→OUT3) → INA3221 CH1 |
| 11-12 | EV_POUMON + / − → COM relais 2 → (NC→OUT2 / NO→OUT4) → INA3221 CH2 |

> La mesure tension batterie (anciennement bornes 13-14 + diviseur 100 kΩ/27 kΩ) est désormais assurée par l'INA3221 CH3 câblé directement sur les bornes 1 (12V) et 2 (GND).

---

## Architecture logicielle

```
main/
├── gpio_config.h           — affectation GPIO centralisée (⚠️ identifier OUT1/OUT2 Quad MOS)
├── main.c                  — app_main, init, tâches FreeRTOS
├── gpio_handler.c/h        — config GPIO, ISR vitesse, contacts NC fail-safe, routage secours MOSFET
├── ina3221.c/h             — driver I2C INA3221 3 canaux — tension + courant EV/batterie
├── mosfet_surveillance.c/h — détection panne MOSFET (CC, HS, EV débranchée), basculement relais secours
├── securites.c/h           — watchdog SEC-1 / SEC-2 / SEC-P — exécuté en premier dans le tick
├── batterie.c/h            — tension batterie via INA3221 CH3, seuils NVS configurables, simulation
├── state_machine.c/h       — 10 états, sous-états poumon, pause pression, cmd_resume, stats campagne
├── calculs_hydraulique.c/h — formule analytique Torricelli + interpolation IDW p_buse — validation programme
├── calculs_mecanique.c/h   — rayon étage, dist/cycle, étage courant, étalonnage
├── regulation.c/h          — feedforward T_attente, correction Kp, moyenne dist_cycle
├── config_nvs.c/h          — NVS 4 namespaces, 5 programmes, stats campagne persistantes
├── webserver.c/h           — HTTP + WebSocket + endpoint /api/vitesse
├── ota.c/h                 — mise à jour firmware OTA
├── telemetry.c/h           — broadcast WebSocket 500ms
├── webui/index.html        — HTML/CSS/JS embarqué (3 onglets, sous-onglets Stats)
├── simulator/              — simulateur web interactif (CONFIG_IRRI_TEST_MODE)
├── machines/
│   ├── machines.h          — profils machine (machine_profile_t)
│   └── st1bis_82_330.c     — Irrifrance ST1 Bis Ø82-330m (profil référence)
├── abaques/
│   ├── abaques.h           — abaques constructeur (canon_abaque_t)
│   ├── sr150c.c            — Nelson SR 150C — 13 entrées
│   └── sr100c.c            — Nelson SR 100C — 24 entrées
└── test/                   — tests unitaires embarqués (CONFIG_IRRI_ENABLE_TESTS)

test/host/                  — tests unitaires PC (Unity/CMake, sans matériel)
├── CMakeLists.txt          — build natif Linux/macOS/Windows
├── mock/                   — stubs ESP-IDF (GPIO, NVS, FreeRTOS, log, timer)
├── helpers/                — helpers de test (config NVS valide/invalide)
├── scenarios/              — scénarios d'intégration (cycle complet, urgences, modes dégradés)
└── test_*.c                — 71 tests unitaires (71/71 verts en CI)
```

### Tâches FreeRTOS

| Tâche | Priorité | Période | Rôle |
|---|---|---|---|
| `state_machine_task` | 10 (haute) | 100 ms | Régulation, sécurités, machine d'états |
| `telemetry_task` | 5 | 500 ms | Diffusion statut JSON WebSocket |
| ISR `isr_capteur_vitesse` | IRAM | événement | Comptage impulsions, horodatage |

### NVS — 4 namespaces

| Namespace | Contenu |
|---|---|
| `irri_machine` | Profil actif, t_vidange, facteur_correction, Kp, cycles_par_tour, modes dégradés, seuils batterie |
| `irri_prog` × 5 | Programmes d'arrosage (dose, largeur, buse, pression, temporisations) |
| `irri_state` | Raison du dernier arrêt urgence + longueurs (persistant après redémarrage) |
| `irri_stats` | Stats campagne cumulatives (surface ha, volume m³, dose moy, vitesse moy, nb sessions, durée) |

---

## Interface web

L'ESP32 génère son propre **point d'accès WiFi** — aucun réseau externe nécessaire sur le terrain.

| Paramètre | Valeur |
|---|---|
| SSID | `Irrifrance-ESP32` |
| Mot de passe | `irrigation` (WPA2) |
| IP fixe | `192.168.4.1` |
| URL | `http://192.168.4.1` |

### 3 onglets

| Onglet | Contenu |
|---|---|
| **Accueil** | Badge état, tuiles (déroulé/enroulé/vitesse/durée), barre de niveau batterie (tension/état/%), estimation heure d'arrivée, alertes, boutons DEMARRER / ARRET URGENCE / RESET / **REPRENDRE SESSION** / Mesurer déroulement |
| **Stats** | Sous-onglet **Session** (surface ha, dose, débit, pressions, étage, cycles/min, dist/cycle, timings) + Sous-onglet **Campagne** (cumulatifs saison + bouton reset) |
| **Config** | Programme actif avec **preview vitesse live**, profil machine, modes dégradés (bypass spires), seuils batterie (alerte/critique), OTA, IRRITESTEUR |

### Bouton "Reprendre session"

Affiché en ARRET_URGENCE / ARRET_FINAL uniquement si une session était en cours (`longueur_enroulee > 0`). Retourne en VEILLE en **préservant la position et les longueurs de session**, contrairement à RESET qui remet tout à zéro.

**Cas débordement bobine** : le bouton est masqué tant que le capteur `secu_spires` est encore actif. Une alerte orange indique "Debordement actif", remplacée par une alerte verte "Debordement resolu" dès que le capteur se libère — REPRENDRE SESSION devient alors disponible. Le firmware rejette aussi la commande `resume` si le capteur est encore actif (double sécurité).

**Réarmement physique (3 appuis poumon_plein)** : conserve désormais les longueurs de session (comportement "reprendre" au lieu de "reset"). Bloqué si le débordement bobine est encore actif.

### Preview vitesse programme

Dans l'onglet Config, dès que l'opérateur renseigne pression / buse / dose, la vitesse cible estimée s'affiche en temps réel sous les champs (appel `/api/vitesse?p=X&b=Y&d=Z`). Le preview s'affiche aussi automatiquement au chargement d'un programme déjà configuré.

### Mode IRRITESTEUR

Accessible depuis Config — pilotage manuel EV_CANON et EV_POUMON hors cycle, lecture entrées en temps réel.

---

## Installation et compilation

### Prérequis

- [ESP-IDF v5.5.2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) installé
- ESP32 Quad MOS Switch Module connecté en USB-C

### Compilation

```bash
git clone git@github.com:matfr11/remplacement_irridoseur.git
cd remplacement_irridoseur
idf.py set-target esp32
idf.py build
```

### Flash

```bash
idf.py -p PORT flash monitor
```

ou flash manuel (Windows PowerShell) :

```powershell
python -m esptool --chip esp32 -b 460800 --before default_reset --after hard_reset write_flash `
    --flash_mode dio --flash_size 4MB --flash_freq 40m `
    0x1000 build\bootloader\bootloader.bin `
    0x8000 build\partition_table\partition-table.bin `
    0xe000 build\ota_data_initial.bin `
    0x10000 build\irrifrance-esp32.bin
```

### Tests unitaires PC (sans matériel)

Exécutés automatiquement sur GitHub Actions à chaque push.

```bash
cd test/host
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/irrifrance_tests
```

### Activer les tests unitaires embarqués

```bash
idf.py menuconfig
# → Irrifrance ESP32 → [x] Activer les tests unitaires embarqués
idf.py build flash monitor
```

### Build simulateur web interactif

```bash
idf.py menuconfig
# → Irrifrance ESP32 → [x] Activer le simulateur web interactif
idf.py build flash
# → http://192.168.4.1/test
```

---

## Configuration initiale

Au premier démarrage, les valeurs par défaut issues de la fiche technique sont utilisées.
**Paramètres à mesurer ou vérifier sur machine avant le premier arrosage :**

| Paramètre | Comment mesurer | Défaut / valeur connue |
|---|---|---|
| `PIN_EV_CANON` / `PIN_EV_POUMON` | QMOS OUT1/OUT2 carte Quad MOS | GPIO **16** / **17** ✅ |
| `t_vidange_s` | Chrono depuis EV_POUMON=OFF jusqu'à détection reprise capteur | 2,0 s ⚠️ à affiner terrain |
| `cycles_par_tour` | Compter les cycles poumon pour 1 tour complet de bobine | **40** sur ST1 Bis ✅ |

> Avec `cycles_par_tour > 0`, la vitesse est automatiquement estimée depuis les cycles poumon (mode dégradé vitesse transparent) — le capteur optionnel de vitesse reste utile pour la calibration.

---

## Plan de développement

| PR | Statut | Contenu |
|---|---|---|
| **PR-01** | ✅ Fait | Structure projet, CMakeLists, partitions OTA, tous les squelettes SPECS_FINAL_v3 |
| **PR-02** | ✅ Fait | GPIO handler complet — ISR vitesse robuste — mode dégradé A — 9 tests unitaires |
| **PR-03** | ✅ Fait | Calculs hydrauliques — double interpolation abaque — p_buse_out — 16 tests |
| **PR-04** | ✅ Fait | Calculs mécaniques — profils machine — auto-calibration — étalonnage — 16 tests |
| **PR-05** | ✅ Fait | Watchdog sécurités — machine d'états complète — 11 tests simulation |
| **PR-06** | ✅ Fait | Régulation feedforward — modes dégradés — 10 tests — câblage tests dans main.c |
| **PR-07** | ✅ Fait | NVS config — 5 programmes — fix namespace prog_actif — 10 tests |
| **PR-08** | ✅ Fait | WiFi AP — WebSocket broadcast + 10 cmds — OTA — sync heure |
| **PR-09** | ✅ Fait | Web UI mobile embarquée — 3 onglets (dark theme, WebSocket, OTA) |
| **PR-10** | ✅ Fait | Intégration complète — rechargement config live, bilan session, UI Config initialisée |
| **PR-11** | ✅ Fait | Tests unitaires PC Unity/CMake (47 tests, CI GitHub Actions) + simulateur web `/test` |
| **Post PR-11** | ✅ Fait | Fix vitesse m/h (×3600), cmd_resume, stats session calculées, campagne NVS, preview vitesse programme, ETAT_DEROULE, estimation heure arrivée, mode_deg_spires, calcul dist_cycle_m |
| **PR-12** | ✅ Fait | Mesure tension batterie ADC1 GPIO 36 — diviseur 100kΩ/27kΩ — seuils NVS — barre UI — simulateur slider |
| **PR-13** | ✅ Fait | Reprendre après sécurité débordement bobine — alertes UI état capteur — 3-tap reprendre (longueurs préservées) |
| **PR-14** | ✅ Fait | Robustesse — TWDT + détection coupure secteur + heartbeat GPIO RC fail-safe + alerte UI coupure |
| **PR-15** | ✅ Fait | Correction fin de course — arrêt normal vs SEC-1 — seuil configurable `fin_course_seuil_m` |
| **PR-16** | ✅ Fait | Alerte dose trop basse — vitesse max physique + dose corrigée exposées dans UI |
| **PR-17** | ✅ Fait | Sécurité longueur SEC-L — arrêt urgence si longueur enroulée > longueur déployée + seuil |
| **PR-18** | ✅ Fait | Calculs hydrauliques analytiques Torricelli — validation programme — hints bornes UI — warnings |
| **Post PR-18** | ✅ Fait | Abaque SR 100C — sélection canon UI — mode jour/nuit UI — fix profil ST1 Bis 5 étages — tests Wokwi e2e — preview vitesse au chargement programme |
| **PR-19** | ✅ Fait | Surveillance MOSFETs INA3221 I2C — mesure tension+courant EV/batterie — détection CC/HS/EV débranchée — basculement automatique sur relais secours OUT3/OUT4 — 9 tests unitaires embarqués |
| **fix/PR-19** | ✅ Fait | 8 corrections post-revue : bug critique lecture pin secours, verifier_apres hors mutex, guard INA3221 sur fil EV commun, retour bool basculement, reset relais urgence, filtre tension batterie |
| **fix/code-review** | ✅ Fait | 10 corrections revue complète : mutex récursif urgence (critique), guards NaN/sqrtf/powf hydraulique, validation NVS floats, guards mécanique, taille OTA, stack telemetry, null-termination strings |

---

## Documentation

| Document | Audience | Contenu |
|---|---|---|
| [docs/dev/GETTING_STARTED.md](docs/dev/GETTING_STARTED.md) | Développeur | Prérequis, build, flash, tests, simulateur |
| [docs/dev/ARCHITECTURE.md](docs/dev/ARCHITECTURE.md) | Développeur | Vue d'ensemble, flux données, ajouter machine/abaque |
| [docs/dev/HARDWARE.md](docs/dev/HARDWARE.md) | Développeur / installateur | Câblage détaillé, diviseurs de tension, mesures terrain |
| [docs/dev/API_WEBSOCKET.md](docs/dev/API_WEBSOCKET.md) | Développeur | Format JSON complet, toutes les commandes, exemple session |
| [docs/dev/CONTRIBUER.md](docs/dev/CONTRIBUER.md) | Contributeur | Conventions commits, workflow Git, règles nommage |
| [docs/dev/TROUBLESHOOTING.md](docs/dev/TROUBLESHOOTING.md) | Développeur / utilisateur | Problèmes connus, logs Serial, reset NVS, OTA |
| [SPECS_FINAL_v3.md](SPECS_FINAL_v3.md) | Référence | Spécifications complètes système (source de vérité) |
| [CHANGELOG.md](CHANGELOG.md) | Tous | Historique des changements par PR |

---

## Machines compatibles

| Machine | Tuyau | Étages | Abaque | Statut |
|---|---|---|---|---|
| Irrifrance Structure 1 bis | PE Ø82mm — 330m | 5 | Nelson SR 100C | ✅ Testé (machine de référence) |
| Irrifrance Structure 1 bis | PE Ø82mm — 330m | 5 | Nelson SR 150C | ✅ Abaque intégré — sélectionnable depuis l'UI |
| Autre enrouleur | — | — | — | Contribuer — voir [CONTRIBUER.md](docs/dev/CONTRIBUER.md) |

---

## Contribuer

Les contributions sont bienvenues, notamment pour adapter le projet à d'autres enrouleurs.

### Ajouter un profil machine

1. Créer `main/machines/[nom].c` avec `machine_profile_t`
2. Déclarer `extern` dans `machines.h` et ajouter à `MACHINES_LISTE[]`
3. Ouvrir une PR

### Ajouter un abaque canon

1. Créer `main/abaques/[canon].c` avec `canon_abaque_t`
2. Déclarer `extern` dans `abaques.h` et ajouter à `ABAQUES_LISTE[]`
3. Ouvrir une PR

### Format des commits

```
feat: nouvelle fonctionnalité
fix: correction bug
test: ajout/modification tests
docs: documentation
refactor: refactorisation
```

Voir le guide complet : [docs/dev/CONTRIBUER.md](docs/dev/CONTRIBUER.md)

---

## Licence

Ce projet est distribué sous licence **MIT**.

Utilisation libre pour usage personnel, agricole ou commercial.
Contributions bienvenues via Pull Request sur GitHub.

---

*Installation de référence : Irrifrance Structure 1 bis — tuyau PE Ø82mm / 330m — canon SR 100C — terrain plat*
