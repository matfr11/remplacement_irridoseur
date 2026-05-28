# SPECS_FINAL v3 — Irrifrance ESP32
## Remplacement régulation Irridoseur 3 — Document de référence unique

> Ce document remplace et consolide tous les documents précédents.
> L'utilisateur apportera ses propres modifications à partir de ce document.

---

## 🤖 Instructions pour Claude Code

Tu es un programmeur embarqué expérimenté, expert en :
- **ESP-IDF v5.x** et FreeRTOS
- Développement C bas niveau pour microcontrôleurs
- Protocoles réseau embarqués (HTTP, WebSocket)
- Systèmes de régulation industriels et hydraulique agricole
- Git, GitHub, workflows de développement professionnel

### Règles de travail

1. **Qualité avant vitesse** — chaque fichier propre, commenté, maintenable
2. **Tests à chaque étape** — test unitaire ou mode IRRITESTEUR par module
3. **Un commit par fonctionnalité** — format `feat:`, `fix:`, `test:`, `docs:`, `refactor:`
4. **Une Pull Request par étape** — description, tests, captures si applicable
5. **Défensif par défaut** — cas limites, valeurs hors plage, déconnexions
6. **Sécurité machine — FAIL SAFE** — doute sur état → EV_CANON=OFF, EV_POUMON=OFF immédiat
7. **Code portable** — paramètres spécifiques en NVS ou profil machine, jamais hardcodés
8. **Commentaires en français** — projet communauté agricole francophone GitHub

### Plan de développement — 10 PRs

```
PR-01 : Structure projet + CMakeLists + sdkconfig.defaults + partitions.csv
PR-02 : GPIO handler + ISR vitesse robuste + mode IRRITESTEUR
PR-03 : Calculs hydrauliques + table constructeur + double interpolation
PR-04 : Calculs mécaniques + profils machine + auto-calibration + étalonnage
PR-05 : Watchdog sécurités + machine d'états complète + tests simulation
PR-06 : Régulation feedforward + modes dégradés + tests
PR-07 : NVS config + 5 programmes + profils + abaques
PR-08 : WiFi AP + WebSocket + sync heure + OTA
PR-09 : Web UI mobile embarquée — 3 onglets complets
PR-10 : Intégration complète + README GitHub + documentation
```

---

## 1. Contexte du projet

### Machine de référence
- **Enrouleur** : Irrifrance Structure 1 bis (ST.1 Bis)
- **Régulation remplacée** : Irridoseur 3 (en panne)
- **Canon** : SR 150C
- **Tuyau** : PE Ø82mm extérieur / épaisseur 6mm / 330m

### Principe mécanique de l'enroulement — TPI (poumon)

```
Cycle poumon :

  Phase 1 — REMPLISSAGE (bobine tourne)
    EV_POUMON = ON
    Le poumon se remplit et POUSSE le cliquet
    → La bobine avance d'un angle fixe
    → Le capteur vitesse compte les impulsions ICI
    Fin : contact poumon plein détecté (ou temporisé en mode dégradé)

  Phase 2 — VIDANGE (bobine immobile)
    EV_POUMON = OFF
    Le ressort rappelle le poumon en position initiale
    Le cliquet revient — bobine NE TOURNE PAS
    Fin : durée fixe t_vidange_s (mécanique — paramètre NVS)

  Phase 3 — ATTENTE RÉGULATION
    EV_POUMON = OFF
    Pause calculée par feedforward
    T_attente = 0 → vitesse maximale
    T_attente > 0 → ralentissement
    → Retour Phase 1
```

### Alimentation
- Batterie 12V / 24Ah + panneau solaire + régulateur de charge
- Fonctionnement **permanent** toute la saison
- WiFi : point d'accès généré par l'ESP32 (pas de réseau externe)

---

## 2. Matériel

### Carte principale
**ESP32 Quad MOS Switch Module** (AliExpress)
- ESP32-32E intégré
- 4 canaux MOSFET DC 5-60V (cycles illimités)
- Régulateur buck intégré → alimentation directe 12V batterie
- USB-C pour programmation
- Tous les GPIO accessibles sur rangées de pins

> ⚠️ **Action avant PR-02** : identifier les GPIO utilisés par les
> 4 canaux MOSFET depuis le schéma technique de la carte.
> Mettre à jour PIN_EV_CANON et PIN_EV_POUMON dans gpio_handler.h.

### Boîtier et connectique
| Composant | Détail |
|---|---|
| Boîtier IP65 | ~200×150×80mm |
| Bornier DIN rail 12 voies | Connexions terrain |
| Rail DIN 15cm | Fixé dans boîtier |
| Résistances 10kΩ × 4 | Pull-up contacts — soudées sur fils + gaine thermo |
| Diviseur 10kΩ + 3.3kΩ | Capteur vitesse — soudé sur fil + gaine thermo |

---

## 3. GPIO et câblage

### Affectation GPIO

| GPIO | Direction | Signal | Conditionnement |
|---|---|---|---|
| **34** | INPUT | Capteur vitesse bobine | Diviseur 10kΩ/3.3kΩ — 12V→3V — pas de pull-up interne |
| **35** | INPUT | Fin de course canon | Pull-up 10kΩ — contact NC |
| **32** | INPUT | Sécurité spires | Pull-up 10kΩ — contact NC |
| **33** | INPUT | Contact poumon plein | Pull-up 10kΩ — contact NC |
| **27** | INPUT | Pressostat | Pull-up 10kΩ — contact NC |
| **A** | OUTPUT | EV_CANON 12V | GPIO à identifier carte Quad MOS |
| **B** | OUTPUT | EV_POUMON 12V | GPIO à identifier carte Quad MOS |

### Logique des signaux — CONTACTS NC (Normalement Fermés)

> ⚠️ Règle fail-safe fondamentale :
> Fil coupé ou capteur HS → GPIO HIGH → sécurité déclenchée.
> Ne jamais utiliser de contacts NO pour les sécurités.

| Signal | Repos LOW | Activé HIGH | Fil coupé |
|---|---|---|---|
| Fin de course | Canon déroulé ✅ | Canon rentré → SEC-1 | → SEC-1 ✅ |
| Sécurité spires | Normal ✅ | Débordement → SEC-2 | → SEC-2 ✅ |
| Contact poumon | En cours ✅ | Poumon plein | → Timeout → dégradé |
| Pressostat | Pression OK ✅ | Pas de pression | → Pause/attente |

### Schéma câblage résistances sur fils

```
CAPTEUR VITESSE (diviseur 12V → 3V)
Borne 3 ──[10kΩ]──────────── GPIO 34
                └──[3.3kΩ]── GND

CONTACTS NC (pull-up — actif haut)
  3.3V carte
     │
   [10kΩ]
     │
Borne x ──────────────────── GPIO xx
     │
 [Contact NC]
     │
    GND
```

### Bornier 12 voies

| Borne | Signal | Vers |
|---|---|---|
| 1 | 12V | Bornier alim carte |
| 2 | GND | GND carte |
| 3 | Capteur vitesse signal | GPIO 34 via diviseur |
| 4 | Fin de course canon | GPIO 35 via pull-up NC |
| 5 | Sécurité spires | GPIO 32 via pull-up NC |
| 6 | Contact poumon plein | GPIO 33 via pull-up NC |
| 7 | Pressostat | GPIO 27 via pull-up NC |
| 8 | GND capteurs | GND carte |
| 9 | EV_CANON + | OUT1+ carte |
| 10 | EV_CANON − | OUT1− carte |
| 11 | EV_POUMON + | OUT2+ carte |
| 12 | EV_POUMON − | OUT2− carte |

---

## 4. Profils machine

### Structure profil

```c
typedef struct {
    char    nom[32];               // Ex: "ST1 Bis Ø82-330m"
    char    constructeur[32];      // Ex: "Irrifrance"
    float   longueur_tuyau_m;      // Longueur tuyau PE (m)
    float   d_tuyau_ext_m;         // Diamètre extérieur tuyau (m)
    float   r_tambour_vide_m;      // Rayon tambour sans tuyau (m)
    int     nb_etages;             // Nombre de couches de spires
    int     abaque_idx;            // Index abaque associé

    // Double entrée — renseigner UN SEUL des deux
    // L'autre est calculé automatiquement au chargement
    float   largeur_bobine_m;      // Mesure terrain (mètre ruban)
    float   spires_par_etage;      // Valeur constructeur ou comptage

    // Paramètres ajustables terrain (peuvent être modifiés en NVS)
    float   t_vidange_s;           // ⚠️ À mesurer terrain
    float   facteur_correction;    // Étalonnage longueur (défaut 1.0)
} machine_profile_t;

// Logique double entrée spires/largeur :
// Si largeur_bobine_m > 0  → spires = largeur / d_tuyau_ext
// Si spires_par_etage > 0  → largeur = spires × d_tuyau_ext
// Les deux à 0             → profil invalide, erreur au chargement
```

### Profil installation référence

```c
// main/machines/st1bis_82_330.c
static const machine_profile_t MACHINE_ST1BIS_82_330 = {
    .nom               = "ST1 Bis Ø82-330m",
    .constructeur      = "Irrifrance",
    .longueur_tuyau_m  = 330.0f,
    .d_tuyau_ext_m     = 0.082f,
    .r_tambour_vide_m  = 0.690f,  // Calculé : 0.977 - (3.5 × 0.082)
    .nb_etages         = 4,
    .abaque_idx        = 0,       // SR 150C
    .largeur_bobine_m  = 0.0f,    // ⚠️ À mesurer terrain (mètre ruban)
    .spires_par_etage  = 13.45f,  // Source tableau constructeur ST.1 Bis
    .t_vidange_s       = 0.0f,    // ⚠️ À mesurer terrain
    .facteur_correction = 1.0f,
};
// Note : largeur_bobine calculée auto → 13.45 × 0.082 = 1.103m
```

### Structure projet — fichiers machines et abaques

```
main/
├── machines/
│   ├── machines.h              ← liste + sélection profil actif
│   └── st1bis_82_330.c         ← profil référence
│   └── [autre_machine].c       ← contributions futures
└── abaques/
    ├── abaques.h               ← liste + sélection abaque actif
    └── sr150c.c                ← abaque référence
    └── [autre_canon].c         ← contributions futures
```

---

## 5. Abaques — Table débit constructeur

### Structure abaque

```c
typedef struct {
    float p_enrouleur;  // Pression manomètre enrouleur (bar)
    float q_m3h;        // Débit (m³/h)
    float p_buse;       // Pression effective à la buse (bar)
    float buse_mm;      // Diamètre buse (mm)
    float esp_m;        // Espacement recommandé (m)
    float D40;          // Vitesse (m/h) pour dose 40mm
    float D30;          // Vitesse (m/h) pour dose 30mm
    float D25;          // Vitesse (m/h) pour dose 25mm
    float D20;          // Vitesse (m/h) pour dose 20mm
    float D15;          // Vitesse (m/h) pour dose 15mm
} canon_entry_t;

typedef struct {
    char              nom[32];         // Ex: "SR 150C"
    char              constructeur[32];
    canon_entry_t     table[20];       // Max 20 lignes par abaque
    int               nb_entrees;
} canon_abaque_t;
```

### Abaque SR 150C (installation référence)

```c
// main/abaques/sr150c.c
static const canon_abaque_t ABAQUE_SR150C = {
    .nom          = "SR 150C",
    .constructeur = "Irrifrance",
    .nb_entrees   = 13,
    .table = {
     // p_enrouleur  Q      p_buse  buse   esp    D40    D30    D25    D20    D15
        {4.9f,  23.0f,  3.5f, 17.3f, 60.f,  9.6f, 12.3f, 15.3f, 19.2f, 25.6f},
        {5.6f,  24.6f,  4.0f, 17.3f, 63.f,  9.8f, 13.0f, 15.6f, 19.5f, 26.0f},
        {5.7f,  29.8f,  3.5f, 20.3f, 63.f, 11.8f, 15.8f, 18.9f, 23.7f, 31.5f},
        {6.5f,  31.9f,  4.0f, 20.3f, 66.f, 12.1f, 16.1f, 19.3f, 24.2f, 32.2f},
        {6.8f,  37.8f,  3.5f, 22.9f, 66.f, 14.3f, 19.1f, 22.9f, 28.6f, 38.2f},
        {6.9f,  27.5f,  5.0f, 17.8f, 66.f, 10.4f, 13.9f, 16.7f, 20.8f, 27.8f},
        {7.7f,  40.4f,  4.0f, 22.9f, 72.f, 14.0f, 18.7f, 22.4f, 28.1f, 37.4f},
        {8.0f,  35.7f,  5.0f, 20.3f, 72.f, 12.4f, 16.5f, 19.8f, 24.8f, 33.1f},
        {8.2f,  30.1f,  6.0f, 17.8f, 66.f, 11.4f, 15.2f, 18.2f, 22.8f, 30.4f},
        {8.4f,  46.9f,  3.5f, 25.4f, 72.f, 16.3f, 21.7f, 26.1f, 32.6f, 43.4f},
        {9.4f,  50.1f,  4.0f, 25.4f, 78.f, 16.1f, 21.4f, 25.7f, 32.1f, 42.8f},
        {9.5f,  39.1f,  6.0f, 20.3f, 72.f, 13.6f, 18.1f, 21.7f, 27.2f, 36.2f},
        {9.5f,  45.2f,  5.0f, 22.9f, 78.f, 14.5f, 19.3f, 23.2f, 29.0f, 38.6f},
    },
};
```

### Conversion pouces → mm
```
0.60" = 15.2mm / 0.70" = 17.8mm / 0.80" = 20.3mm
0.90" = 22.9mm / 1.00" = 25.4mm
```

### Fonction lookup — double interpolation

```c
/**
 * Lookup vitesse cible — double interpolation.
 *
 * Niveau 1 — Ligne (p_enrouleur + buse_mm) :
 *   2 lignes les plus proches → interpolation linéaire
 *
 * Niveau 2 — Dose (dose_mm) :
 *   Interpolation linéaire entre colonnes D15/D20/D25/D30/D40
 *   Extrapolation pour doses 10-15mm et 40-50mm
 *   Plage valide : 10 à 50mm
 *   Hors plage → log warning + valeur extrapolée
 *   Note : vitesse DIMINUE quand dose AUGMENTE
 *
 * @param abaque       Pointeur abaque actif
 * @param p_enrouleur  Pression manomètre (bar)
 * @param buse_mm      Diamètre buse (mm)
 * @param dose_mm      Dose cible (mm) — plage 10 à 50mm
 * @param debit_out    Débit résultant (m³/h)
 * @return             Vitesse cible (m/h)
 */
float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float *debit_out);
```

---

## 6. Calculs mécaniques

### Modèle géométrique étages

```c
/**
 * Rayon effectif à l'étage n (numérotation depuis 1).
 * R_n = r_tambour_vide + (n - 0.5) × d_tuyau_ext
 *
 * Valeurs installation référence :
 *   r_tambour_vide = 0.690m, d_tuyau_ext = 0.082m
 *   Étage 1 : R = 0.690 + 0.041 = 0.731 m
 *   Étage 2 : R = 0.690 + 0.123 = 0.813 m
 *   Étage 3 : R = 0.690 + 0.205 = 0.895 m
 *   Étage 4 : R = 0.690 + 0.287 = 0.977 m ✅ (vérifié)
 */
float calcul_rayon_etage(int n, const machine_profile_t *profil);

/**
 * Distance par impulsion capteur à l'étage n.
 * dist_pulse_m = (2π × R_n) / NB_PASTILLES
 */
float calcul_dist_pulse_m(float r_etage_m);

/**
 * Détermination étage courant depuis longueur enroulée.
 * Utilise spires_par_etage du profil machine.
 * longueur_etage_n = spires_par_etage × 2π × R_n
 * Cumul jusqu'à longueur_enroulee → étage courant
 */
int calcul_etage_courant(float longueur_enroulee_m,
                          const machine_profile_t *profil);
```

### Capteur vitesse — mesure robuste multi-cycles

```c
/**
 * Contexte : plusieurs cycles poumon par impulsion capteur.
 * La bobine avance de plusieurs cycles avant qu'une pastille passe.
 *
 * ISR robuste :
 *   Anti-rebond temporel  : ignorer tout front < 50ms (mouvement lent)
 *   Directionnel          : RISING uniquement (recul mécanique ignoré)
 *   Reset compteur cycles : à chaque impulsion
 *
 * Vitesse — fenêtre glissante N impulsions :
 *   vitesse_m_h = (N × dist_pulse_m × facteur_correction)
 *                / temps_N_pulses_s × 3600
 *   N = fenetre_vitesse (NVS, défaut 5)
 *
 * Vitesse nulle :
 *   Si cycles_sans_impulsion > MAX_CYCLES_SANS_IMPULSION (NVS, défaut 15)
 *   → vitesse = 0
 */
#define NB_PASTILLES                  10
#define DEBOUNCE_VITESSE_MS           50
```

### Étalonnage longueur

```c
/**
 * L'opérateur corrige la longueur affichée depuis la web UI
 * en saisissant la valeur réelle marquée sur le tuyau.
 *
 * facteur_correction = longueur_reelle / longueur_theorique
 * Appliqué à toutes les distances calculées.
 *
 * Validation avant application :
 *   C1 : nb_impulsions_session > 50 (capteur a fonctionné)
 *   C2 : 0.5 < facteur < 2.0
 *   C3 : écart < 30%
 *
 * Refus avec message explicite si validation échoue.
 * Stocké dans profil NVS — persistant.
 * Reset depuis Config → facteur = 1.0
 */
```

### Chaîne de calcul complète

```
p_enrouleur + buse_mm + dose_mm
    │
    ▼
lookup_vitesse_cible(abaque_actif)
    → vitesse_cible_m_h
    → debit_m3h
    │
    ▼
vitesse_cible_m_s = vitesse_cible_m_h / 3600
    │
    ▼
dist_par_cycle_m (auto-calibrée, moyenne 5 cycles)
    │
    ▼
cycles_par_min   = (vitesse_cible_m_h / 60) / dist_par_cycle_m
T_cycle_cible_s  = 60 / cycles_par_min
    │
    ▼
T_attente_s = T_cycle_cible_s - t_remplissage_moyen_s - t_vidange_s
```

---

## 7. Régulation poumon — Feedforward + correction

### Calcul T_attente

```c
/**
 * T_attente_s = T_cycle_cible_s - t_remplissage_mesure_s - t_vidange_s
 *
 * Compensation automatique pression :
 *   Pression haute → t_remplissage court → T_attente augmente
 *   Pression basse → t_remplissage long  → T_attente diminue
 *
 * Guards :
 *   T_attente < 0    → T_attente = 0
 *                    → alerte "⚠️ Pression insuffisante pour la dose"
 *   T_attente > 300s → log warning anomalie
 */
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s);

/**
 * Correction proportionnelle depuis vitesse réelle.
 * Activée après n_cycles_calib cycles (NVS, défaut 3).
 * Kp (NVS, défaut 0.1)
 */
float correction_vitesse(float t_attente_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp);
```

### Gestion timeout remplissage — 2 tentatives

```
Tentative 1 :
  EV_POUMON = ON
  ├── Contact poumon plein avant 20s → succès ✅
  └── 20s sans contact :
        EV_POUMON = OFF, attente t_vidange_s
        Web UI : "⚠️ Remplissage poumon — tentative (1/2)"

Tentative 2 :
  EV_POUMON = ON
  ├── Contact poumon plein avant 20s → succès ✅
  └── 20s sans contact :
        EV_POUMON = OFF, EV_CANON = OFF
        → ARRET_URGENCE
          "Remplissage poumon échoué (2/2)
           Vérifier EV_POUMON et circuit hydraulique"
```

---

## 8. Modes dégradés

> **Activation manuelle uniquement** depuis Config web UI.
> Pas de basculement automatique.
> Défaut détecté → alerte → l'opérateur décide.

### Mode dégradé A — Capteur vitesse HS

```
Activation : Config → [✓] Mode dégradé capteur vitesse

Fonctionnement :
  Vitesse estimée depuis cycles poumon :
    vitesse_theorique = dist_par_cycle_nvs × cycles_par_min × 60
  dist_par_cycle_nvs = dernière valeur connue (NVS) ou théorique
  Correction Kp désactivée
  Étalonnage longueur impossible

Web UI : "⚠️ MODE DÉGRADÉ — Vitesse estimée (capteur HS)"
Sécurités actives : SEC-1 ✅  SEC-2 ✅
```

### Mode dégradé B — Contact poumon plein HS

```
Activation : Config → [✓] Mode dégradé contact poumon
             Saisir t_remplissage_fixe_s (à mesurer terrain)

Fonctionnement :
  EV_POUMON = ON pendant t_remplissage_fixe_s
  Pas de détection fin de remplissage
  Vidange normale

Web UI : "⚠️ MODE DÉGRADÉ — Remplissage temporisé : X.Xs"
Sécurités actives : SEC-1 ✅  SEC-2 ✅
```

### Mode dégradé A + B combinés

```
Web UI : "⚠️⚠️ MODE TRÈS DÉGRADÉ — Précision non garantie"
Sécurités actives : SEC-1 ✅  SEC-2 ✅
```

---

## 9. Watchdog sécurités

**Exécuté EN PREMIER dans state_machine_task — priorité absolue.**

```c
void securites_watchdog(void) {

    // SEC-2 : Débordement spires — TOUT état sans exception
    if (gpio_get_level(PIN_SECU_SPIRES)) {  // HIGH = danger (NC)
        gpio_set_level(PIN_EV_CANON,  0);
        gpio_set_level(PIN_EV_POUMON, 0);
        declencher_arret_urgence("Débordement bobine");
        return;
    }

    // SEC-1 : Fin de course en cours de cycle
    // Non applicable pendant TEMPO_ARRIVEE (fin de course normal)
    bool sec1_applicable = (etat == ETAT_OUVERTURE_CANON    ||
                            etat == ETAT_TEMPO_DEPART       ||
                            etat == ETAT_REMPLISSAGE_POUMON ||
                            etat == ETAT_EN_COURS           ||
                            etat == ETAT_PAUSE_PRESSION);

    if (sec1_applicable && gpio_get_level(PIN_FIN_COURSE)) {
        gpio_set_level(PIN_EV_CANON,  0);
        gpio_set_level(PIN_EV_POUMON, 0);
        declencher_arret_urgence("Fin de course en cours de cycle");
        return;
    }
    // SEC-P pression : gérée dans tick_state_machine()
}
```

### Tableau sécurités par état

| Sécurité | VEILLE | OUVERT_CAN | TEMPO_DEP | REMPL_POU | EN_COURS | PAUSE_P | TEMPO_ARR | ARRET_* |
|---|---|---|---|---|---|---|---|---|
| SEC-2 spires | URG | URG | URG | URG | URG | URG | URG | ignoré |
| SEC-1 fin course | C2 VEILLE | URG | URG | URG | URG | URG | **ignoré** ✅ | ignoré |
| SEC-P pression | pas démarrage | PAUSE | PAUSE | PAUSE | PAUSE | — | ARRET_FINAL | ignoré |

---

## 10. Machine d'états

### États

```c
typedef enum {
    ETAT_VEILLE,              // Attente conditions démarrage
    ETAT_OUVERTURE_CANON,     // EV_CANON=ON, attend pression stable 3s
    ETAT_TEMPO_DEPART,        // EV_CANON=ON, arrosage sur place avant enroulement
    ETAT_REMPLISSAGE_POUMON,  // EV_POUMON=ON, attend contact poumon plein
    ETAT_EN_COURS,            // EV_CANON=ON, régulation cycles poumon active
    ETAT_PAUSE_PRESSION,      // EV_CANON=OFF EV_POUMON=OFF, pression perdue
    ETAT_TEMPO_ARRIVEE,       // EV_CANON=ON EV_POUMON=OFF, arrosage final
    ETAT_ARRET_FINAL,         // EV_CANON=OFF EV_POUMON=OFF, bilan session
    ETAT_ARRET_URGENCE,       // EV_CANON=OFF EV_POUMON=OFF, incident matériel
} etat_machine_t;
```

### Détail états

#### ETAT_VEILLE
```
EV_CANON = OFF, EV_POUMON = OFF
Boot ESP32 → toujours cet état

Conditions démarrage simultanées :
  C1 : Pressostat actif     (LOW = pression présente)
  C2 : Fin de course inactif (LOW = canon déroulé)
  C3 : Programme valide     (dose, largeur, buse, pression > 0)
  C4 : Pas en PAUSE_PRESSION (sinon reprendre l'état sauvegardé)

Alertes web UI :
  C1 KO → "En attente de pression"
  C2 KO → "⚠️ Canon non déroulé — vérifier position"
  C3 KO → "Configurer un programme avant de démarrer"

Transition automatique C1+C2+C3 → ETAT_OUVERTURE_CANON
```

#### ETAT_OUVERTURE_CANON
```
EV_CANON = ON
EV_POUMON = OFF
Compteur stabilité pression (ticks 100ms)
Web UI : "⏳ Mise en pression canon..."

Logique :
  Pressostat actif (LOW) → compteur++, sinon compteur = 0
  Compteur >= 30 (3s stables) → transition

Transition :
  Pression stable 3s :
    tempo_depart_on → ETAT_TEMPO_DEPART
    Sinon           → ETAT_REMPLISSAGE_POUMON
  Pas de pression → attente indéfinie
    Web UI : "⏳ En attente de pression..."
```

#### ETAT_TEMPO_DEPART
```
EV_CANON = ON  (canon arrose sur place avant enroulement)
EV_POUMON = OFF
Décompte timer tempo_depart_s
Web UI : "⏱ Arrosage départ — encore MM:SS"

Transitions :
  Timer écoulé    → ETAT_REMPLISSAGE_POUMON
  Pression perdue → ETAT_PAUSE_PRESSION (timer suspendu)
  SEC-1 ou SEC-2  → ETAT_ARRET_URGENCE
```

#### ETAT_REMPLISSAGE_POUMON
```
EV_POUMON = ON
EV_CANON = OFF si premier remplissage
EV_CANON = ON  si re-remplissage en cours de cycle
Logique 2 tentatives (voir section 7)

Transitions :
  Contact poumon plein (HIGH) → EV_POUMON=OFF → ETAT_EN_COURS
  2 tentatives échouées       → ETAT_ARRET_URGENCE
  Pression perdue             → ETAT_PAUSE_PRESSION
  SEC-1 ou SEC-2              → ETAT_ARRET_URGENCE
```

#### ETAT_EN_COURS
```
EV_CANON = ON
EV_POUMON = piloté en cycles

Sous-états cycle poumon :
  SOUS_REMPLISSAGE : EV_POUMON=ON, chrono t_remplissage
                     Capteur vitesse compte ici
  SOUS_VIDANGE     : EV_POUMON=OFF, chrono t_vidange_s
                     Bobine immobile
  SOUS_ATTENTE     : EV_POUMON=OFF, attente T_attente

Mesures continues :
  ISR → fenêtre N → vitesse réelle × facteur_correction
  dist_par_cycle auto-calibrée (moyenne 5 cycles)
  T_attente feedforward recalculé chaque cycle
  Correction Kp après n_cycles_calib
  longueur_enroulee, étage courant, surface, dose, heure arrivée

Transitions :
  SEC-2 débordement         → ETAT_ARRET_URGENCE
  SEC-1 fin de course       → ETAT_ARRET_URGENCE
  Pression perdue           → ETAT_PAUSE_PRESSION
  Arrêt manuel opérateur    → ETAT_ARRET_FINAL
  Fin de course détecté :
    tempo_arrivee_on ET s>0 → ETAT_TEMPO_ARRIVEE
    Sinon                   → ETAT_ARRET_FINAL
```

#### ETAT_PAUSE_PRESSION
```
EV_CANON = OFF, EV_POUMON = OFF
Sauvegarde état précédent + timer suspendu si TEMPO_DEPART
Chronomètre durée arrêt
Web UI : "⚠️ Pression perdue — Canon arrêté depuis MM:SS"

Transitions :
  Pression revenue (LOW stable 3s) :
    → reprend état sauvegardé
    → Web UI : "✅ Pression revenue — Arrêt de MM:SS"
    → alerte conservée jusqu'au prochain reset
    → recalcul heure arrivée
  SEC-2 → ETAT_ARRET_URGENCE
```

#### ETAT_TEMPO_ARRIVEE
```
EV_CANON = ON  (arrosage final sur place)
EV_POUMON = OFF
Fin de course ACTIF ici → NORMAL — SEC-1 ignorée ✅
Décompte timer tempo_arrivee_s
Web UI : "⏱ Arrosage arrivée — encore MM:SS"

Transitions :
  Timer écoulé    → EV_CANON=OFF → ETAT_ARRET_FINAL
  Pression perdue → EV_CANON=OFF → ETAT_ARRET_FINAL
    Web UI : "Cycle terminé — eau coupée"
  SEC-2           → ETAT_ARRET_URGENCE
```

#### ETAT_ARRET_FINAL
```
EV_CANON = OFF, EV_POUMON = OFF
Bilan session :
  Longueur enroulée (m), Surface (m²), Dose moyenne (mm)
  Durée (hh:mm), Volume eau (m³), Nb cycles poumon
  Temps arrêt pression cumulé, Facteur correction appliqué

Transition :
  Reset web UI → remise à zéro compteurs → ETAT_VEILLE
```

#### ETAT_ARRET_URGENCE
```
EV_CANON = OFF, EV_POUMON = OFF (GPIO direct — pas de fonction)
Raison stockée NVS (survit au redémarrage)
Web UI : alerte rouge + raison + durée

Causes :
  "Débordement bobine — vérifier enroulement"
  "Fin de course déclenché en cours de cycle"
  "Remplissage poumon échoué (2/2) — Vérifier EV_POUMON"

Transition :
  Reset manuel web UI → acquittement → ETAT_VEILLE
```

### Vue d'ensemble

```
VEILLE ──C1+C2+C3──→ OUVERTURE_CANON
                           │ pression stable 3s
                    ┌──────┴──────┐
               tempo_dep       pas tempo
                    ▼              ▼
              TEMPO_DEPART → REMPLISSAGE_POUMON
                                   │ poumon plein
                              EN_COURS
                                   │ fin de course
                    ┌──────────────┴──────────┐
               tempo_arr                   pas tempo
                    ▼                          ▼
              TEMPO_ARRIVEE ──────────→ ARRET_FINAL
                                             │ reset
                                           VEILLE

PAUSE_PRESSION ←── pression perdue (depuis TEMPO_DEP, REMPL_POU, EN_COURS)
      │ pression revenue
      └──────────────→ reprend état précédent

ARRET_URGENCE ←── SEC-1 ou SEC-2 (depuis tout état sauf TEMPO_ARR, ARRET_*)
      │ reset manuel
      └──────────────→ VEILLE
```

---

## 11. NVS — Paramètres

### Namespace `irri_machine`

| Clé | Type | Valeur | Note |
|---|---|---|---|
| `machine_active` | int | 0 | Index profil machine actif |
| `t_vidange_s` | float | 0.0 | ⚠️ À mesurer terrain |
| `facteur_correction` | float | 1.0 | Étalonnage longueur |
| `dist_cycle_nvs` | float | 0.0 | Dernière dist/cycle connue |
| `kp_regulation` | float | 0.1 | Gain correction vitesse |
| `n_cycles_calib` | int | 3 | Cycles avant correction |
| `fenetre_vitesse` | int | 5 | Nb impulsions fenêtre |
| `max_cycles_si` | int | 15 | Cycles sans impulsion → v=0 |
| `mode_deg_vitesse` | uint8 | 0 | Mode dégradé capteur |
| `mode_deg_poumon` | uint8 | 0 | Mode dégradé contact poumon |
| `t_rempl_fixe_s` | float | 0.0 | ⚠️ Durée remplissage fixe |
| `denivele_m` | float | 0.0 | Terrain plat — portabilité |

### Namespace `irri_prog` — 5 programmes

| Clé | Type | Défaut | Description |
|---|---|---|---|
| `p{n}_nom` | string | "Programme N" | 20 chars max |
| `p{n}_dose` | float | 25.0 | Dose cible (mm) — plage 10-50 |
| `p{n}_largeur` | float | 18.0 | Largeur position (m) |
| `p{n}_buse` | int | 18 | Diamètre buse (mm) |
| `p{n}_pression` | float | 6.0 | Pression enrouleur (bar) |
| `p{n}_tdep_on` | uint8 | 0 | Tempo départ activée |
| `p{n}_tdep_s` | int | 0 | Durée tempo départ (s) |
| `p{n}_tarr_on` | uint8 | 0 | Tempo arrivée activée |
| `p{n}_tarr_s` | int | 0 | Durée tempo arrivée (s) |

### Namespace `irri_state`

| Clé | Type | Description |
|---|---|---|
| `prog_actif` | int | Index programme actif (0-4) |
| `last_urgence` | string | Dernière raison arrêt urgence |

**Total : 12 + 45 + 2 = 59 clés NVS**

---

## 12. Architecture logicielle ESP-IDF v5.x

### Structure fichiers

```
irrifrance-esp32/
├── CMakeLists.txt
├── partitions.csv              ← app0, app1 (OTA), nvs
├── sdkconfig.defaults
├── README.md
├── SPECS_FINAL_v3.md
├── CHANGELOG.md
├── .github/
│   └── PULL_REQUEST_TEMPLATE.md
└── main/
    ├── CMakeLists.txt
    ├── main.c                   ← app_main, init, tâches
    ├── gpio_handler.c/h         ← GPIO, ISR vitesse robuste
    ├── securites.c/h            ← watchdog SEC-1, SEC-2
    ├── state_machine.c/h        ← machine d'états complète
    ├── regulation.c/h           ← feedforward, Kp, auto-calib
    ├── calculs_hydraulique.c/h  ← lookup, double interpolation
    ├── calculs_mecanique.c/h    ← étages, dist/pulse, étalonnage
    ├── config_nvs.c/h           ← NVS, programmes, modes dégradés
    ├── ota.c/h                  ← mise à jour OTA depuis web UI
    ├── webserver.c/h            ← HTTP + WebSocket esp_http_server
    ├── webui.h                  ← HTML/CSS/JS embarqué
    ├── telemetry.c/h            ← couche vide LoRa futur
    ├── machines/
    │   ├── machines.h           ← liste profils + sélection
    │   └── st1bis_82_330.c      ← profil référence
    ├── abaques/
    │   ├── abaques.h            ← liste abaques + sélection
    │   └── sr150c.c             ← abaque référence
    └── test/
        ├── test_calculs_hydraulique.c
        ├── test_calculs_mecanique.c
        ├── test_regulation.c
        └── test_state_machine.c
```

### FreeRTOS

```c
void app_main(void) {
    nvs_flash_init();
    gpio_handler_init();
    config_nvs_load();
    wifi_ap_init();
    webserver_init();
    ota_init();

    xTaskCreate(state_machine_task, "state_machine", 4096, NULL, 5, NULL);
    xTaskCreate(websocket_task,     "websocket",     8192, NULL, 3, NULL);
}

void state_machine_task(void *pv) {
    while (1) {
        securites_watchdog();   // TOUJOURS EN PREMIER
        tick_state_machine();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## 13. OTA — Mise à jour firmware

```
Partitions :
  app0  ← firmware actif
  app1  ← firmware OTA
  nvs   ← config préservée ✅

Condition :
  OTA impossible si cycle en cours
  → Message : "Arrêter le cycle avant la mise à jour"

Procédure web UI :
  Config → [🔄 Mise à jour firmware]
  [Sélectionner fichier .bin] ← input file navigateur mobile
  [📤 Mettre à jour]

Pendant la mise à jour :
  EV_CANON=OFF, EV_POUMON=OFF forcés
  Machine d'états suspendue
  Barre de progression XX%

Succès :
  "✅ Mise à jour réussie — Redémarrage dans 3s"
  → Reboot, NVS préservée ✅

Échec :
  "❌ Échec — Firmware précédent conservé"
  → Rollback automatique app0
  → Machine d'états reprend
```

---

## 14. WiFi et WebSocket

```
SSID : "Irrifrance-ESP32"  |  Password : "irrigation"
IP   : 192.168.4.1         |  URL : http://192.168.4.1

Sync heure :
  Navigateur → timestamp Unix à chaque connexion WS (automatique)
  Heure arrivée en HH:MM absolu si synchro / +HH:MM sinon
  Non stockée en NVS — resync à chaque connexion
```

### Message ESP32 → Navigateur (500ms)

```json
{
  "etat": "EN_COURS", "etat_code": 4,
  "prog_nom": "Maïs",
  "machine_nom": "ST1 Bis Ø82-330m",
  "abaque_nom": "SR 150C",
  "longueur_deroulee_m": 330.0,
  "longueur_enroulee_m": 127.5,
  "vitesse_m_h": 54.2, "vitesse_cible_m_h": 54.0,
  "duree_s": 4980,
  "heure_arrivee_unix": 1718012100,
  "heure_arrivee_relative_s": 6300,
  "heure_synchro": true,
  "surface_m2": 2295.0,
  "dose_inst_mm": 27.4, "debit_m3h": 24.3,
  "p_enrouleur_bar": 7.0, "p_buse_bar": 3.4,
  "etage_courant": 2, "nb_etages": 4,
  "ev_canon": true, "ev_poumon": false,
  "pression_ok": true,
  "t_remplissage_ms": 2340, "t_attente_ms": 8200,
  "dist_par_cycle_m": 0.438,
  "cycles_par_min_cible": 6.6,
  "cycles_par_min_reel": 6.4,
  "cycles_total": 291,
  "alerte_pression_insuff": false,
  "mode_degrade_vitesse": false,
  "mode_degrade_poumon": false,
  "facteur_correction": 1.0,
  "raison_arret": "",
  "cfg_valide": true
}
```

---

## 15. Interface Web (Web UI)

### Contraintes
- 100% embarqué — zéro ressource externe
- Mobile 6" portrait, ~390px
- Dark theme, boutons ≥ 48px, chiffres ≥ 48px
- HTML + CSS + JS dans webui.h

### Onglet 1 — 🌊 Accueil

```
[BADGE ÉTAT — couleur selon état]
[Prog : Maïs | Machine : ST1 Bis | Canon : SR 150C]

[Déroulé ✏️]  [Enroulé  ]
[330 m     ]  [127 m    ]  ← ✏️ cliquable → étalonnage

[Vitesse   ]  [Durée    ]
[54 m/h    ]  [01:23    ]

[Arrivée : 14h35 / +1h23]
[⚠️ Alertes modes dégradés]
[⚠️ Pression insuffisante si T_attente=0]

[EV_CANON  ● OUVERTE ]
[EV_POUMON ● FERMÉE  ]
[Pression  ✅ OK      ]

[▶ DÉMARRER          ]  ← vert
[⏹ ARRÊT URGENCE    ]  ← rouge
[↺ RESET             ]  ← orange si ARRET_*
```

### Onglet 2 — 📊 Stats

```
Surface arrosée       : 2 295 m²
Dose instantanée      : 27.4 mm
Débit calculé         : 24.3 m³/h
P. enrouleur / buse   : 7.0 / 3.4 bar
Étage spires          : 2 / 4
Volume eau            : 38.1 m³
Cycles/min cible      : 6.6
Cycles/min réel       : 6.4
Dist/cycle            : 0.438 m
T. remplissage moy    : 2.3 s
T. vidange            : 1.5 s
T. attente            : 5.1 s
Facteur correction    : 1.000
Nb cycles total       : 291
```

### Onglet 3 — ⚙️ Config

```
PROGRAMME ACTIF
[Dropdown : 🌽 Maïs ▼]  [✏️ Renommer]

Dose cible         [ 25  ] mm  (10-50)
Largeur position   [ 18.0] m
Diamètre buse      [ 18  ] mm
Pression enrouleur [ 7.0 ] bar
Tempo départ  [✓] ON  [ 120] s
Tempo arrivée [ ] OFF [    ] s
[💾 ENREGISTRER PROGRAMME]

▶ Profil machine (dépliable)
  [Dropdown : ST1 Bis Ø82-330m ▼]
  [Dropdown abaque : SR 150C ▼  ]
  Durée vidange poumon  [ 0.0] s ⚠️
  Largeur bobine  [ 0.0] m  OU  Spires/étage [ 0.0]
  (saisir un seul — l'autre calculé automatiquement)
  Dénivelé              [ 0.0] m
  Gain Kp               [ 0.1]
  Cycles calibration    [   3]
  Fenêtre vitesse       [   5] impulsions
  [Reset étalonnage → facteur = 1.0]
  [💾 ENREGISTRER PARAMÈTRES]

▶ Modes dégradés (dépliable)
  [✓] Mode dégradé capteur vitesse
  [✓] Mode dégradé contact poumon
  T. remplissage fixe [ 0.0] s (mode B)

▶ Mise à jour firmware (dépliable)
  [Sélectionner fichier .bin]
  [📤 Mettre à jour]

▶ Mode IRRITESTEUR (dépliable)
  SORTIES :
    [EV_CANON ON ] [EV_CANON OFF ]
    [EV_POUMON ON] [EV_POUMON OFF]
  ENTRÉES (temps réel) :
    Capteur vitesse : [0000] pulses  [0.0] m/h
    Fin de course   : [OUVERT/FERMÉ]
    Sécurité spires : [OUVERT/FERMÉ]
    Contact poumon  : [OUVERT/FERMÉ]
    Pressostat      : [OUVERT/FERMÉ]
```

---

## 16. Couche télémétrie (LoRa futur)

```c
// Phase 1 : fonctions vides — log Serial uniquement
// Phase 2 : SX1276/SX1262 sur SPI sans modifier le reste
// GPIOs SPI réservés : MOSI=23, MISO=19, SCK=18, CS=5, RST=14, DIO0=2
void telemetry_send_status(const machine_status_t *s);
void telemetry_send_alarm(const char *raison);
void telemetry_send_session_end(const session_summary_t *s);
```

---

## 17. À mesurer terrain avant PR-05

```
⚠️ Ces valeurs sont nécessaires avant de tester la régulation :

1. t_vidange_s
   Chronométrer depuis EV_POUMON=OFF jusqu'au prochain
   démarrage de mouvement bobine (capteur vitesse)

2. Sens contacteurs (NO/NC)
   Multimètre en continuité sur chaque contact au repos
   Confirmer logique HIGH = danger pour tous

3. GPIO MOSFETs carte Quad MOS
   Schéma technique de la carte
   Identifier PIN_EV_CANON et PIN_EV_POUMON

4. Largeur bobine OU spires par étage
   Mètre ruban entre les deux flasques du tambour
```

---

## 18. Références

| Source | Usage |
|---|---|
| Fiche technique Irrifrance Structure 2 AT/P (Ø82-330m) | Table débit, paramètres machine |
| Tableau valeurs théoriques ST.1 Bis | Couches, spires, longueur spire |
| Schéma Branchement Bornier Irrifrance | Mapping bornes → GPIO |
| Manuel Irridoseur 4 (FAE-N21471 4/06) | Référence logique fonctionnement |
| UASA46, "Choisir le bon diamètre de buse", 19/06/2025 | Pertes de charge référence |

---

## 19. Portabilité et contribution GitHub

### Pour ajouter un nouvel enrouleur
```
1. Créer main/machines/[nom].c avec machine_profile_t
2. Ajouter à la liste dans machines.h
3. Créer main/abaques/[canon].c si nouveau canon
4. Ouvrir une PR sur GitHub
```

### Paramètres d'adaptation
| Paramètre | Fichier |
|---|---|
| Nb pastilles | `NB_PASTILLES` dans gpio_handler.h |
| Géométrie bobine | profil machine (r_tambour, d_tuyau, nb_etages) |
| Spires / largeur bobine | profil machine (un ou l'autre) |
| Mécanique poumon | t_vidange_s NVS |
| Table débit | canon_abaque_t dans abaques/ |
| Dénivelé | denivele_m NVS (défaut 0.0) |

---

## 20. GitHub — Conventions

```
Branches : main (protégée) / develop / feature/PR-XX
Commits  : feat: / fix: / test: / docs: / refactor:
```

### Template Pull Request
```markdown
## Description
## Tests effectués
- [ ] Tests unitaires passés
- [ ] Mode IRRITESTEUR vérifié
- [ ] Transitions états machine vérifiées
- [ ] securites_watchdog() appelé en premier
- [ ] EV_CANON=OFF, EV_POUMON=OFF sur toute erreur
- [ ] Contacts NC — logique HIGH = danger vérifiée
## Checklist
- [ ] Commentaires en français
- [ ] Aucune valeur hardcodée
- [ ] Portable (pas spécifique ST1 Bis)
```

---

*SPECS_FINAL_v3.md — Irrifrance ESP32*
*Installation référence : Irrifrance Structure 1 bis — Ø82mm ext / 330m / SR 150C — Terrain plat*
*L'utilisateur apportera ses modifications directement à partir de ce document.*
