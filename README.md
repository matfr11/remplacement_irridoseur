# Irrifrance ESP32 — Remplacement régulation Irridoseur 3

> Remplacement de la carte de régulation **Irridoseur 3** (en panne) d'un enrouleur d'irrigation **Irrifrance Structure 1 bis** par un **ESP32 DevKit C**.
>
> Projet open source destiné à la communauté agricole francophone.

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue)
![Plateforme](https://img.shields.io/badge/plateforme-ESP32-red)
![Licence](https://img.shields.io/badge/licence-MIT-green)
![Statut](https://img.shields.io/badge/statut-en%20développement-orange)

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
| Tuyau PE | Ø82mm intérieur — épaisseur 6mm — 330m |
| Diamètre extérieur tuyau | 94mm |
| Rayon tambour nu | 648mm (calculé fiche technique) |
| Nombre d'étages de spires | 4 couches |
| Capteur vitesse | Inductif 10 pastilles / tour |
| Alimentation | Batterie 12V / 24Ah + panneau solaire |

---

## Principe de fonctionnement

L'enroulement est **hydraulique** : un **poumon** (vérin pneumatique) actionne un cliquet qui fait tourner la bobine. La vitesse d'enroulement est régulée par la fréquence des cycles du poumon.

```
Cycle poumon :
  1. REMPLISSAGE  EV2=ON  → poumon se remplit (détecté par contact poumon plein)
  2. VIDANGE      EV2=OFF → ressort vide le poumon → cliquet → bobine tourne
  3. ATTENTE      EV2=OFF → pause calculée pour atteindre la vitesse cible
```

Le capteur de vitesse (10 pastilles sur la bobine) mesure la distance parcourue à chaque cycle et permet une **auto-calibration** et une **régulation feedforward + correction proportionnelle**.

### Machine d'états

```
VEILLE → TEMPO_DEPART → REMPLISSAGE_POUMON → EN_COURS → TEMPO_ARRIVEE → ARRET_FINAL
                                                  ↓
                                          ARRET_URGENCE (sécurité spires, timeout poumon)
```

---

## Matériel nécessaire

| Composant | Référence | Qté |
|---|---|---|
| ESP32 DevKit C (38 pins) | — | 1 |
| Module DC-DC buck 12V→5V | LM2596 min 1A | 1 |
| Module relais 2 canaux optocouplé | 5V, actif bas | 1 |
| Résistances 10 kΩ | Pull-up contacts secs | 4 |
| Diviseur tension capteur vitesse | 10 kΩ + 3,3 kΩ | 1 set |
| Diodes 1N4007 | Antiparasitage électrovannes | 2 |
| Condensateur 100 nF | Découplage alimentation | 1 |
| Boîtier étanche IP65 | Fixation machine | 1 |
| Borniers à vis | Connexions terrain | 1 set |

---

## Câblage

### Affectation GPIO ESP32

| GPIO | Direction | Signal | Conditionnement |
|---|---|---|---|
| **34** | INPUT | Capteur vitesse bobine | Diviseur 10 kΩ/3,3 kΩ (12V→3V) — pas de pull-up interne |
| **35** | INPUT | Fin de course canon | Pull-up 10 kΩ vers 3,3V — actif bas |
| **32** | INPUT | Sécurité spires (débordement) | Pull-up 10 kΩ vers 3,3V — actif bas |
| **33** | INPUT | Contact poumon plein | Pull-up 10 kΩ vers 3,3V — actif bas |
| **27** | INPUT | Pressostat RP1 | Pull-up 10 kΩ vers 3,3V — actif bas |
| **25** | OUTPUT | Relais EV1 — vanne canon 12V DC | LOW = relais actif |
| **26** | OUTPUT | Relais EV2 — poumon 12V DC | LOW = relais actif |

> ⚠️ GPIO 34 et 35 sont **input only** sur ESP32 — le pull-up/diviseur externe est obligatoire.

### Schéma contacts secs (GPIO 35, 32, 33, 27)

```
3,3V ──[10kΩ]──┬── GPIO
               │
           [Contacteur]
               │
              GND

Contact NO (repos ouvert)  → GPIO HIGH → actif : GPIO LOW
Contact NC (repos fermé)   → GPIO LOW  → actif : GPIO HIGH
```

> Le sens NO/NC de chaque contacteur est **configurable dans l'interface web**.

### Schéma diviseur capteur vitesse inductif NPN 12V (GPIO 34)

```
12V  ── fil brun  (VCC capteur)
GND  ── fil bleu  (GND capteur)
Signal ──[10kΩ]──┬── GPIO 34
                 │
               [3,3kΩ]
                 │
                GND
Signal résultant : 0V / ~3,0V ✅
```

### Alimentation générale

```
Batterie 12V
    │
[Fusible 2A]
    │
[DC-DC LM2596 12V→5V]
    ├── 5V → Module relais (VCC)
    └── 5V → ESP32 VIN
GND batterie → GND commun
```

---

## Architecture logicielle

```
main/
├── main.c                  — app_main, init, tâches FreeRTOS
├── gpio_handler.c/h        — config GPIO, ISR vitesse, sens NO/NC configurable
├── state_machine.c/h       — machine d'états, sous-états poumon, fail-safe
├── calculs_hydraulique.c/h — table débit constructeur, lookup vitesse cible
├── calculs_mecanique.c/h   — rayons étages, dist/pulse, auto-calibration, T_attente
├── config_nvs.c/h          — lecture/écriture NVS, 5 programmes, paramètres machine
├── webserver.c/h           — HTTP + WebSocket natif esp_http_server
├── telemetry.c/h           — abstraction LoRa (phase 2)
├── webui.h                 — HTML/CSS/JS embarqué (single-file)
└── test/                   — tests unitaires (activés via CONFIG_IRRI_ENABLE_TESTS)
```

### Tâches FreeRTOS

| Tâche | Priorité | Période | Rôle |
|---|---|---|---|
| `state_machine_task` | 5 (haute) | 100 ms | Régulation, sécurités, machine d'états |
| `websocket_task` | 3 (basse) | 500 ms | Diffusion statut JSON vers navigateur |
| ISR `isr_vitesse` | IRAM | événement | Comptage impulsions, mesure période |

### Paramètres NVS

Deux namespaces principaux :

- **`irri_machine`** — géométrie bobine, mécanique poumon, sens contacteurs, régulation
- **`irri_prog`** — 5 programmes d'arrosage (dose, largeur, buse, pression, temporisations)

---

## Interface web

L'ESP32 génère son propre **point d'accès WiFi** — aucun réseau externe nécessaire sur le terrain.

| Paramètre | Valeur |
|---|---|
| SSID | `Irrifrance-ESP32` |
| Mot de passe | `irrigation` |
| Adresse IP | `192.168.4.1` |
| URL navigateur | `http://192.168.4.1` ou `http://irrifrance.local` |

### 3 onglets

| Onglet | Contenu |
|---|---|
| 🌊 **Accueil** | État machine, longueur enroulée/déroulée, vitesse, heure d'arrivée, commandes démarrer/arrêter |
| 📊 **Stats** | Surface, dose, débit, pression canon, étage spires, timings poumon, volume eau |
| ⚙️ **Config** | 5 programmes d'arrosage, paramètres installation, sens contacteurs, mode IRRITESTEUR |

> Interface optimisée **mobile 6" portrait**, dark theme, boutons ≥ 48px, zéro CDN externe.

### Mode IRRITESTEUR

Accessible depuis l'onglet Config — permet de tester chaque entrée/sortie indépendamment sans lancer un cycle complet (inspiré du mode test de l'Irridoseur original).

---

## Installation et compilation

### Prérequis

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) installé et configuré
- ESP32 DevKit C connecté en USB

### Compilation et flash

```bash
git clone git@github.com:matfr11/remplacement_irridoseur.git
cd remplacement_irridoseur

# Configurer la cible
idf.py set-target esp32

# Compiler
idf.py build

# Flasher
idf.py -p PORT flash monitor
```

### Activer les tests unitaires

```bash
idf.py menuconfig
# → Irrifrance Configuration → [x] Activer les tests unitaires au démarrage
idf.py build flash monitor
```

---

## Configuration initiale

Au premier démarrage, les valeurs par défaut issues de la fiche technique sont utilisées.
**Un seul paramètre est à mesurer sur machine avant le premier arrosage :**

| Paramètre | Comment mesurer | Défaut |
|---|---|---|
| `t_vidange_s` | Chrono depuis EV2=OFF jusqu'à détection mouvement capteur vitesse | 5,0 s |

Les autres paramètres sont configurables depuis l'onglet **⚙️ Config** de l'interface web :

- Sens de chaque contacteur (NO/NC) — à vérifier au multimètre
- Diamètre extérieur tuyau (défaut : 94 mm)
- Rayon tambour nu (défaut : 648 mm)
- Gain de régulation Kp (défaut : 0,1)

---

## Plan de développement

| PR | Statut | Contenu |
|---|---|---|
| **PR-01** | ✅ Fait | Structure projet, CMakeLists, sdkconfig, squelettes modules |
| **PR-02** | 🔲 À faire | GPIO handler complet, ISR vitesse, mode IRRITESTEUR |
| **PR-03** | 🔲 À faire | Table débit constructeur, lookup vitesse cible |
| **PR-04** | 🔲 À faire | Calculs mécaniques, auto-calibration, T_attente feedforward |
| **PR-05** | 🔲 À faire | Machine d'états complète, sous-états poumon, fail-safe |
| **PR-06** | 🔲 À faire | NVS complet, 5 programmes, paramètres machine |
| **PR-07** | 🔲 À faire | WiFi AP, WebSocket, synchronisation heure |
| **PR-08** | 🔲 À faire | Interface web mobile — 3 onglets embarqués |
| **PR-09** | 🔲 À faire | Intégration complète, tests terrain simulés |
| **PR-10** | 🔲 À faire | Documentation finale, nettoyage |

---

## Contribuer

Les contributions sont bienvenues, notamment pour adapter le projet à d'autres modèles d'enrouleurs.

### Points d'adaptation pour une autre machine

1. **`NB_PASTILLES`** — constante dans `calculs_mecanique.h`
2. **Géométrie bobine** — `r_tambour_vide`, `d_tuyau_ext`, `nb_etages` via interface web
3. **Mécanique poumon** — `t_vidange_s` via interface web
4. **Longueur et diamètre tuyau** — via interface web
5. **Pertes de charge enrouleur** — `perte_enrouleur_bar` via interface web
6. **Dénivelé** — `denivele_m` via interface web (0 = terrain plat)

### Format des commits

```
feat: nouvelle fonctionnalité
fix: correction bug
test: ajout/modification tests
docs: documentation
refactor: refactorisation
```

### Branches

```
main          ← stable, protégée
develop       ← intégration continue
feature/PR-XX ← une branche par PR
```

---

*Installation de référence : Irrifrance Structure 1 bis — tuyau PE Ø82mm / 330m — terrain plat*
