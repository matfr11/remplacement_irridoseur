# SPECS_FINAL v3 — Irrifrance ESP32
## Remplacement régulation Irridoseur 3 — Document de référence unique

> Ce document remplace et consolide tous les documents précédents.
> Mis à jour au 2026-05-29 pour refléter l'implémentation réelle sur `main`.

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
4. **Défensif par défaut** — cas limites, valeurs hors plage, déconnexions
5. **Sécurité machine — FAIL SAFE** — doute sur état → EV_CANON=OFF, EV_POUMON=OFF immédiat
6. **Code portable** — paramètres spécifiques en NVS ou profil machine, jamais hardcodés
7. **Commentaires en français** — projet communauté agricole francophone GitHub
8. **Strings C ASCII uniquement** — pas d'accents dans le code C (logs, clés NVS)

### État du développement — 2026-05-29

Toutes les fonctionnalités sont implémentées et mergées sur `main`. Build OK, 54% flash libre.

```
PR-01 à PR-12 + session 2026-05-29 : TERMINÉS
Commit courant : 58d433c
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
    → Le capteur vitesse peut compter des impulsions ICI
    Fin : contact poumon plein détecté (ou temporisé en mode dégradé B)

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

> ⚠️ **Action avant mise en service** : identifier les GPIO utilisés par les
> 4 canaux MOSFET depuis le schéma technique de la carte.
> Mettre à jour PIN_EV_CANON et PIN_EV_POUMON dans `main/gpio_config.h`.
> Retirer les deux lignes `#warning` une fois les pins identifiés.

### Boîtier et connectique
| Composant | Détail |
|---|---|
| Boîtier IP65 | ~200×150×80mm |
| Bornier DIN rail 12 voies | Connexions terrain |
| Rail DIN 15cm | Fixé dans boîtier |
| Résistances 10kΩ × 4 | Pull-up contacts — soudées sur fils + gaine thermo |
| Diviseur 10kΩ + 3.3kΩ | Capteur vitesse — soudé sur fil + gaine thermo |
| Diviseur 100kΩ + 27kΩ | Mesure tension batterie GPIO 36 — 14V→3V — soudé |

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
| **36** | INPUT (ADC1) | Mesure tension batterie | Diviseur 100kΩ/27kΩ — plage 0-14V→0-3V |
| **⚠️ A** | OUTPUT | EV_CANON 12V | GPIO à identifier carte Quad MOS (provisoire GPIO 25) |
| **⚠️ B** | OUTPUT | EV_POUMON 12V | GPIO à identifier carte Quad MOS (provisoire GPIO 26) |

### Logique des signaux — CONTACTS NC (Normalement Fermés)

> ⚠️ Règle fail-safe fondamentale :
> Fil coupé ou capteur HS → GPIO HIGH → sécurité déclenchée.

| Signal | Repos LOW | Activé HIGH | Fil coupé |
|---|---|---|---|
| Fin de course | Canon déroulé ✅ | Canon rentré → SEC-1 | → SEC-1 ✅ |
| Sécurité spires | Normal ✅ | Débordement → SEC-2 | → SEC-2 ✅ |
| Contact poumon | En cours ✅ | Poumon plein | → Timeout → mode dégradé B |
| Pressostat | Pression OK ✅ | Pas de pression | → PAUSE_PRESSION |

### Bornier 14 voies

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
| 13 | Batterie + | GPIO 36 via diviseur 100kΩ/27kΩ |
| 14 | GND mesure batterie | GND commun |

---

## 4. Profils machine

### Structure profil

```c
typedef struct {
    char    nom[32];
    char    constructeur[32];
    float   longueur_tuyau_m;
    float   d_tuyau_ext_m;
    float   r_tambour_vide_m;
    int     nb_etages;
    int     abaque_idx;

    // Double entrée — renseigner UN SEUL des deux
    float   largeur_bobine_m;   // Mesure terrain (mètre ruban)
    float   spires_par_etage;   // Valeur constructeur ou comptage
} machine_profile_t;
```

### Profil installation référence

```c
// main/machines/st1bis_82_330.c
{
    .nom               = "ST1 Bis Ø82-330m",
    .constructeur      = "Irrifrance",
    .longueur_tuyau_m  = 330.0f,
    .d_tuyau_ext_m     = 0.082f,
    .r_tambour_vide_m  = 0.690f,   // 0.977 - (3.5 × 0.082)
    .nb_etages         = 4,
    .abaque_idx        = 0,        // SR 150C
    .spires_par_etage  = 13.45f,   // Source tableau constructeur ST.1 Bis
    // largeur_bobine auto-calculée → 13.45 × 0.082 = 1.103m
}
```

---

## 5. Abaques — Table débit constructeur

### Fonction lookup — double interpolation

```c
/**
 * Niveau 1 — Ligne (p_enrouleur + buse_mm) : 2 voisins les plus proches
 * Niveau 2 — Dose (dose_mm) : colonnes D40/D30/D25/D20/D15 (décroissant)
 * Extrapolation pour doses 10-15mm et 40-50mm.
 *
 * @param abaque       Pointeur abaque actif
 * @param p_enrouleur  Pression manomètre (bar)
 * @param buse_mm      Diamètre buse (mm)
 * @param dose_mm      Dose cible (mm) — plage 10 à 50mm
 * @param debit_out    Débit résultant m³/h (NULL si non souhaité)
 * @param p_buse_out   Pression effective à la buse bar (NULL si non souhaité)
 * @return             Vitesse cible m/h
 */
float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float *debit_out,
                            float *p_buse_out);
```

---

## 6. Calculs mécaniques

### Fonctions disponibles

```c
// Rayon effectif à l'étage n (numérotation depuis 1)
// R_n = r_tambour_vide + (n - 0.5) × d_tuyau_ext
float calcul_rayon_etage(int n, const machine_profile_t *profil);

// Distance par impulsion capteur à l'étage n
// dist_pulse_m = (2π × R_n) / NB_PASTILLES
float calcul_dist_pulse_m(float r_etage_m);

// Distance par cycle poumon à l'étage n (géométrie — indépendant du capteur)
// dist_cycle_m = (2π × R_n) / cycles_par_tour
float calcul_dist_cycle_m(float r_etage_m, float cycles_par_tour);

// Étage courant depuis longueur enroulée (position absolue interne)
int calcul_etage_courant(float longueur_enroulee_m, const machine_profile_t *profil);

// Surface arrosée session
// surface = longueur_session_m × largeur_m
float calcul_surface_m2(float longueur_session_m, float largeur_m);

// Dose instantanée
// dose = (debit_m3h / (vitesse_m_h × largeur_m)) × 1000
float calcul_dose_inst_mm(float debit_m3h, float vitesse_m_h, float largeur_m);

// Validation et calcul facteur étalonnage
bool calcul_facteur_etalonnage(float longueur_theorique_m,
                                float longueur_reelle_m,
                                int nb_impulsions,
                                float *facteur_out);
```

### Capteur vitesse — mesure robuste

```c
// ISR : anti-rebond 50ms, RISING uniquement, fenêtre N impulsions
// vitesse_m_h = (N × dist_pulse_m × facteur_correction) / temps_N_pulses_s × 3600
// N = fenetre_vitesse (NVS, défaut 5)
// Si cycles_sans_impulsion > max_cycles_si → vitesse = 0
#define NB_PASTILLES       10
#define DEBOUNCE_VITESSE_MS 50
```

### Vitesse depuis cycles poumon (mode normal si cycles_par_tour > 0)

```
dist_cycle_m = calcul_dist_cycle_m(r_etage, cycles_par_tour)
v_reel = dist_cycle_nvs × 3600 / t_cycle_s      (m/h)
```

`gpio_handler_set_vitesse_estimee(v_reel)` est appelé chaque tick EN_COURS.
Quand `cycles_par_tour > 0`, `gpio_handler_set_mode_degrade_a(true)` est activé
automatiquement — `gpio_get_vitesse_m_h()` retourne directement cette estimation.

---

## 7. Régulation poumon — Feedforward + correction

### Calcul T_attente

```c
// T_attente_s = T_cycle_cible_s - t_remplissage_mesure_s - t_vidange_s
// Compensation automatique pression (t_remplissage court → T_attente augmente)
// T_attente < 0 → T_attente = 0 + alerte "Pression insuffisante"
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s,
                          bool *alerte_out);

// Correction Kp après n_cycles_calib cycles
float correction_vitesse(float t_attente_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp);
```

### Gestion timeout remplissage — 2 tentatives

```
Tentative 1 :
  EV_POUMON = ON → contact poumon plein avant 20s → succès ✅
  20s sans contact → EV_POUMON=OFF, attente t_vidange_s, tentative 2

Tentative 2 :
  EV_POUMON = ON → contact avant 20s → succès ✅
  20s sans contact → EV_POUMON=OFF, EV_CANON=OFF → ARRET_URGENCE
    "Remplissage poumon echoue (2/2)"
```

---

## 8. Modes dégradés

> **Activation manuelle uniquement** depuis Config web UI (sauf mode vitesse).

### Mode vitesse automatique (cycles_par_tour)

```
Pas de flag dédié. Comportement automatique si cycles_par_tour > 0 (NVS).
  - Vitesse estimée depuis les cycles poumon : v = dist_cycle_nvs × 3600 / t_cycle_s
  - cycles_par_tour = 40 sur l'enrouleur de référence (mesure physique)
  - gpio_handler_set_mode_degrade_a(true) activé au chargement config
  - Transparent pour l'opérateur, aucune alerte
```

### Mode dégradé B — Contact poumon plein HS

```
Activation : Config → [✓] Mode dégradé contact poumon
             Saisir t_rempl_fixe_s (à mesurer terrain)
Fonctionnement : EV_POUMON=ON pendant t_rempl_fixe_s, sans détection contact
Web UI : alerte orange "Mode dégradé : contact poumon"
NVS : mode_deg_poumon, t_rempl_fixe_s
```

### Bypass sécurité spires — Capteur SEC-2 défaillant

```
Activation : Config → Modes dégradés → [✓] Bypass sécurité spires (capteur défaillant)
Fonctionnement : SEC-2 loggée WARN mais ne déclenche plus l'urgence
Web UI : alerte rouge permanente "SECURITE SPIRES DESACTIVEE — risque debordement bobine"
NVS : mode_deg_spires (bool)
⚠️ Usage uniquement si le capteur de débordement est physiquement HS
```

---

## 9. Watchdog sécurités

**Exécuté EN PREMIER dans tick_state_machine — priorité absolue.**

```c
void securites_watchdog(void) {
    // SEC-2 : Débordement spires — priorité absolue, tout état
    if (e.secu_spires) {
        if (s_bypass_spires) {
            ESP_LOGW(TAG, "SEC-2 spires active — IGNOREE (mode degrade spires ON)");
        } else {
            gpio_all_ev_off();
            state_machine_declencher_urgence("Debordement bobine");
            return;
        }
    }

    // SEC-1 : Fin de course en cours de cycle
    bool sec1_applicable = (etat == ETAT_OUVERTURE_CANON    ||
                            etat == ETAT_TEMPO_DEPART       ||
                            etat == ETAT_REMPLISSAGE_POUMON ||
                            etat == ETAT_EN_COURS           ||
                            etat == ETAT_PAUSE_PRESSION);
    if (sec1_applicable && e.fin_course) {
        gpio_all_ev_off();
        state_machine_declencher_urgence("Fin de course en cours de cycle");
        return;
    }
    // SEC-P pression : gérée dans tick_state_machine()
}
```

---

## 10. Machine d'états — 10 états

### Définition

```c
typedef enum {
    ETAT_VEILLE,              // 0 — Attente conditions démarrage
    ETAT_OUVERTURE_CANON,     // 1 — EV_CANON=ON, attend pression stable 3s
    ETAT_TEMPO_DEPART,        // 2 — EV_CANON=ON, arrosage sur place avant enroulement
    ETAT_REMPLISSAGE_POUMON,  // 3 — EV_POUMON=ON, attend contact poumon plein
    ETAT_EN_COURS,            // 4 — EV_CANON=ON, régulation cycles poumon active
    ETAT_PAUSE_PRESSION,      // 5 — EV tout OFF, pression perdue
    ETAT_TEMPO_ARRIVEE,       // 6 — EV_CANON=ON EV_POUMON=OFF, arrosage final
    ETAT_ARRET_FINAL,         // 7 — EV tout OFF, bilan session
    ETAT_ARRET_URGENCE,       // 8 — EV tout OFF, incident matériel
    ETAT_DEROULE,             // 9 — Mesure longueur déployée (avant session)
} etat_machine_t;

// Sous-états cycle poumon (ETAT_EN_COURS uniquement)
typedef enum {
    SOUS_VIDANGE,     // EV_POUMON=OFF, ressort rétracte cliquet (bobine immobile)
    SOUS_ATTENTE,     // EV_POUMON=OFF, pause régulation
    SOUS_REMPLISSAGE, // EV_POUMON=ON, cliquet avance bobine
} sous_etat_poumon_t;
```

### Détail des états

#### ETAT_VEILLE
```
EV_CANON=OFF, EV_POUMON=OFF
Démarrage automatique si C1+C2+C3+C4 :
  C1 : pression OK (pressostat LOW)
  C2 : fin_course inactif (canon déroulé, LOW)
  C3 : programme valide (dose+largeur+buse+pression > 0)
  C4 : s_demarrage_autorise=true (false après urgence ou cmd_stop)

Transitions :
  C1+C2+C3+C4 → ETAT_OUVERTURE_CANON
  Flanc descendant fin_course → ETAT_DEROULE (déploiement auto)
  cmd_start → s_demarrage_autorise=true
  cmd_start_deroule → ETAT_DEROULE (manuel)
```

#### ETAT_DEROULE (état 9 — nouveau)
```
EV_CANON=OFF, EV_POUMON=OFF
Mesure la longueur déployée via pastilles pendant que le tracteur tire le tuyau.

Fonctionnement :
  Chaque impulsion capteur → incrémente s_mesure_deroule_m
  (dist_pulse × facteur_correction par impulsion, étage calculé depuis position inverse)
  Affichage en temps réel dans l'UI ("Déployé mesuré : X m")
  L'opérateur peut corriger la valeur mesurée (bouton ✏️ → set_longueur)

Démarrage session :
  Mêmes conditions que VEILLE (pression + pas fin_course + cfg_valide)
  → s_longueur_deroule_m = s_mesure_deroule_m
  → s_longueur_enroulee = total - s_mesure_deroule_m (position abs. interne)
  → s_longueur_session_m = 0
  → ETAT_OUVERTURE_CANON
```

#### ETAT_OUVERTURE_CANON
```
EV_CANON=ON
Attend pression stable 30 ticks × 100ms = 3s
→ tempo_depart_on : ETAT_TEMPO_DEPART
→ sinon : ETAT_REMPLISSAGE_POUMON
```

#### ETAT_TEMPO_DEPART
```
EV_CANON=ON, EV_POUMON=OFF
Canon arrose sur place (départ tuyau) pendant tempo_depart_s
→ ETAT_REMPLISSAGE_POUMON
→ Pression perdue → ETAT_PAUSE_PRESSION
```

#### ETAT_REMPLISSAGE_POUMON
```
EV_POUMON=ON
2 tentatives × 20s (voir section 7)
Poumon plein → EV_POUMON=OFF → ETAT_EN_COURS (SOUS_VIDANGE)
2 tentatives échouées → ETAT_ARRET_URGENCE
```

#### ETAT_EN_COURS
```
EV_CANON=ON, EV_POUMON=piloté
Sous-états : SOUS_REMPLISSAGE → SOUS_VIDANGE → SOUS_ATTENTE → (boucle)

Chaque fin de SOUS_REMPLISSAGE :
  - dist_cycle_m calculée (géométrie étage courant)
  - s_longueur_session_m += dist_cycle
  - NVS sauvé toutes les 5m
  - T_attente recalculé (feedforward + Kp après n_cycles_calib)
  - v_reel estimée → gpio_handler_set_vitesse_estimee()

Fin de course → tempo_arrivee_on → ETAT_TEMPO_ARRIVEE, sinon ETAT_ARRET_FINAL
Pression perdue → ETAT_PAUSE_PRESSION
SEC → ETAT_ARRET_URGENCE
```

#### ETAT_PAUSE_PRESSION
```
EV tout OFF
Chronomètre durée arrêt
Pression revenue → EV_CANON=ON → ETAT_EN_COURS (SOUS_VIDANGE)
```

#### ETAT_TEMPO_ARRIVEE
```
EV_CANON=ON, EV_POUMON=OFF — SEC-1 ignorée (fin de course normal ici)
Timer tempo_arrivee_s → EV_CANON=OFF → ETAT_ARRET_FINAL
```

#### ETAT_ARRET_FINAL
```
EV tout OFF
Bilan session (longueur, surface, dose, volume, durée, cycles)
Mise à jour stats campagne NVS (irri_stats) avant envoi bilan
Retour VEILLE automatique quand fin_course disparaît (opérateur redéploie)

Transitions :
  Fin_course LOW (redéploiement) → reset longueurs → ETAT_VEILLE
  cmd_reset → reset longueurs → ETAT_VEILLE
  cmd_resume → préserve longueurs → ETAT_VEILLE (reprise session)
```

#### ETAT_ARRET_URGENCE
```
EV tout OFF
Raison stockée NVS (survit au redémarrage, s_demarrage_autorise=false)
Réarmement physique : 3 appuis poumon_plein → ETAT_VEILLE

Transitions :
  cmd_reset → reset longueurs → ETAT_VEILLE
  cmd_resume → préserve longueurs → ETAT_VEILLE (si incident résolu)
```

### Vue d'ensemble transitions

```
VEILLE ──────────────────────────→ DEROULE (flanc fin_course ou cmd_start_deroule)
  │ C1+C2+C3+C4                        │ pression+fin_course libéré
  ▼                                    ▼
OUVERTURE_CANON ─────────────────────────────────────────────────
       │ pression 3s
  ┌────┴────┐
tdep     pas tdep
  ▼         ▼
TEMPO_DEPART → REMPLISSAGE_POUMON → EN_COURS
                                        │ fin_course
                              ┌─────────┴─────────┐
                           tempo_arr            pas tempo
                              ▼                    ▼
                        TEMPO_ARRIVEE ──→ ARRET_FINAL
                                                │ fin_course↓ ou cmd_reset ou cmd_resume
                                              VEILLE

PAUSE_PRESSION ← pression perdue (depuis TEMPO_DEP, REMPL_POU, EN_COURS)
      │ pression revenue → EN_COURS

ARRET_URGENCE ← SEC-1 ou SEC-2 (tout état sauf TEMPO_ARR, ARRET_*)
      │ cmd_reset ou cmd_resume → VEILLE
```

---

## 11. API state_machine

```c
// Commandes opérateur (depuis WebSocket)
void state_machine_cmd_start(void);
void state_machine_cmd_stop(void);
void state_machine_cmd_reset(void);          // reset longueurs → VEILLE
void state_machine_cmd_resume(void);         // préserve longueurs → VEILLE
void state_machine_cmd_start_deroule(void);  // entre ETAT_DEROULE
void state_machine_cmd_set_longueur(float longueur_deroule_m);
void state_machine_cmd_etalonner(float longueur_reelle_m);
void state_machine_set_time(int64_t timestamp_unix);

// Stats campagne
void state_machine_cmd_reset_campagne(void);

// Calcul vitesse pour preview programme (appel depuis webserver HTTP GET /api/vitesse)
float state_machine_calc_vitesse(float pression_bar, float buse_mm, float dose_mm,
                                  float *debit_out, float *p_buse_out);

// Mode IRRITESTEUR (VEILLE uniquement)
void state_machine_cmd_ev_canon_set(bool actif);
void state_machine_cmd_ev_poumon_set(bool actif);

// Sécurité (appelé par securites.c)
void state_machine_declencher_urgence(const char *raison);

// Reload config NVS sans redémarrage (VEILLE uniquement)
void state_machine_recharger_config(void);
```

### Longueurs internes — trois variables distinctes

```
s_longueur_enroulee   : position absolue tambour (0 = vide) — calculs étage/rayon
s_longueur_session_m  : progression session (0 → déroulée) — affiché "Enroulé"
s_longueur_deroule_m  : longueur déployée champ (constante session) — affiché "Déroulé"

cmd_reset  → les trois à zéro (nouvelle session)
cmd_resume → toutes PRÉSERVÉES (reprise session interrompue)
```

---

## 11b. Module batterie

### API publique (batterie.h)

```c
// Initialise ADC1 (GPIO 36, attenuation 12dB, calibration curve fitting si eFuse).
esp_err_t batterie_init(void);

// Lit la tension batterie instantanee (moyenne 16 echantillons ADC).
float batterie_lire_voltage(void);

// Retourne le statut complet (tension + etat + pourcentage).
batt_status_t batterie_get_status(void);

// Charge les seuils configurables (appele dans charger_config_interne).
void batterie_set_seuils(float warn_v, float crit_v);

const char* batterie_etat_str(batt_etat_t etat);    // "En charge" / "Pleine" / etc.
const char* batterie_etat_couleur(batt_etat_t etat); // "#22c55e" etc. (CSS hex)

#ifdef CONFIG_IRRI_TEST_MODE
// Injecte une tension fixe. val=0.0 reactive l'ADC reel.
void batterie_sim_set_voltage(float v);
#endif
```

### Etats et seuils

| Etat | Seuil | Couleur |
|---|---|---|
| En charge | >= 13.5V | `#3b82f6` (bleu) |
| Pleine | >= 12.4V | `#22c55e` (vert) |
| Correcte | >= 11.8V | `#eab308` (jaune) |
| Faible | >= seuil NVS (def. 11.5V) | `#f97316` (orange) |
| Critique | < seuil NVS (def. 11.0V) | `#ef4444` (rouge) |

### Mesure

- Diviseur : V_batt = V_adc × (100 + 27) / 27 = V_adc × 4,70
- Cadence : toutes les **30 secondes** (300 ticks × 100ms dans tick_state_machine)
- Première mesure au 1er tick (s_batt_tick initialisé à 299)
- Simulateur web : slider 10.0-14.5V, encodé entier × 10 sur la WebSocket `/ws_test`

---

## 12. NVS — Paramètres

Toutes les structures stockées en **blob** (une seule clé par namespace).

### Namespace `irri_machine` — blob clé `cfg` (config_machine_t)

| Champ | Défaut | Note |
|---|---|---|
| machine_active | 0 | Index profil machine |
| t_vidange_s | 0.0 | ⚠️ À mesurer terrain |
| facteur_correction | 1.0 | Étalonnage longueur |
| dist_cycle_nvs | 0.0 | Dernière dist/cycle connue (moyenne) |
| kp_regulation | 0.1 | Gain correction vitesse |
| n_cycles_calib | 3 | Cycles avant activation Kp |
| fenetre_vitesse | 5 | Nb impulsions fenêtre vitesse |
| max_cycles_si | 15 | Cycles sans impulsion → v=0 |
| mode_deg_poumon | false | Mode dégradé contact poumon |
| mode_deg_spires | false | Bypass SEC-2 capteur défaillant |
| t_rempl_fixe_s | 0.0 | Durée remplissage fixe (mode dégradé B) |
| denivele_m | 0.0 | Dénivelé terrain |
| cycles_par_tour | 0.0 | Nb cycles poumon par tour bobine (40 sur enrouleur référence) |

### Namespace `irri_machine` — clés float blob séparées (hors cfg)

Stockées individuellement pour éviter toute migration du blob `cfg`.

| Clé | Défaut | Description |
|---|---|---|
| `batt_warn` | 11.5V | Seuil alerte batterie faible (float blob) |
| `batt_crit` | 11.0V | Seuil critique batterie (float blob) |

### Namespace `irri_prog` × 5 — blob clé `prog` (config_programme_t)

Namespaces : `irri_prog0` à `irri_prog4`

| Champ | Type | Note |
|---|---|---|
| nom | char[21] | 20 chars + null |
| dose_mm | float | Dose cible (mm) |
| largeur_m | float | Largeur position (m) |
| buse_mm | int | Diamètre buse (mm) |
| pression_bar | float | Pression enrouleur (bar) |
| tempo_depart_on | bool | |
| tempo_depart_s | int | |
| tempo_arrivee_on | bool | |
| tempo_arrivee_s | int | |

### Namespace `irri_state` — clés individuelles

| Clé | Type | Description |
|---|---|---|
| `prog_actif` | int32 | Index programme actif (0-4) |
| `urgence` | string | Dernière raison arrêt urgence (vide = pas d'urgence) |
| `longueur_m` | float blob | Position absolue tambour (survie redémarrage) |
| `deroule_m` | float blob | Longueur déployée champ (survie redémarrage) |

### Namespace `irri_stats` — blob clé `camp` (config_stats_t)

| Champ | Type | Description |
|---|---|---|
| total_surface_ha | float | Surface cumulée (ha) |
| total_volume_m3 | float | Volume eau cumulé (m³) |
| dose_moy_mm | float | Dose moyenne pondérée par surface |
| vitesse_moy_m_h | float | Vitesse moyenne pondérée par durée |
| nb_sessions | uint32_t | Nombre de sessions terminées |
| total_duree_h | float | Durée totale cumulée (h) |

Mise à jour à chaque ETAT_ARRET_FINAL. Reset via cmd_reset_campagne (UI ou WS).

---

## 13. Machine d'états — machine_status_t (statut diffusé)

```c
typedef struct {
    etat_machine_t   etat;
    sous_etat_poumon_t sous_etat;
    char             raison_arret[64];

    // Programme actif
    char             prog_nom[21];
    char             machine_nom[32];
    char             abaque_nom[32];
    float            prog_dose_mm, prog_largeur_m, prog_pression_bar;
    int              prog_buse_mm;
    bool             prog_tempo_depart_on, prog_tempo_arrivee_on;
    int              prog_tempo_depart_s,  prog_tempo_arrivee_s;

    // Longueurs
    float            longueur_deroulee_m;   // déroulé champ (constant session)
    float            longueur_enroulee_m;   // progression session (0 → déroulée)

    // Vitesse
    float            vitesse_m_h;           // vitesse réelle mesurée/estimée
    float            vitesse_cible_m_h;     // vitesse cible abaque

    // Temps
    int32_t          duree_s;
    int64_t          heure_arrivee_unix;
    int32_t          heure_arrivee_relative_s;
    bool             heure_synchro;

    // Agronomie (calculés chaque tick)
    float            surface_m2;
    float            dose_inst_mm;
    float            debit_m3h;
    float            p_enrouleur_bar;
    float            p_buse_bar;

    // Mécanique bobine
    int              etage_courant;
    int              nb_etages;

    // GPIO
    bool             ev_canon, ev_poumon, pression_ok, fin_course, secu_spires, poumon_plein;

    // ETAT_DEROULE
    float            mesure_deroule_m;

    // Régulation poumon (calculés chaque tick)
    int32_t          t_remplissage_ms, t_attente_ms;
    float            dist_par_cycle_m;
    float            cycles_par_min_cible, cycles_par_min_reel;
    uint32_t         cycles_total;

    // Alertes / modes
    bool             alerte_pression_insuff;
    bool             mode_degrade_poumon;
    bool             mode_degrade_spires;
    float            facteur_correction;
    bool             cfg_valide;

    // Campagne (cumulatif NVS)
    float            camp_surface_ha;
    float            camp_volume_m3;
    float            camp_dose_moy_mm;
    float            camp_vitesse_moy_m_h;
    uint32_t         camp_nb_sessions;
    float            camp_duree_h;

    // Batterie
    float           batterie_v;
    int             batterie_pct;
    int             batterie_etat;      // batt_etat_t cast en int
    float           cfg_batt_warn_v;
    float           cfg_batt_crit_v;

    // Config machine (initialisation UI)
    float            cfg_t_vidange_s, cfg_kp_regulation, cfg_t_rempl_fixe_s, cfg_denivele_m;
    int              cfg_n_cycles_calib, cfg_fenetre_vitesse, cfg_max_cycles_si, cfg_machine_active;
    float            cfg_cycles_par_tour;
} machine_status_t;
```

---

## 14. WiFi et WebSocket

```
SSID : "Irrifrance-ESP32"  |  Password : "irrigation"
IP   : 192.168.4.1         |  URL : http://192.168.4.1
JSON_BUF_SIZE : 3072 bytes
Broadcast statut : toutes les 500ms via WebSocket
```

Champs batterie inclus dans chaque broadcast :

| Champ JSON | Type | Description |
|---|---|---|
| `batterie_v` | float | Tension batterie (V) |
| `batterie_pct` | int | Pourcentage indicatif (0-100%) |
| `batterie_etat` | string | "En charge" / "Pleine" / "Correcte" / "Faible" / "Critique" |
| `batterie_couleur` | string | Code couleur CSS (#rrggbb) |
| `cfg_batt_warn_v` | float | Seuil alerte NVS (V) |
| `cfg_batt_crit_v` | float | Seuil critique NVS (V) |

### Commandes WebSocket (navigateur → ESP32)

| Commande | Paramètres | Action |
|---|---|---|
| `start` | — | Autorise démarrage |
| `stop` | — | Arrêt → ARRET_FINAL |
| `reset` | — | Reset longueurs → VEILLE |
| `resume` | — | Préserve longueurs → VEILLE |
| `set_time` | `ts` (int64) | Sync heure Unix |
| `ev_canon` | `actif` (bool) | IRRITESTEUR (VEILLE seulement) |
| `ev_poumon` | `actif` (bool) | IRRITESTEUR (VEILLE seulement) |
| `start_deroule` | — | Entre ETAT_DEROULE manuellement |
| `select_programme` | `idx` (0-4) | Change programme actif |
| `etalonner` | `longueur_m` | Calcule facteur correction |
| `set_longueur` | `longueur_m` | Force longueur déroulée |
| `save_programme` | voir config_programme_t | Sauvegarde programme |
| `save_machine` | voir config_machine_t | Sauvegarde params machine |
| `reset_campagne` | — | RAZ stats campagne NVS |

### Endpoints HTTP

| Méthode | URL | Description |
|---|---|---|
| GET | `/` | Web UI embarquée |
| GET | `/ws` | WebSocket |
| POST | `/ota/update` | Mise à jour firmware |
| GET | `/api/vitesse?p=X&b=Y&d=Z` | Preview vitesse cible → `{"vitesse_m_h":X,"debit_m3h":Y,"p_buse_bar":Z}` |

---

## 15. Interface Web (Web UI)

### Contraintes
- 100% embarqué — zéro ressource externe — EMBED_FILES CMake
- Mobile 6" portrait, ~390px, dark theme, boutons ≥ 48px
- Fichier : `main/webui/index.html` (~50KB)

### Onglet 1 — Accueil

```
[BADGE ÉTAT — couleur selon état]
[Prog : Maïs • Machine : ST1 Bis]

[Déroulé ✏️] [Enroulé ]
[330 m     ] [127 m   ]

[Vitesse   ] [Durée   ]
[54.0 m/h  ] [01:23   ]

[Estimation arrivée (si vitesse > 0 et déroulé connu) :]
  Durée enroulement : 1h45min
  Arrivée prévue   : 14h35 (si heure synchro)

[Alerte ARRET URGENCE : raison]
[Alerte pression insuffisante]
[Alerte mode dégradé poumon]
[Alerte SECURITE SPIRES DESACTIVEE (rouge vif)]

[Panneau DEROULE (si ETAT_DEROULE) :]
  Déployé mesuré : 247 m [✏️]

[Batterie : 12.5 V — Pleine (94%)]
[████████████████░░░░] ← barre de niveau, couleur selon état

[EV CANON  ● OUVERTE/FERMEE]
[EV POUMON ● OUVERTE/FERMEE]
[Pression  ✅/⛔]
[Fin de course ●]

[▶ DEMARRER          ] ← vert, désactivé si cfg invalide ou hors VEILLE/DEROULE
[■ ARRET URGENCE     ] ← rouge, visible si session en cours
[↺ RESET             ] ← orange, visible si ARRET_*
[↩ REPRENDRE SESSION ] ← orange, visible si ARRET_* ET longueur_enroulee_m > 0
[▶ Mesurer deroulement] ← gris, visible si VEILLE + pas fin_course
```

### Onglet 2 — Stats

Deux sous-onglets [Session] [Campagne]

**Sous-onglet Session** (reset à chaque nouvelle session/DEROULE) :
```
Surface arrosée    : 0.230 ha
Dose instantanée   : 27.4 mm
Débit calculé      : 24.3 m³/h
P. enrouleur/buse  : 7.0 / 3.4 bar
Vitesse cible      : 54.0 m/h
Etage spires       : 2 / 4
Volume eau estimé  : 6.3 m³
Cycles/min cible   : 6.6
Cycles/min réel    : 6.4
Dist/cycle         : 0.438 m
T. remplissage     : 2.3 s
T. attente         : 5.1 s
Facteur correction : 1.0000
Cycles total       : 291
Batterie           : 12.5 V
```

**Sous-onglet Campagne** (cumulatif NVS — survit au redémarrage) :
```
Sessions          : 12
Surface totale    : 3.540 ha
Volume total      : 123.4 m³
Dose moyenne      : 27.1 mm
Vitesse moyenne   : 53.8 m/h
Durée totale      : 22.3 h

[Réinitialiser campagne] ← rouge, avec confirm()
```

### Onglet 3 — Config

```
PROGRAMME ACTIF
[Dropdown prog 1-5]  [✏️ Renommer]
Dose (mm)   [25 ] | Largeur (m)  [18.0]
Buse (mm)   [18 ] | Pression bar [7.0 ]
  Vitesse est. : 54.0 m/h • Débit : 24.3 m³/h  ← preview live (fetch /api/vitesse)
Tempo départ  [✓] [120 s]
Tempo arrivée [ ]
[Enregistrer programme]

▶ Profil machine
  [Dropdown machine] [Dropdown abaque]
  T. vidange (s) [0.0] ⚠️
  Dénivelé (m)   [0.0]
  Gain Kp        [0.1]
  Cycles avant correction [3]
  Impulsions fenêtre vitesse [5]
  Cycles poumon / tour bobine [40] ← vitesse depuis cycles si > 0
  [Reset étalonnage → facteur = 1.0]
  [Enregistrer parametres]

▶ Modes dégradés
  [✓] Mode dégradé contact poumon
  T. remplissage fixe [0.0 s]
  [✓] Bypass sécurité spires (capteur défaillant) ← rouge vif

▶ Batterie
  Seuil alerte (V)   [11.5]
  Seuil critique (V) [11.0]
  [Enregistrer seuils]

▶ Mise à jour firmware (OTA)

▶ IRRITESTEUR
  SORTIES : [EV CANON ON] [EV CANON OFF] [EV POUMON ON] [EV POUMON OFF]
  ENTREES : Fin de course, Sécurité spires, Contact poumon, Pressostat
```

---

## 16. Architecture logicielle

### Structure fichiers

```
irrifrance-esp32/
├── CMakeLists.txt
├── partitions.csv              ← app0, app1 (OTA), nvs
├── sdkconfig.defaults
├── SPECS_FINAL_v3.md
├── .github/workflows/ci.yml   ← CI/CD GitHub Actions
└── main/
    ├── CMakeLists.txt          ← EMBED_FILES index.html
    ├── main.c
    ├── gpio_handler.c/h        ← ISR vitesse, entrees_t, mode_degrade_a
    ├── securites.c/h           ← watchdog SEC-1/SEC-2 + bypass spires
    ├── batterie.c/h            ← mesure ADC1 GPIO 36, seuils NVS, sim
    ├── state_machine.c/h       ← 10 états, machine_status_t
    ├── regulation.c/h          ← feedforward T_attente, Kp, dist_cycle moyenne
    ├── calculs_hydraulique.c/h ← lookup_vitesse_cible, surface, dose, débit
    ├── calculs_mecanique.c/h   ← rayon, dist_pulse, dist_cycle, étage, étalonnage
    ├── config_nvs.c/h          ← 4 namespaces NVS (blobs)
    ├── ota.c/h                 ← OTA HTTP POST
    ├── webserver.c/h           ← WiFi AP, HTTP, WebSocket, /api/vitesse
    ├── webui.h                 ← EMBED_FILES (webui_html_start/end)
    ├── telemetry.c/h           ← bilan session WebSocket, stubs LoRa futur
    ├── webui/index.html        ← UI embarquée (~50KB)
    ├── simulator/              ← Simulateur web (CONFIG_IRRI_TEST_MODE) — slider tension batterie
    ├── machines/               ← Profils machine
    ├── abaques/                ← Tables débit constructeur
    └── test/                   ← Tests Unity sur cible (test_config_nvs.c etc.)

test/host/                      ← Tests Unity natif PC (CMake)
    ├── CMakeLists.txt
    ├── scenarios/              ← 13 scénarios intégration (35 tests)
    └── build_ci/               ← Résultats CI
```

### FreeRTOS — tâches

```c
void app_main(void) {
    config_nvs_init();
    batterie_init();           // ADC1 GPIO 36, calibration eFuse
    gpio_handler_init();
    state_machine_init();      // charge NVS, stats campagne, profil
    webserver_init();          // WiFi AP + HTTP + WebSocket

    xTaskCreate(state_machine_task, "sm",   4096, NULL, 5, NULL);
    xTaskCreate(broadcast_task,     "ws",   8192, NULL, 3, NULL);
}

void state_machine_task(void *pv) {
    while (1) {
        tick_state_machine();          // inclut securites_watchdog() EN PREMIER
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void broadcast_task(void *pv) {
    while (1) {
        webserver_broadcast_status();  // JSON 3072B → tous clients WS
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

---

## 17. OTA — Mise à jour firmware

```
Partitions : app0 (actif), app1 (OTA), nvs (préservée ✅)
Config → Mise à jour → [Sélectionner .bin] → [Mettre à jour]
Barre de progression → "Reussi - Redemarrage dans 3s"
```

---

## 18. Tests

### Tests PC (Unity natif, CI GitHub Actions)

```bash
cd test/host
cmake -B build && cmake --build build && build/run_tests
# 35 tests — 13 scénarios (hydraulique, mécanique, régulation, modes dégradés, urgences)
```

### Tests sur cible (ESP-IDF Unity)

Activés via `CONFIG_IRRI_ENABLE_TESTS=y` dans sdkconfig.

---

## 19. À faire avant mise en service terrain

```
1. ⚠️ GPIO EV_CANON / EV_POUMON
   Identifier OUT1/OUT2 sur le schéma technique carte Quad MOS
   → Mettre à jour PIN_EV_CANON, PIN_EV_POUMON dans main/gpio_config.h
   → Retirer les deux #warning

2. ⚠️ t_vidange_s
   Chronométrer : depuis EV_POUMON=OFF jusqu'au prochain mouvement bobine
   → Config → Profil machine → T. vidange poumon

3. cycles_par_tour
   Déjà mesuré physiquement = 40 sur cet enrouleur
   → Config → Modes dégradés → Cycles poumon / tour bobine

4. Test vitesse en simulation
   En mode simulation (cycles_par_tour=40) → vitesse ~30 m/h attendue
   Vérifier que l'onglet Stats Session affiche surface, dose, débit non-nuls

5. Test campagne
   Après une session complète → onglet Campagne : surface += session, nb_sessions++
```

---

## 20. Commande build ESP-IDF (PowerShell)

```powershell
$idf_python = "C:\Users\destr\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$idf_path   = "C:\Users\destr\esp\v5.5.2\esp-idf"
$idf_script = "$idf_path\tools\idf.py"
$cmake_dir  = "C:\Users\destr\.espressif\tools\cmake\3.30.2\bin"
$ninja_dir  = "C:\Users\destr\.espressif\tools\ninja\1.12.1"
$xtensa     = "C:\Users\destr\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin"
$env:PATH     = "$xtensa;$cmake_dir;$ninja_dir;$env:PATH"
$env:IDF_PATH = $idf_path
$env:IDF_PYTHON_ENV_PATH = "C:\Users\destr\.espressif\python_env\idf5.5_py3.11_env"
Set-Location "C:\esp\irridoseur"
& $idf_python $idf_script build
```

---

## 21. Portabilité et contribution GitHub

```
Branche : main  |  Dépôt : git@github.com:matfr11/remplacement_irridoseur.git

Pour ajouter un nouvel enrouleur :
  1. Créer main/machines/[nom].c avec machine_profile_t
  2. Ajouter à la liste dans machines.h
  3. Créer main/abaques/[canon].c si nouveau canon
  4. Ouvrir une PR sur GitHub

Paramètres d'adaptation :
  NB_PASTILLES      : gpio_handler.h
  Géométrie bobine  : profil machine
  Mécanique poumon  : t_vidange_s, cycles_par_tour (NVS)
  Table débit       : canon_abaque_t dans abaques/
```

---

*SPECS_FINAL_v3.md — Irrifrance ESP32*
*Installation référence : Irrifrance Structure 1 bis — Ø82mm / 330m / SR 150C — Terrain plat*
*Mise à jour : 2026-05-29 — commit 58d433c*
