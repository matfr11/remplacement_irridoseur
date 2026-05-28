# SPECS — Irrifrance ESP32
## Remplacement régulation Irridoseur 3 par ESP32

---

## 🤖 Instructions pour Claude Code

Tu es un programmeur embarqué expérimenté, expert en :
- **ESP-IDF v5.x** et FreeRTOS
- Développement C bas niveau pour microcontrôleurs
- Protocoles réseau embarqués (HTTP, WebSocket)
- Hydraulique agricole et systèmes de régulation industriels
- Git, GitHub, workflows de développement professionnel

### Tes règles de travail

1. **Qualité avant vitesse** — chaque fichier produit doit être propre, commenté, maintenable
2. **Tests à chaque étape** — pour chaque module implémenté, tu écris un test unitaire ou un mode de test dédié (mode IRRITESTEUR inspiré de l'original Irrifrance)
3. **Un commit par fonctionnalité** — commits atomiques avec messages clairs en anglais au format `feat:`, `fix:`, `test:`, `docs:`
4. **Une Pull Request par étape** — chaque étape du plan de développement fait l'objet d'une PR distincte avec description, tests effectués et captures si applicable
5. **Défensif par défaut** — toujours gérer les cas limites, les valeurs hors plage, les déconnexions, les états incohérents
6. **Sécurité machine** — en cas de doute sur un état, **toujours couper les électrovannes** (fail-safe)
7. **Code portable** — les paramètres spécifiques à l'installation de référence sont en NVS ou clairement documentés, pas hardcodés
8. **Commentaires en français** — le projet est destiné à une communauté agricole francophone sur GitHub

### Plan de développement — étapes et PRs

```
PR-01 : Structure projet + CMakeLists + sdkconfig.defaults
PR-02 : GPIO handler + ISR vitesse + tests entrées/sorties (mode IRRITESTEUR)
PR-03 : Calculs hydrauliques + table abaque + tests unitaires
PR-04 : Calculs mécaniques bobine + étages spires + tests unitaires
PR-05 : Machine d'états + logique poumon + tests simulation
PR-06 : NVS config + 5 programmes + tests lecture/écriture
PR-07 : WiFi AP + WebSocket + synchronisation heure
PR-08 : Web UI mobile (HTML/CSS/JS embarqué) — 3 onglets
PR-09 : Intégration complète + tests terrain simulés
PR-10 : README GitHub + documentation + nettoyage final
```

---

## 1. Contexte du projet

### Machine concernée
- **Enrouleur d'irrigation** : Irrifrance Structure 1 bis
- **Régulation remplacée** : Irridoseur 3 (panne)
- **Principe** : un tuyau PE de 330m s'enroule sur une bobine hydraulique. Un canon d'irrigation monté sur un chariot est tiré par le tuyau qui s'enroule. La vitesse d'enroulement détermine la dose d'eau appliquée.

### Principe mécanique de l'enroulement
Le moteur d'enroulement est **hydraulique** : un **poumon** (vérin pneumatique hydraulique) actionne un **cliquet** qui fait tourner la bobine d'un angle fixe α à chaque cycle. La vitesse d'enroulement linéaire est donc régulée par la **fréquence des cycles poumon** (nombre de cycles par minute).

### Alimentation
- Batterie 12V / 24Ah + panneau solaire + régulateur de charge
- Fonctionnement **en permanence** pendant toute la saison d'irrigation
- Pas de réseau WiFi externe sur le terrain — l'ESP32 génère son propre point d'accès WiFi

---

## 2. Matériel et câblage

### Composants nécessaires

| Composant | Référence | Quantité |
|---|---|---|
| ESP32 DevKit C (38 pins) | — | 1 |
| Module DC-DC buck 12V→5V | LM2596 min 1A | 1 |
| Module relais 2 canaux optocouplé | 5V, actif bas | 1 |
| Résistances 10kΩ | Pull-up contacts secs | 4 |
| Diviseur tension capteur vitesse | 10kΩ + 3.3kΩ | 1 set |
| Diodes 1N4007 | Antiparasitage EV | 2 |
| Condensateur 100nF | Découplage alim | 1 |
| Boîtier étanche IP65 | Fixation machine | 1 |
| Borniers à vis | Connexions terrain | 1 set |

### Affectation GPIO ESP32

| GPIO | Direction | Signal | Conditionnement |
|---|---|---|---|
| **34** | INPUT | Capteur vitesse bobine | Diviseur 10kΩ/3.3kΩ (12V→3V) — pas de pull-up interne possible |
| **35** | INPUT | Fin de course canon | Pull-up 10kΩ vers 3.3V — actif bas |
| **32** | INPUT | Sécurité spires (débordement) | Pull-up 10kΩ vers 3.3V — actif bas |
| **33** | INPUT | Contact poumon plein | Pull-up 10kΩ vers 3.3V — actif bas |
| **27** | INPUT | Pressostat RP1 | Pull-up 10kΩ vers 3.3V — actif bas |
| **25** | OUTPUT | Relais EV1 — vanne canon 12V DC | LOW = relais actif |
| **26** | OUTPUT | Relais EV2 — poumon 12V DC | LOW = relais actif |

> ⚠️ GPIO 34, 35 sont **input only** sur ESP32 (pas de pull-up interne) — le diviseur/pull-up externe est obligatoire.

### Schéma de câblage contacts secs
```
3.3V ──[10kΩ]──┬── GPIO xx
               │
           [Contacteur]
               │
              GND

État repos (ouvert)  → GPIO lit HIGH
État activé (fermé)  → GPIO lit LOW  ← logique ACTIVE BAS
```

### Schéma diviseur capteur vitesse inductif NPN 12V
```
12V  ── fil brun  (VCC capteur)
GND  ── fil bleu  (GND capteur)
Fil signal noir ──[10kΩ]──┬── GPIO 34
                           │
                         [3.3kΩ]
                           │
                          GND
Signal résultant : 0V / ~3.0V ✅ compatible ESP32 3.3V
```

### Câblage électrovannes 12V DC
```
12V ──────────────────────── EV (+)
                             EV (−)
                              │
                        [1N4007 antiparasitage — cathode vers 12V]
                              │
                      Contact NO module relais
                              │
                             GND

GPIO 25 → IN1 module relais → EV1 (vanne canon)
GPIO 26 → IN2 module relais → EV2 (poumon)
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

## 3. Capteurs — comportement et logique

### Capteur vitesse bobine (GPIO 34)
- **Type** : inductif NPN 12V
- **Couronne** : 10 pastilles métalliques (~15-20cm de diamètre)
- **Signal** : impulsion à chaque passage de pastille → 10 impulsions = 1 tour de bobine
- **ISR** : front montant (RISING), en IRAM, anti-rebond logiciel 5000µs
- **Timeout vitesse nulle** : si aucune impulsion depuis > 4000ms → vitesse = 0

### Fin de course canon (GPIO 35)
- **Type** : micro-switch à levier (contact sec)
- **Logique** : actif bas quand le chariot canon est revenu à la bobine
- **Action** : déclenche la fin de cycle (arrêt ou tempo arrivée)
- **Sens à vérifier** sur machine réelle : NO ou NC — adapter `!digitalRead()` en conséquence

### Sécurité enroulement spires (GPIO 32)
- **Type** : micro-switch mécanique (contact sec)
- **Logique** : actif bas si les spires débordent du diamètre prévu de la bobine
- **Action** : **ARRÊT TOTAL IMMÉDIAT** — EV1=OFF, EV2=OFF sans délai
- **Priorité maximale** — cette sécurité prime sur tout autre état

### Contact poumon plein (GPIO 33)
- **Type** : micro-switch (contact sec)
- **Logique** : actif bas quand le poumon est en fin de course (plein)
- **Action** : ferme EV2 (arrêt remplissage poumon)

### Pressostat RP1 (GPIO 27)
- **Type** : pressostat mécanique avec contacts (RP1 380V/4A = capacité contacts)
- **Logique** : actif bas quand la pression est présente dans le circuit
- **Rôle** : condition de démarrage automatique du cycle
- **Note** : seuil de déclenchement réglable mécaniquement sur le pressostat (vis de réglage)

---

## 4. Calculs hydrauliques

### 4.1 Pression effective au canon

```c
/**
 * Calcul de la pression effective au canon à partir de la pression
 * lue au manomètre positionné côté ARRIVÉE EAU (avant circuit enrouleur).
 *
 * Pertes prises en compte (source : UASA46, article N°1, 19/06/2025) :
 *   - Enrouleur        : 2.5 bar (médiane fourchette 2-3 bar)
 *   - Tuyau flexible   : 1 bar / 300m
 *   - Dénivelé         : 1 bar / 10m (positif = canon plus haut que bobine)
 *
 * INSTALLATION DE RÉFÉRENCE : Structure 1 bis, terrain plat → denivele_m = 0
 * Le paramètre denivele_m est conservé pour portabilité (autres installations).
 *
 * @param p_mano_bar      Pression lue au manomètre (bar)
 * @param longueur_tuyau_m Longueur du tuyau PE (m) — fixe = 330m sur réf.
 * @param denivele_m      Dénivelé bobine→canon (m) — 0 si terrain plat
 * @return Pression effective au canon (bar)
 */
float calcul_pression_canon(float p_mano_bar,
                             float longueur_tuyau_m,
                             float denivele_m);

// Exemple installation référence :
// p_mano = 7.0 bar → p_canon = 7.0 - 2.5 - (330/300 × 1.0) - 0 = 3.4 bar
```

### 4.2 Table débit abaque UASA46

Source : Union d'ASA du Lot, "Choisir le bon diamètre de buse", 19/06/2025.
Table empirique terrain — plus fiable que le calcul théorique avec coefficient Cd.

```c
/**
 * Table débit canon (m³/h)
 * Lignes  : pression au canon — 4.0 / 4.5 / 5.0 / 5.5 / 6.0 bar
 * Colonnes: diamètre buse     — 12 / 13 / 14 / ... / 26 mm (15 valeurs)
 */
static const float DEBIT_TABLE_M3H[5][15] = {
    /* 4.0 bar */ {10.8,12.7,14.7,16.9,19.2,21.7,24.3,27.1,30.0,33.1,36.4,39.8,43.3,47.0,50.8},
    /* 4.5 bar */ {11.4,13.4,15.6,17.9,20.4,23.0,25.8,28.8,31.9,35.1,38.6,42.2,45.9,49.8,53.9},
    /* 5.0 bar */ {12.1,14.2,16.4,18.9,21.5,24.3,27.2,30.3,33.6,37.1,40.7,44.5,48.4,52.5,56.8},
    /* 5.5 bar */ {12.7,14.9,17.2,19.8,22.5,25.5,28.5,31.8,35.2,38.9,42.7,46.6,50.8,55.1,59.6},
    /* 6.0 bar */ {13.2,15.5,18.0,20.7,23.5,26.6,29.8,33.2,36.8,40.6,44.6,48.7,53.0,57.5,62.2},
};
// Indices pression : (p_bar - 4.0) / 0.5  → 0 à 4
// Indices buse     : (d_mm - 12)           → 0 à 14

/**
 * Interpolation bilinéaire dans la table.
 * Si p_canon < 4.0 bar ou > 6.0 bar → warning log, extrapolation non garantie.
 * Si d_buse  < 12mm ou > 26mm       → warning log, extrapolation non garantie.
 *
 * @param p_canon_bar  Pression effective au canon (bar)
 * @param d_buse_mm    Diamètre buse (mm), entier de 12 à 26
 * @return Débit (m³/h)
 */
float calcul_debit_m3h(float p_canon_bar, int d_buse_mm);

// Conversion finale :
// debit_L_min = debit_m3h × 1000.0f / 60.0f
```

### 4.3 Dose et vitesse cible

```c
/**
 * Vitesse d'avancement cible du chariot canon pour atteindre la dose souhaitée.
 *
 * dose_mm   = débit_L_min / (largeur_m × vitesse_m_min)
 * → vitesse = débit_L_min / (largeur_m × dose_mm)
 *
 * dose en mm = L/m² (1 mm de pluie = 1 L/m²)
 *
 * @param debit_L_min   Débit calculé (L/min)
 * @param largeur_m     Largeur de position d'arrosage (m)
 * @param dose_mm       Dose cible (mm = L/m²)
 * @return Vitesse linéaire cible (m/min)
 */
float calcul_vitesse_cible_m_min(float debit_L_min,
                                  float largeur_m,
                                  float dose_mm);
```

---

## 5. Calculs mécaniques bobine

### 5.1 Modèle géométrique des étages de spires

Le tuyau PE s'enroule en couches concentriques autour du tambour. Le rayon effectif augmente avec chaque couche (étage).

```c
/**
 * Rayon effectif à l'étage n (numérotation depuis 1).
 *
 * R_n = R_tambour_vide + (n - 0.5) × d_tuyau_ext
 *
 * Exemple : R_tambour_vide=0.25m, d_tuyau=0.09m
 *   Étage 1 : R = 0.25 + 0.045 = 0.295 m
 *   Étage 2 : R = 0.25 + 0.135 = 0.385 m
 *   Étage 3 : R = 0.25 + 0.225 = 0.475 m
 *   Étage 4 : R = 0.25 + 0.315 = 0.565 m
 *
 * @param n                Numéro d'étage (1 à n_etages)
 * @param r_tambour_vide_m Rayon du tambour sans tuyau (m) — paramètre NVS
 * @param d_tuyau_ext_m    Diamètre extérieur du tuyau (m) — paramètre NVS
 * @return Rayon effectif à l'étage n (m)
 */
float calcul_rayon_etage(int n, float r_tambour_vide_m, float d_tuyau_ext_m);
```

### 5.2 Nombre de spires par étage et détermination de l'étage courant

```c
/**
 * Nombre de spires par étage (arrondi entier).
 * spires_par_etage = longueur_tuyau_total / (nb_etages × 2π × R_moyen_etage)
 *
 * En pratique : calculer depuis le comptage d'impulsions total.
 * nb_tours_total = nb_impulsions / NB_PASTILLES (= 10)
 * tours_par_etage = calculé depuis la géométrie
 *
 * Détermination étage courant :
 *   longueur_enroulee = nb_impulsions × dist_par_pulse
 *   → cumul par étage jusqu'à trouver l'étage courant
 */
int calcul_etage_courant(uint32_t nb_impulsions_total,
                          float r_tambour_vide_m,
                          float d_tuyau_ext_m,
                          float longueur_tuyau_m,
                          int   nb_etages);
```

### 5.3 Distance par impulsion et par cycle poumon

```c
/**
 * Distance linéaire de tuyau enroulée par impulsion capteur.
 * dist_pulse = (2π × R_etage) / NB_PASTILLES
 *
 * Distance linéaire par cycle poumon complet (remplissage+vidange).
 * dist_poumon = (alpha_deg / 360.0) × 2π × R_etage
 *
 * NB_PASTILLES = 10 (constant, mesuré sur machine)
 * alpha_deg    = paramètre NVS (à mesurer : marquer bobine, compter cycles/tour)
 */
#define NB_PASTILLES 10

float calcul_dist_pulse_m(float r_etage_m);
float calcul_dist_poumon_m(float r_etage_m, float alpha_deg);
```

### 5.4 Fréquence poumon cible

```c
/**
 * Fréquence de cycles poumon nécessaire pour atteindre la vitesse cible.
 *
 * freq_poumon (cycles/min) = vitesse_cible_m_min / dist_poumon_m
 *
 * Cette fréquence est recalculée à chaque changement d'étage de spires
 * car dist_poumon_m dépend du rayon effectif R_etage.
 *
 * Timing EV2 :
 *   - Durée ON  = duree_remplissage_s (paramètre NVS, fixe mécaniquement)
 *   - Durée OFF = (60.0 / freq_poumon) - duree_remplissage_s
 *   - Si durée OFF < 0 → vitesse non atteignable → log warning
 */
float calcul_freq_poumon(float vitesse_cible_m_min, float dist_poumon_m);
```

---

## 6. Machine d'états

### États

```c
typedef enum {
    ETAT_VEILLE,              // Attente programme configuré + pression
    ETAT_TEMPO_DEPART,        // Temporisation avant ouverture EV1
    ETAT_REMPLISSAGE_POUMON,  // EV2=ON, attend contact poumon plein
    ETAT_EN_COURS,            // EV1=ON, régulation fréquence poumon active
    ETAT_TEMPO_ARRIVEE,       // EV1=ON, EV2=OFF, minuteur tempo arrivée
    ETAT_ARRET_FINAL,         // EV1=OFF, EV2=OFF, affiche bilan
    ETAT_ARRET_URGENCE,       // EV1=OFF, EV2=OFF, message erreur, attend RESET
} etat_machine_t;
```

### Transitions

```
ETAT_VEILLE
  Condition démarrage :
    - Programme valide configuré (dose, largeur, buse, pression tous > 0)
    - Pressostat actif (pression présente)
  → Si tempo_depart_on ET tempo_depart_s > 0 : → ETAT_TEMPO_DEPART
  → Sinon                                      : → ETAT_REMPLISSAGE_POUMON

ETAT_TEMPO_DEPART
  - EV1=OFF, EV2=OFF
  - Décompte timer tempo_depart_s
  → Timer écoulé : → ETAT_REMPLISSAGE_POUMON
  → Arrêt manuel : → ETAT_ARRET_FINAL

ETAT_REMPLISSAGE_POUMON
  - EV2=ON (remplit le poumon)
  - Attente contact poumon plein (GPIO 33 actif bas)
  → Contact poumon plein détecté : EV2=OFF → ETAT_EN_COURS
  → Timeout 60s sans contact     : → ETAT_ARRET_URGENCE ("Timeout poumon")
  → Sécurité spires              : → ETAT_ARRET_URGENCE ("Sécurité spires")

ETAT_EN_COURS
  - EV1=ON (canon arrose)
  - Régulation active fréquence poumon via EV2 en cycles
  - Comptage impulsions vitesse → calcul étage, vitesse, dose, surface
  → Sécurité spires actif         : → ETAT_ARRET_URGENCE ("Sécurité spires — débordement bobine")
  → Fin de course canon actif     :
      Si tempo_arrivee_on ET s > 0 : → ETAT_TEMPO_ARRIVEE
      Sinon                        : → ETAT_ARRET_FINAL
  → Arrêt manuel opérateur        : → ETAT_ARRET_FINAL

ETAT_TEMPO_ARRIVEE
  - EV1=ON (canon continue d'arroser sur place)
  - EV2=OFF (enroulement arrêté)
  - Décompte timer tempo_arrivee_s
  → Timer écoulé  : EV1=OFF → ETAT_ARRET_FINAL
  → Arrêt manuel  : EV1=OFF → ETAT_ARRET_FINAL

ETAT_ARRET_FINAL
  - EV1=OFF, EV2=OFF
  - Calcul et affichage bilan : surface totale, dose moyenne, durée, volume eau
  → Commande RESET : remet compteurs à zéro → ETAT_VEILLE

ETAT_ARRET_URGENCE
  - EV1=OFF, EV2=OFF (IMMÉDIAT — priorité absolue)
  - Stockage raison en mémoire
  - Affichage alerte web UI
  → Commande RESET manuel obligatoire : → ETAT_VEILLE
```

### Règle générale fail-safe
```c
// En cas d'état inconnu ou d'erreur non gérée → couper tout
if (etat_invalide) {
    gpio_set_level(PIN_EV1, 0);
    gpio_set_level(PIN_EV2, 0);
    etat = ETAT_ARRET_URGENCE;
    strcpy(raison_arret, "Erreur état interne");
}
```

---

## 7. Régulation fréquence poumon

### Logique de pilotage EV2 en cours de cycle

```c
/**
 * Pilotage EV2 en cycles pour réguler la vitesse d'enroulement.
 *
 * Principe :
 *   - 1 cycle poumon = EV2 ON pendant duree_remplissage_s
 *                    + EV2 OFF pendant temps_attente_s
 *   - La fréquence des cycles détermine la vitesse linéaire
 *
 * Recalcul à chaque changement d'étage de spires.
 *
 * Paramètres :
 *   freq_cible_cycles_min = vitesse_cible / dist_poumon_etage_courant
 *   periode_s             = 60.0 / freq_cible_cycles_min
 *   t_off_s               = periode_s - duree_remplissage_s
 *
 * Guard : si t_off_s < 0.5s → vitesse max atteinte → log warning
 *         si freq_cible < 0.1 cycles/min → vitesse trop faible → log warning
 */
```

---

## 8. Paramètres NVS

### Namespace `irri_machine` — Paramètres fixes de l'installation
Ces paramètres ne changent pas d'un arrosage à l'autre.

| Clé NVS | Type | Défaut | Description |
|---|---|---|---|
| `longueur_tuyau` | float | 330.0 | Longueur tuyau PE (m) |
| `diam_int_tuyau` | float | 0.082 | Diamètre intérieur tuyau (m) |
| `r_tambour_vide` | float | 0.0 | ⚠️ À mesurer — rayon tambour nu (m) |
| `d_tuyau_ext` | float | 0.0 | ⚠️ À mesurer — diamètre ext tuyau (m) |
| `nb_etages` | int | 4 | Nombre de couches de spires |
| `alpha_poumon` | float | 0.0 | ⚠️ À mesurer — angle rotation/cycle poumon (°) |
| `duree_rempl_s` | float | 0.0 | ⚠️ À mesurer — durée remplissage poumon (s) |
| `perte_enroul` | float | 2.5 | Perte de charge enrouleur (bar) |
| `denivele_m` | float | 0.0 | Dénivelé bobine→canon (m), 0=plat |

> ⚠️ Les paramètres marqués "À mesurer" déclenchent un avertissement dans la web UI si encore à 0. L'enrouleur ne peut pas démarrer si ces valeurs sont nulles.

### Namespace `irri_prog` — 5 programmes d'arrosage
Schéma de clé : `p{idx}_{champ}` avec idx de 0 à 4.

| Clé NVS | Type | Défaut | Description |
|---|---|---|---|
| `p{n}_nom` | string | "Programme N" | Nom personnalisé (20 chars max) |
| `p{n}_dose` | float | 25.0 | Dose cible (mm = L/m²) |
| `p{n}_largeur` | float | 18.0 | Largeur de position (m) |
| `p{n}_buse` | int | 18 | Diamètre buse (mm, 12 à 26) |
| `p{n}_pression` | float | 6.0 | Pression manomètre (bar) |
| `p{n}_tdep_on` | uint8 | 0 | Tempo départ activée (0/1) |
| `p{n}_tdep_s` | int | 0 | Durée tempo départ (s) |
| `p{n}_tarr_on` | uint8 | 0 | Tempo arrivée activée (0/1) |
| `p{n}_tarr_s` | int | 0 | Durée tempo arrivée (s) |

### Namespace `irri_state` — État persistant
| Clé NVS | Type | Description |
|---|---|---|
| `prog_actif` | int | Index programme actif (0 à 4) |

**Total : 9×5 + 9 + 1 = 55 clés NVS** — bien dans les limites ESP32 (max ~100 clés/namespace).

---

## 9. Architecture logicielle ESP-IDF

### Structure des fichiers

```
irrifrance-esp32/
├── CMakeLists.txt
├── sdkconfig.defaults
├── README.md
├── SPECS.md                    ← ce fichier
├── CHANGELOG.md
├── .github/
│   └── PULL_REQUEST_TEMPLATE.md
└── main/
    ├── CMakeLists.txt
    ├── main.c                  ← app_main, init, démarrage tâches
    ├── gpio_handler.c          ← config GPIO, ISR, lecture états
    ├── gpio_handler.h
    ├── state_machine.c         ← machine d'états, logique poumon
    ├── state_machine.h
    ├── calculs_hydraulique.c   ← pression, débit, dose, table abaque
    ├── calculs_hydraulique.h
    ├── calculs_mecanique.c     ← étages, rayon, distance, fréquence
    ├── calculs_mecanique.h
    ├── config_nvs.c            ← lecture/écriture NVS, 5 programmes
    ├── config_nvs.h
    ├── webserver.c             ← HTTP + WebSocket natif esp_http_server
    ├── webserver.h
    ├── telemetry.c             ← couche abstraction (vide — LoRa futur)
    ├── telemetry.h
    ├── webui.h                 ← HTML/CSS/JS embarqué en PROGMEM
    └── test/
        ├── test_calculs_hydraulique.c
        ├── test_calculs_mecanique.c
        └── test_state_machine.c
```

### Architecture FreeRTOS

```c
void app_main(void) {
    // 1. Init NVS flash
    // 2. Init GPIO
    // 3. Init WiFi AP
    // 4. Init WebServer + WebSocket
    // 5. Démarrage tâches

    xTaskCreate(state_machine_task, "state_machine",
                4096, NULL, 5, NULL);  // Priorité haute

    xTaskCreate(websocket_task, "websocket",
                8192, NULL, 3, NULL);  // Priorité basse
}

// Tâche 1 : machine d'états (100ms)
void state_machine_task(void *pvParam) {
    while (1) {
        tick_state_machine();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tâche 2 : envoi état WebSocket (500ms)
void websocket_task(void *pvParam) {
    while (1) {
        ws_broadcast_status();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ISR : capteur vitesse (IRAM)
void IRAM_ATTR isr_vitesse(void *arg) {
    // Capture timestamp, calcul période, incrémente compteur
    // Anti-rebond logiciel 5000µs
}
```

### Couche télémétrie (vide — LoRa futur)

```c
// telemetry.h
/**
 * Couche d'abstraction pour la télémétrie distante.
 * Phase 1 : implémentations vides (log Serial uniquement).
 * Phase 2 : brancher module LoRa SX1276/SX1262 sur SPI sans
 *           modifier le reste du code.
 *
 * GPIOs SPI disponibles pour LoRa futur :
 *   MOSI=23, MISO=19, SCK=18, CS=5, RST=14, DIO0=2
 */
void telemetry_send_status(const machine_status_t *status);
void telemetry_send_alarm(alarm_type_t alarm, const char *message);
void telemetry_send_session_end(const session_summary_t *summary);
```

---

## 10. WiFi et WebSocket

### Configuration WiFi Access Point

```c
// sdkconfig.defaults
CONFIG_ESP_WIFI_SSID="Irrifrance-ESP32"
CONFIG_ESP_WIFI_PASSWORD="irrigation"
CONFIG_ESP_WIFI_CHANNEL=6
CONFIG_LWIP_LOCAL_HOSTNAME="irrifrance"

// IP fixe : 192.168.4.1
// URL     : http://192.168.4.1
```

### Synchronisation heure

```c
/**
 * L'ESP32 n'a pas de RTC matérielle.
 * Synchronisation : le navigateur envoie un timestamp Unix à chaque
 * connexion WebSocket (automatique, invisible pour l'utilisateur).
 *
 * Après sync : heure d'arrivée affichée en HH:MM absolu
 * Sans sync  : heure d'arrivée affichée en "+HH:MM" relatif
 *
 * Dérive oscillateur ESP32 : ~1-2 min/jour → acceptable terrain
 * L'heure n'est PAS stockée en NVS (resync à chaque connexion)
 */

// Message WebSocket de sync (envoyé automatiquement par le navigateur) :
// { "cmd": "sync_time", "timestamp": 1718000000 }

// Côté ESP32 :
// struct timeval tv = { .tv_sec = timestamp };
// settimeofday(&tv, NULL);
```

### Format messages WebSocket

```json
// ESP32 → Navigateur (toutes les 500ms)
{
  "etat": "EN_COURS",
  "etat_code": 3,
  "prog_nom": "Maïs",
  "longueur_deroulee_m": 330.0,
  "longueur_enroulee_m": 127.5,
  "vitesse_m_h": 54.2,
  "duree_s": 4980,
  "heure_arrivee_unix": 1718012100,
  "heure_arrivee_relative_s": 6300,
  "heure_synchro": true,
  "surface_m2": 2295.0,
  "dose_inst_mm": 27.4,
  "debit_m3h": 24.3,
  "p_mano_bar": 7.0,
  "p_canon_bar": 3.4,
  "etage_courant": 2,
  "nb_etages": 4,
  "ev1": true,
  "ev2": false,
  "pression_ok": true,
  "raison_arret": "",
  "heure_synchro": true,
  "cfg_valide": true
}

// Navigateur → ESP32 (commandes)
{ "cmd": "sync_time", "timestamp": 1718000000 }
{ "cmd": "start" }
{ "cmd": "stop" }
{ "cmd": "reset" }
{ "cmd": "set_prog", "idx": 2 }
{ "cmd": "save_prog", "idx": 2, "nom": "Maïs", "dose": 25.0,
  "largeur": 18.0, "buse": 18, "pression": 7.0,
  "tdep_on": true, "tdep_s": 120, "tarr_on": false, "tarr_s": 0 }
{ "cmd": "save_machine", "longueur_tuyau": 330.0, "r_tambour": 0.25,
  "d_tuyau_ext": 0.09, "alpha_poumon": 36.0, "duree_rempl_s": 2.5,
  "denivele_m": 0.0 }
```

---

## 11. Interface Web (Web UI)

### Contraintes impératives
- **100% embarqué** dans la flash ESP32 — zéro ressource externe (pas de CDN, pas de fonts Google)
- **Mobile 6" portrait** — viewport ~390px, touch-friendly
- **Dark theme** — lisibilité en plein soleil
- **Boutons minimum 48px** de hauteur, pleine largeur
- **Chiffres clés ≥ 48px** (surface, vitesse, dose)
- **Pas de scroll horizontal**
- **HTML + CSS + JS dans un seul fichier** (webui.h en tableau C)

### Structure — 3 onglets (navigation bas de page)

#### Onglet 1 — 🌊 Accueil (vue opérateur terrain)
```
[BADGE ÉTAT — EN COURS ●]        ← couleur selon état
[Prog actif : Maïs]

[Déroulé]   [Enroulé]
[330 m  ]   [127 m  ]            ← 48px+

[Vitesse]   [Durée  ]
[54 m/h ]   [01:23  ]            ← 48px+

[Arrivée canon : 14h35]          ← heure absolue si synchro
[         ou +1h23    ]          ← relative si non synchro

[EV1 ● OUVERTE  ]               ← vert/rouge
[EV2 ● FERMÉE   ]
[Pression ✅ OK  ]

[▶ DÉMARRER          ]           ← bouton pleine largeur vert
[⏹ ARRÊT D'URGENCE  ]           ← bouton pleine largeur rouge
```

#### Onglet 2 — 📊 Stats (bilan session)
```
[Surface arrosée : 2 295 m²]
[Dose instantanée : 27.4 mm]
[Débit calculé : 24.3 m³/h ]
[P. mano : 7.0 bar | P. canon : 3.4 bar]
[Étage spires : 2 / 4       ]
[Volume eau : 38.1 m³        ]
```

#### Onglet 3 — ⚙️ Config

**Sélecteur programme :**
```
[Dropdown : 🌽 Maïs ▼]
[✏️ Renommer]
```

**Paramètres du programme sélectionné :**
```
Dose cible        [ 25 ] mm
Largeur position  [ 18.0 ] m
Diamètre buse     [ 18 ] mm  (12 à 26)
Pression mano     [ 7.0 ] bar
Tempo départ      [✓] ON  [ 120 ] s
Tempo arrivée     [ ] OFF [ --- ] s

[💾 ENREGISTRER PROGRAMME]
```

**Paramètres machine (section dépliable) :**
```
▶ Paramètres installation
Longueur tuyau    [ 330 ] m
Ø intérieur tuyau [ 82 ] mm
Rayon tambour nu  [ --- ] m  ← warning si 0
Ø ext. tuyau      [ --- ] m  ← warning si 0
Angle/cycle poumon [ --- ] ° ← warning si 0
Durée remplissage  [ --- ] s ← warning si 0
Dénivelé          [ 0.0 ] m  (+ montée / - descente)

[💾 ENREGISTRER PARAMÈTRES]
```

---

## 12. Mode test IRRITESTEUR (inspiré de l'original Irrifrance)

Accessible depuis la web UI (bouton discret en bas de l'onglet Config).
Permet de tester chaque entrée/sortie indépendamment **sans lancer un cycle complet**.

```
SORTIES (test manuel) :
  [EV1 ON]  [EV1 OFF]
  [EV2 ON]  [EV2 OFF]

ENTRÉES (lecture temps réel) :
  Capteur vitesse    : [0000] impulsions / [0.0] m/h
  Fin de course      : [OUVERT / FERMÉ]
  Sécurité spires    : [OUVERT / FERMÉ]
  Contact poumon     : [OUVERT / FERMÉ]
  Pressostat         : [OUVERT / FERMÉ]
```

> ⚠️ Le mode IRRITESTEUR désactive la machine d'états. Quitter ce mode remet l'état à VEILLE avec EV1=OFF, EV2=OFF.

---

## 13. Tests unitaires

Chaque module dispose de tests dans `main/test/`.
Les tests sont activés via `CONFIG_IRRI_ENABLE_TESTS=y` dans sdkconfig.
En mode test, `app_main()` exécute les tests puis démarre normalement.

### Tests obligatoires par PR

| PR | Tests |
|---|---|
| PR-02 | GPIO : lecture niveaux, détection front, comptage ISR |
| PR-03 | Hydraulique : table abaque (valeurs connues), interpolation, P_canon |
| PR-04 | Mécanique : rayon par étage, détermination étage, dist/pulse, fréquence poumon |
| PR-05 | Machine d'états : toutes les transitions, fail-safe, timeout poumon |
| PR-06 | NVS : écriture/lecture/reset 5 programmes, valeurs limites |
| PR-07 | WebSocket : connexion, sync heure, commandes |
| PR-09 | Intégration : simulation cycle complet avec capteurs mockés |

---

## 14. Références et documentation

| Source | Usage |
|---|---|
| UASA46, "Choisir le bon diamètre de buse", 19/06/2025 | Table abaque débit + pertes de charge |
| Manuel Irridoseur 4 (Irrifrance, réf. FAE-N21471 4/06) | Logique de fonctionnement, schéma câblage, bornes |
| ESP-IDF Programming Guide v5.x | API FreeRTOS, NVS, WiFi, HTTP Server |
| Datasheet ESP32 | GPIO, ISR, horloge interne |

---

## 15. Portabilité et réutilisation

Ce projet est conçu pour être réutilisé sur d'autres enrouleurs Irrifrance ou similaires.
Les points d'adaptation sont :

1. **Nombre de pastilles** : constante `NB_PASTILLES` dans `calculs_mecanique.h`
2. **Géométrie bobine** : `r_tambour_vide`, `d_tuyau_ext`, `nb_etages` en NVS
3. **Mécanique poumon** : `alpha_poumon`, `duree_remplissage_s` en NVS
4. **Longueur et diamètre tuyau** : en NVS
5. **Pertes de charge enrouleur** : `perte_enrouleur_bar` en NVS (défaut 2.5 bar)
6. **Dénivelé** : `denivele_m` en NVS (défaut 0.0 = terrain plat)
7. **Position manomètre** : documentée dans README, constante `MANOMETRE_POSITION`

---

## 16. GitHub — conventions

### Branches
```
main          ← stable, protégée
develop       ← intégration continue
feature/PR-XX ← une branche par PR
```

### Format commits
```
feat: description courte en français
fix: correction bug X
test: ajout tests module Y
docs: mise à jour README
refactor: restructuration module Z
```

### Template Pull Request
```markdown
## Description
[Ce que fait cette PR]

## Tests effectués
- [ ] Tests unitaires passés
- [ ] Mode IRRITESTEUR vérifié
- [ ] Pas de régression

## Captures / logs
[Si applicable]

## Checklist
- [ ] Commentaires en français
- [ ] Pas de valeurs hardcodées
- [ ] Fail-safe EV vérifié
```

---

*SPECS.md — Irrifrance ESP32 v1.0 — Généré depuis conversation Claude*
*Installation de référence : Structure 1 bis, tuyau PE 82mm / 330m, terrain plat*
