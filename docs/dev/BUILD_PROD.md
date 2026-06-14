# BUILD_PROD — Irrifrance ESP32
## Workflow de build production et nommage firmware

> Ce document décrit comment compiler un firmware de production
> avec profil machine et abaque canon hard-codés.
> En mode production, la configuration machine est verrouillée
> à la compilation — seuls les paramètres opérateur restent
> modifiables depuis la web UI.

---

## 1. Deux modes de build

### Mode développement (défaut)

```
idf.py build

Résultat : irrifrance_dev.bin

Comportement :
  Profil machine depuis NVS (modifiable web UI) ✅
  Abaque canon depuis NVS (modifiable web UI) ✅
  Toute la section Config → Machine visible ✅
  Idéal pour tests et développement
```

### Mode production

```
Profil machine hard-codé à la compilation ✅
Abaque canon hard-codé à la compilation ✅
Section Config → Machine masquée (non modifiable) ✅
NVS machine ignorée au démarrage ✅
Firmware nommé avec machine + canon + version ✅
```

---

## 2. Nommage firmware

```
Format : irrifrance_{machine}_{canon}_v{version}.bin

Exemples :
  irrifrance_st1bis_82_330_sr100c_v1.2.3.bin
  irrifrance_st1bis_82_330_sr150c_v1.2.3.bin
  irrifrance_st2atp82290_sr150c_v1.2.3.bin

Machine : identifiant sans espaces ni caractères spéciaux
Canon   : identifiant sans espaces ni caractères spéciaux
Version : MAJOR.MINOR.PATCH (depuis main/version.h)
```

---

## 3. Fichier version.h

```c
// main/version.h
#pragma once

#define IRRI_VERSION_MAJOR  1
#define IRRI_VERSION_MINOR  2
#define IRRI_VERSION_PATCH  3
#define IRRI_VERSION        "1.2.3"

/**
 * Incrémenter à chaque release :
 *   PATCH : correctif bug
 *   MINOR : nouvelle fonctionnalité compatible
 *   MAJOR : changement incompatible (NVS, câblage, API)
 */
```

---

## 4. Kconfig — options de build

```kconfig
# Kconfig.projbuild — à placer à la racine du projet

menu "Irrifrance — Configuration production"

config IRRI_PROD
    bool "Mode production (profil machine hard-codé)"
    default n
    help
        En mode production, le profil machine et l'abaque
        canon sont compilés dans le firmware.
        La configuration NVS machine est ignorée au démarrage.
        La section machine/canon de la web UI est masquée.
        Utiliser ce mode pour les firmwares de release.

choice IRRI_MACHINE_SELECT
    prompt "Profil machine"
    depends on IRRI_PROD
    default IRRI_MACHINE_ST1BIS_82_330

    config IRRI_MACHINE_ST1BIS_82_330
        bool "Structure 1 Bis — Ø82mm / 330m"

    config IRRI_MACHINE_ST2ATP_82_290
        bool "Structure 2 AT/P — Ø82mm / 290m"

    # Ajouter ici les nouvelles machines contribuées

endchoice

choice IRRI_CANON_SELECT
    prompt "Abaque canon"
    depends on IRRI_PROD
    default IRRI_CANON_SR100C

    config IRRI_CANON_SR100C
        bool "Irrifrance SR 100C"

    config IRRI_CANON_SR150C
        bool "Irrifrance SR 150C"

    # Ajouter ici les nouveaux canons contribués

endchoice

endmenu
```

---

## 5. Implémentation — config_nvs.c

```c
/**
 * Chargement profil machine au démarrage.
 *
 * Mode PROD  : constante compilée, NVS ignorée
 * Mode DEV   : depuis NVS (modifiable web UI)
 */
void config_nvs_charger_machine(config_machine_t *cfg) {

#ifdef CONFIG_IRRI_PROD

    // ── Profil machine hard-codé ────────────────────────────
    #if defined(CONFIG_IRRI_MACHINE_ST1BIS_82_330)
        *cfg = MACHINE_ST1BIS_82_330;

    #elif defined(CONFIG_IRRI_MACHINE_ST2ATP_82_290)
        *cfg = MACHINE_ST2ATP_82_290;

    #else
        #error "Aucun profil machine sélectionné — "\
               "activer CONFIG_IRRI_PROD et choisir IRRI_MACHINE_*"
    #endif

    // ── Abaque canon hard-codé ──────────────────────────────
    #if defined(CONFIG_IRRI_CANON_SR100C)
        cfg->abaque_idx = ABAQUE_IDX_SR100C;

    #elif defined(CONFIG_IRRI_CANON_SR150C)
        cfg->abaque_idx = ABAQUE_IDX_SR150C;

    #else
        #error "Aucun abaque canon sélectionné — "\
               "activer CONFIG_IRRI_PROD et choisir IRRI_CANON_*"
    #endif

    ESP_LOGI("config", "Mode PROD — %s / %s v%s",
             cfg->nom, cfg->nom_canon, IRRI_VERSION);

#else

    // ── Mode DEV : lecture NVS ──────────────────────────────
    nvs_lire_machine(cfg);
    ESP_LOGI("config", "Mode DEV — profil machine depuis NVS");

#endif
}
```

---

## 6. CMakeLists.txt — nommage automatique du binaire

```cmake
# CMakeLists.txt (racine)

cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Lire la version depuis version.h
file(READ "${CMAKE_SOURCE_DIR}/main/version.h" VERSION_H)
string(REGEX MATCH "IRRI_VERSION \"([^\"]+)\"" _ ${VERSION_H})
set(IRRI_VERSION ${CMAKE_MATCH_1})

# Nommage firmware selon le mode
if(CONFIG_IRRI_PROD)

    # Identifier machine et canon depuis sdkconfig
    if(CONFIG_IRRI_MACHINE_ST1BIS_82_330)
        set(MACHINE_ID "st1bis_82_330")
        set(MACHINE_LABEL "ST1 Bis Ø82-330m")
    elseif(CONFIG_IRRI_MACHINE_ST2ATP_82_290)
        set(MACHINE_ID "st2atp82290")
        set(MACHINE_LABEL "ST2 AT/P Ø82-290m")
    endif()

    if(CONFIG_IRRI_CANON_SR100C)
        set(CANON_ID "sr100c")
        set(CANON_LABEL "SR 100C")
    elseif(CONFIG_IRRI_CANON_SR150C)
        set(CANON_ID "sr150c")
        set(CANON_LABEL "SR 150C")
    endif()

    set(FIRMWARE_NAME
        "irrifrance_${MACHINE_ID}_${CANON_ID}_v${IRRI_VERSION}")

    message(STATUS "Build PROD : ${FIRMWARE_NAME}.bin")
    message(STATUS "  Machine  : ${MACHINE_LABEL}")
    message(STATUS "  Canon    : ${CANON_LABEL}")
    message(STATUS "  Version  : ${IRRI_VERSION}")

else()
    set(FIRMWARE_NAME "irrifrance_dev")
    message(STATUS "Build DEV  : ${FIRMWARE_NAME}.bin")
endif()

project(${FIRMWARE_NAME})
```

---

## 7. JSON WebSocket — info firmware

```json
{
  "firmware_version": "1.2.3",
  "firmware_machine": "ST1 Bis Ø82-330m",
  "firmware_canon":   "SR 100C",
  "firmware_prod":    true,
  "firmware_nom":     "irrifrance_st1bis_82_330_sr100c_v1.2.3"
}
```

Affichage web UI — pied de page :
```
Irrifrance ESP32 | ST1 Bis Ø82-330m | SR 100C | v1.2.3 PROD
```

---

## 8. Web UI — section machine en mode PROD

```c
// webui/index.html.in

// En mode PROD : section machine/canon masquée
// Seuls les paramètres opérateur restent configurables

#ifdef CONFIG_IRRI_PROD
// ── Affiché en PROD ─────────────────────────────────────────
// Config → Machine (lecture seule)
//   Machine : ST1 Bis Ø82-330m    [verrouillé 🔒]
//   Canon   : SR 100C              [verrouillé 🔒]

// ── Masqué en PROD ──────────────────────────────────────────
// Dropdown sélection machine
// Dropdown sélection canon
// Bouton "Changer de machine"
#endif

// ── Toujours configurables (PROD et DEV) ────────────────────
// t_vidange_s
// facteur_correction
// duree_impulsion_ev_ms
// seuils_batterie
// tempo_depart / arrivee
// dose, pression, buse (programmes)
```

---

## 9. Paramètres toujours modifiables en PROD

```
Ces paramètres restent configurables depuis la web UI
même en mode production — ils sont spécifiques
à l'installation et à l'usage, pas à la machine :

Programmes (5 slots) :
  ✅ Dose cible (mm)
  ✅ Pression enrouleur (bar)
  ✅ Diamètre buse (mm)
  ✅ Espacement (m)
  ✅ Tempo départ / arrivée

Calibration terrain :
  ✅ t_vidange_s (mesure terrain)
  ✅ facteur_correction (étalonnage longueur)
  ✅ duree_impulsion_ev_ms (test EV)

Sécurités :
  ✅ seuil_batt_alerte_v
  ✅ seuil_batt_critique_v

Watchdog :
  ✅ reprise_auto_on
  ✅ t_ouv_canon_s
```

---

## 10. Commandes de build

### Build développement (défaut)

```bash
idf.py build

# Sortie : build/irrifrance_dev.bin
# Flash   : idf.py flash monitor
```

### Build production — ST1 Bis + SR 100C

```bash
# Supprimer le sdkconfig dev s'il existe
rm -f sdkconfig

# Première passe — génère le sdkconfig PROD depuis les defaults
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.st1bis_82_330_sr100c" build

# Reconfigurer pour appliquer le nom PROD (limitation ESP-IDF : sdkconfig
# est généré après le premier cmake, idf.py reconfigure force la relecture)
idf.py reconfigure
idf.py build

# Sortie : build/irrifrance_st1bis_82_330_sr100c_v2.1.1.bin
```

> En CI (GitHub Actions), `sdkconfig` n'existe jamais au départ — les deux passes
> sont déjà enchaînées automatiquement par ESP-IDF.

### sdkconfig.st1bis_82_330_sr100c (fichier à créer)

```
CONFIG_IRRI_PROD=y
CONFIG_IRRI_MACHINE_ST1BIS_82_330=y
CONFIG_IRRI_CANON_SR100C=y
```

### sdkconfig.st1bis_82_330_sr150c

```
CONFIG_IRRI_PROD=y
CONFIG_IRRI_MACHINE_ST1BIS_82_330=y
CONFIG_IRRI_CANON_SR150C=y
```

---

## 11. GitHub Actions — build automatique des releases

```yaml
# .github/workflows/release.yml
name: Build release firmwares

on:
  push:
    tags:
      - 'v*'   # Déclenché sur git tag v1.2.3

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        config:
          - {sdkconfig: "st1bis_sr100c",  label: "ST1 Bis + SR 100C"}
          - {sdkconfig: "st1bis_sr150c",  label: "ST1 Bis + SR 150C"}
          # Ajouter ici les nouvelles combinaisons

    name: ${{ matrix.config.label }}

    steps:
      - uses: actions/checkout@v4

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.5.2

      - name: Build firmware
        run: |
          idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.${{ matrix.config.sdkconfig }}" \
                 build

      - name: Upload firmware
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ matrix.config.sdkconfig }}
          path: build/irrifrance_*.bin

  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4

      - name: Créer la release GitHub
        uses: softprops/action-gh-release@v1
        with:
          files: firmware-**/*.bin
          body: |
            ## Firmwares disponibles

            | Fichier | Machine | Canon |
            |---------|---------|-------|
            | irrifrance_st1bis_82_330_sr100c_*.bin | ST1 Bis Ø82-330m | SR 100C |
            | irrifrance_st1bis_82_330_sr150c_*.bin | ST1 Bis Ø82-330m | SR 150C |

            ## Installation
            Flasher via OTA depuis la web UI Config → Mise à jour firmware
            ou via : `idf.py -p PORT flash`
```

---

## 12. Workflow release complet

```
1. Développer et tester en mode DEV

2. Incrémenter version dans main/version.h
   Ex: "1.2.2" → "1.2.3"

3. Merger sur main

4. Créer un tag Git :
   git tag v1.2.3
   git push origin v1.2.3

5. GitHub Actions build automatiquement :
   irrifrance_st1bis_82_330_sr100c_v1.2.3.bin
   irrifrance_st1bis_82_330_sr150c_v1.2.3.bin
   ...

6. Release GitHub créée automatiquement
   avec tous les firmwares en pièces jointes

7. Agriculteur télécharge son firmware :
   → Identifie sa machine et son canon
   → Télécharge le .bin correspondant
   → Flash via OTA ou USB ✅
```

---

## 13. Contribution — ajouter une nouvelle machine

```
Pour la communauté GitHub :

1. Créer main/machines/ma_machine.c
   avec machine_profile_t MA_MACHINE = {...}

2. Ajouter dans Kconfig.projbuild :
   config IRRI_MACHINE_MA_MACHINE
       bool "Ma Machine — Øxx / xxxm"

3. Ajouter dans config_nvs.c (bloc #ifdef PROD) :
   #elif defined(CONFIG_IRRI_MACHINE_MA_MACHINE)
       *cfg = MA_MACHINE;

4. Ajouter dans CMakeLists.txt :
   elseif(CONFIG_IRRI_MACHINE_MA_MACHINE)
       set(MACHINE_ID "mamachine")

5. Ajouter dans release.yml :
   - {sdkconfig: "mamachine_sr100c", label: "Ma Machine + SR 100C"}

6. Créer sdkconfig.mamachine_sr100c :
   CONFIG_IRRI_PROD=y
   CONFIG_IRRI_MACHINE_MA_MACHINE=y
   CONFIG_IRRI_CANON_SR100C=y

7. Ouvrir une PR → firmware disponible à la prochaine release ✅
```

---

## 14. Fichiers à créer / modifier

```
Nouveau :
  Kconfig.projbuild           ← options PROD/machine/canon
  main/version.h              ← version sémantique
  sdkconfig.st1bis_82_330_sr100c     ← config prod ST1 Bis + SR 100C
  sdkconfig.st1bis_82_330_sr150c     ← config prod ST1 Bis + SR 150C
  .github/workflows/release.yml ← build + release automatique

Modifier :
  CMakeLists.txt              ← nommage firmware
  main/config_nvs.c           ← chargement machine PROD vs DEV
  main/webserver.c            ← JSON firmware info
  main/webui/index.html.in    ← section machine verrouillée PROD
```

---

## 15. PR associée

```
PR-24 : Build production + nommage firmware
  Commits :
    feat: version.h + versioning sémantique
    feat: Kconfig.projbuild — options PROD/machine/canon
    feat: CMakeLists.txt — nommage firmware automatique
    feat: config_nvs — chargement machine PROD vs DEV
    feat: web UI — section machine verrouillée en PROD
    feat: JSON WebSocket — info firmware version/machine/canon
    ci: GitHub Actions — build + release firmwares automatiques
    docs: BUILD_PROD.md
```

---

*BUILD_PROD.md — Irrifrance ESP32*
*Mode DEV : configuration NVS modifiable*
*Mode PROD : profil machine + canon hard-codés à la compilation*
*Nommage : irrifrance_{machine}_{canon}_v{version}.bin*
