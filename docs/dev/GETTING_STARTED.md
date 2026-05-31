# Getting Started — Irrifrance ESP32

Guide pour un développeur qui rejoint le projet.

---

## Prérequis

### Logiciels

| Outil | Version | Rôle |
|---|---|---|
| ESP-IDF | **v5.5.2** | Framework ESP32 (build, flash, monitor) |
| VSCode | récent | IDE principal |
| Extension ESP-IDF VSCode | récente | Intégration build/flash dans VSCode |
| Git | — | Gestion de version |
| Python | 3.11 | Utilisé par ESP-IDF (installé automatiquement) |

> **Important** : utilisez exactement ESP-IDF v5.5.2. Les versions antérieures peuvent manquer
> des APIs ADC ou avoir des différences dans l'API httpd WebSocket.

### Installation ESP-IDF (Windows)

1. Télécharger l'installateur officiel depuis [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/)
2. Choisir l'installation dans `C:\Users\<user>\esp\v5.5.2\esp-idf`
3. L'installateur place les outils dans `C:\Users\<user>\.espressif\`

Chemins résultants (utilisés dans le script PowerShell de build) :
```
C:\Users\<user>\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe
C:\Users\<user>\esp\v5.5.2\esp-idf\tools\idf.py
C:\Users\<user>\.espressif\tools\cmake\3.30.2\bin
C:\Users\<user>\.espressif\tools\ninja\1.12.1
C:\Users\<user>\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin
```

---

## Clone et configuration

```bash
git clone https://github.com/matfr11/remplacement_irridoseur.git
cd remplacement_irridoseur
```

Aucun sous-module. Aucune dépendance externe (tout est dans `main/` ou fourni par ESP-IDF).

---

## Premier build

### Windows (PowerShell)

```powershell
$idf_python = "C:\Users\$env:USERNAME\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$idf_path   = "C:\Users\$env:USERNAME\esp\v5.5.2\esp-idf"
$idf_script = "$idf_path\tools\idf.py"
$cmake_dir  = "C:\Users\$env:USERNAME\.espressif\tools\cmake\3.30.2\bin"
$ninja_dir  = "C:\Users\$env:USERNAME\.espressif\tools\ninja\1.12.1"
$xtensa     = "C:\Users\$env:USERNAME\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin"
$env:PATH     = "$xtensa;$cmake_dir;$ninja_dir;$env:PATH"
$env:IDF_PATH = $idf_path

Set-Location "C:\esp\irridoseur"
& $idf_python $idf_script build
```

### Linux/macOS (bash)

```bash
. ~/esp/v5.5.2/esp-idf/export.sh
cd /path/to/irridoseur
idf.py build
```

**Résultat attendu** : `Project build complete. [...] bytes free.`

Taille actuelle : ~900 KB, 53% flash libre sur ESP32 4MB.

---

## Flash et monitor

```bash
# Flash (remplacer PORT par le port série réel : COM3, /dev/ttyUSB0, etc.)
idf.py -p PORT flash

# Monitor série (115200 baud)
idf.py -p PORT monitor

# Les deux en une commande
idf.py -p PORT flash monitor
```

Quitter le monitor : `Ctrl+]`

---

## Connexion WiFi et accès Web UI

1. L'ESP32 démarre en **Point d'Accès WiFi** (AP)
2. SSID : `IRRIDOSEUR-XXXX` (XXXX = 4 derniers octets MAC)
3. Mot de passe : aucun (réseau ouvert)
4. Ouvrir un navigateur → `http://192.168.4.1`

L'IP `192.168.4.1` est l'adresse AP par défaut d'ESP-IDF. Elle est fixe.

---

## Tests unitaires PC (sans matériel)

Les tests tournent sur PC en compilant le code avec des stubs ESP-IDF.

```bash
cd test/host
mkdir build && cd build
cmake ..
make
./tests
```

> Les tests couvrent : calculs_hydraulique, calculs_mecanique, gpio (avec injection),
> regulation, state_machine (transitions), config_nvs (NVS simulé).

---

## Mode simulateur (TEST_MODE)

Le mode simulateur permet de tester l'UI et la machine d'états sans matériel réel.

### Activer le simulateur

```bash
idf.py menuconfig
# Component config → Irrifrance → Enable test/simulator mode
```

Ou directement dans `sdkconfig` :
```
CONFIG_IRRI_TEST_MODE=y
```

### Utiliser le simulateur

1. Flasher le firmware en mode test
2. Connecter au WiFi AP
3. Ouvrir `http://192.168.4.1/test` — page simulateur
4. Injecter des valeurs GPIO via les curseurs (fin_course, secu_spires, pression, etc.)
5. Observer la machine d'états réagir en temps réel dans l'UI principale (`http://192.168.4.1`)

Le simulateur remplace `gpio_get_level()` par `sim_gpio_get_level()` via macro `READ_GPIO`.
Les impulsions de vitesse peuvent être générées automatiquement (curseur vitesse).

---

## Commandes idf.py utiles

```bash
# Build complet
idf.py build

# Effacer le build (nécessaire si CMakeLists.txt modifié)
idf.py fullclean

# Effacer la NVS flash (reset config et urgences)
idf.py -p PORT erase-flash
# Puis reflasher :
idf.py -p PORT flash

# Voir la configuration du projet
idf.py menuconfig

# Taille du firmware
idf.py size

# Taille détaillée par composant
idf.py size-components

# OTA sans câble série (si ESP32 sur le réseau)
# Upload via l'onglet Config → OTA dans l'UI web
```

---

## Workflow quotidien recommandé

1. `idf.py build` — vérifie la compilation
2. `idf.py -p PORT flash monitor` — flash et observe les logs
3. Connecter smartphone au WiFi AP, ouvrir l'UI
4. En mode simulateur : tester les transitions d'état sans matériel
5. Pour les tests unitaires PC : `cd test/host/build && make && ./tests`
