# SPECS_MISEAJOUR — Irrifrance ESP32
## Corrections et compléments au SPECS.md initial

> Ce document complète et corrige SPECS.md.
> En cas de contradiction, SPECS_MISEAJOUR.md prime.
> Créé après analyse des documents techniques : schéma bornier,
> fiche technique Structure 2 AT/P, tableau valeurs théoriques par structure.

---

## MàJ 1 — Paramètres machine maintenant connus (plus de ⚠️)

Ces paramètres étaient marqués "À mesurer" dans SPECS.md.
Ils sont maintenant calculés depuis la fiche technique constructeur.

### Source : Fiche technique Structure 2 AT/P (Ø82 - 330m)

| Paramètre NVS | Ancienne valeur | Nouvelle valeur | Source |
|---|---|---|---|
| `longueur_tuyau` | 330.0 | **330.0 m** | Fiche technique ✅ |
| `diam_int_tuyau` | 0.082 | **0.082 m** | Fiche technique ✅ |
| `d_tuyau_ext` | 0.0 ⚠️ | **0.094 m** | Calculé : 82 + 2×6mm ✅ |
| `r_tambour_vide` | 0.0 ⚠️ | **0.648 m** | Calculé (voir ci-dessous) ✅ |
| `nb_etages` | 4 | **4 couches** | Tableau spires ST.1 Bis ✅ |
| `nb_pastilles` | 10 | **10** | Confirmé terrain ✅ |

### Calcul r_tambour_vide
```
Source tableau spires ST.1 Bis, ligne Ø82-290 :
  Longueur spire dernière couche = 6.14 m
  → R_couche4 = 6.14 / (2π) = 0.977 m

  d_tuyau_ext = 82mm + 2×6mm = 94mm = 0.094 m
  R_tambour_vide = R_couche4 - (3.5 × d_tuyau_ext)
                 = 0.977 - (3.5 × 0.094)
                 = 0.977 - 0.329
                 = 0.648 m ✅
```

### Paramètres toujours à mesurer terrain

| Paramètre | Comment mesurer |
|---|---|
| `t_vidange_poumon_s` | Chronométrer depuis EV2=OFF jusqu'au prochain mouvement bobine détecté par capteur vitesse |
| Sens contacteurs | Multimètre continuité au repos sur chaque contact |

---

## MàJ 2 — Table débit : remplacer abaque UASA46 par table constructeur

### Suppression
~~Table abaque UASA46 5×15 valeurs embarquée~~
~~Interpolation bilinéaire pression/buse~~

### Remplacement
La **fiche technique constructeur** (Irrifrance, Structure 2 AT/P) fournit
une table spécifique à la machine, bien plus précise que l'abaque générique.

Elle donne directement pour chaque combinaison pression borne + buse :
- Le débit Q (m³/h)
- La pression au canon (bar)
- L'espacement recommandé (m)
- La vitesse d'avancement (m/h) pour chaque dose cible

### Table constructeur à embarquer

```c
/**
 * Table constructeur Irrifrance — Structure 2 AT/P
 * Ø82mm intérieur — épaisseur 6mm — 330m — Arroseur SR 150C
 * Source : fiche technique fournie par l'utilisateur
 *
 * Colonnes : P_borne(bar), Q(m3/h), P_canon(bar), buse(mm),
 *            esp(m), v_40mm, v_30mm, v_25mm, v_20mm, v_15mm
 * Vitesses en m/h pour chaque dose cible (mm)
 */
typedef struct {
    float p_borne_bar;
    float q_m3h;
    float p_canon_bar;
    float buse_mm;
    float esp_m;
    float v_40mm;   // vitesse m/h pour dose 40mm
    float v_30mm;
    float v_25mm;
    float v_20mm;
    float v_15mm;
} canon_entry_t;

static const canon_entry_t CANON_TABLE[] = {
 // p_borne  Q      p_can  buse   esp   v40    v30    v25    v20    v15
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
};
#define CANON_TABLE_SIZE 13
```

### Nouvelle logique de lookup

```c
/**
 * Recherche dans la table constructeur.
 *
 * Entrées utilisateur : p_borne_bar + buse_mm + dose_mm
 *
 * Algorithme :
 *   1. Trouver la ligne avec p_borne et buse les plus proches
 *      (distance euclidienne normalisée dans l'espace p_borne/buse)
 *   2. Interpoler entre les colonnes dose pour la dose cible
 *   3. Retourner vitesse_cible_m_h et debit_m3h
 *
 * Note : la table couvre buses 17.3 à 25.4mm et pressions 4.9 à 9.5 bar.
 * Hors plage → warning log, utiliser la ligne la plus proche.
 *
 * Conversion pouces → mm disponible :
 *   0.60" = 15.2mm / 0.70" = 17.8mm / 0.80" = 20.3mm
 *   0.90" = 22.9mm / 1.00" = 25.4mm
 */
float lookup_vitesse_cible(float p_borne_bar,
                            float buse_mm,
                            float dose_mm,
                            float *debit_m3h_out);
```

### Suppression des fonctions devenues inutiles
Les fonctions suivantes de `calculs_hydraulique.c` sont **supprimées** :
- ~~`calcul_pression_canon()`~~ → la table donne directement P_canon
- ~~`calcul_debit_m3h()`~~ → remplacé par `lookup_vitesse_cible()`
- ~~`DEBIT_TABLE_M3H[5][15]`~~ → remplacé par `CANON_TABLE[]`
- ~~`calcul_vitesse_cible_m_min()`~~ → vitesse lue directement dans la table

La fonction `calcul_pression_canon()` peut être **conservée** à titre indicatif
pour l'affichage de P_canon dans l'onglet Stats, mais n'est plus dans la
boucle de régulation.

---

## MàJ 3 — Révision complète de la régulation poumon

### Ancien modèle (SPECS.md) — REMPLACÉ
~~Fréquence poumon = vitesse_cible / dist_poumon_etage~~
~~Pilotage EV2 en cycles avec T_off calculé~~

### Nouveau modèle — Feedforward + correction

#### Mécanique réelle clarifiée
```
Un cycle poumon complet :

  Phase 1 — REMPLISSAGE (durée variable)
    EV2 = ON
    Le poumon se remplit (plus ou moins vite selon pression)
    S'arrête quand contact_poumon_plein détecté (actif bas)
    Durée mesurée = T_remplissage (ms) — chronométrée par l'ESP32

  Phase 2 — VIDANGE (durée fixe — mécanique pure)
    EV2 = OFF
    Le poumon se vide, actionne le cliquet → bobine tourne
    Durée fixe = T_vidange_s (paramètre NVS — à mesurer)
    Le capteur 10 pastilles détecte le mouvement

  Phase 3 — ATTENTE RÉGULATION (durée variable)
    EV2 = OFF (pause)
    Durée = T_attente calculée par l'ESP32
    T_attente = 0 → vitesse maximale
    T_attente > 0 → ralentissement
```

#### Distance par cycle — auto-calibration
```c
/**
 * La distance parcourue par cycle poumon est mesurée directement
 * depuis le capteur 10 pastilles pendant la phase de vidange.
 *
 * dist_par_cycle_m = nb_impulsions_phase_vidange × dist_par_pulse_m
 * dist_par_pulse_m = (2π × R_etage_courant) / NB_PASTILLES
 *
 * Moyenne glissante sur 5 cycles pour stabiliser la mesure.
 * → Pas besoin de saisir l'angle α du cliquet !
 * → L'ESP32 s'auto-calibre automatiquement.
 * → Le réglage 1 ou 2 crans est transparent pour le logiciel.
 */
```

#### Calcul T_attente — régulation feedforward
```c
/**
 * Calcul direct de T_attente (feedforward).
 *
 * vitesse_cible_m_s = vitesse_cible_m_h / 3600.0f
 * T_cycle_cible_s   = dist_par_cycle_m / vitesse_cible_m_s
 * T_attente_s       = T_cycle_cible_s
 *                   - T_remplissage_mesure_s   (mesuré ce cycle)
 *                   - T_vidange_s              (paramètre NVS)
 *
 * Guards :
 *   T_attente < 0    → vitesse max insuffisante
 *                    → T_attente = 0
 *                    → alerte web UI "Pression insuffisante
 *                       pour la dose programmée"
 *   T_attente > 300s → anomalie (vitesse cible trop faible)
 *                    → log warning
 */
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s);
```

#### Correction par capteur vitesse (boucle fermée)
```c
/**
 * Correction fine sur T_attente depuis la vitesse réelle mesurée.
 * S'applique après N cycles d'auto-calibration (N=3 par défaut).
 *
 * erreur_m_h  = vitesse_reelle_m_h - vitesse_cible_m_h
 * correction  = erreur_m_h × Kp    (Kp proportionnel, à tuner terrain)
 * T_attente  += correction
 *
 * Rôle : compenser les petites dérives mécaniques,
 *        détecter les anomalies (blocage, glissement, capteur HS)
 *
 * Kp par défaut : 0.1 (conservateur — ajustable NVS)
 */
```

---

## MàJ 4 — Suppression paramètre alpha_poumon du NVS

### Ancien NVS (SPECS.md) — MODIFIÉ
~~`alpha_poumon` (float) — Angle rotation/cycle poumon (°)~~

### Nouveau NVS
| Clé NVS | Type | Défaut | Description |
|---|---|---|---|
| `t_vidange_s` | float | 0.0 ⚠️ | Durée vidange mécanique poumon (s) — à mesurer |
| `kp_regulation` | float | 0.1 | Gain proportionnel correction vitesse |
| `n_cycles_calib` | int | 3 | Nb cycles avant activation correction |

`alpha_poumon` est **supprimé** — la distance par cycle est mesurée
automatiquement par le capteur 10 pastilles.

---

## MàJ 5 — Câblage bornier (schéma officiel)

Le schéma "Branchement Bornier" confirme et précise le câblage.
Correspondance bornes → GPIO ESP32 :

| Bornes | Signal | Câble n° | GPIO ESP32 |
|---|---|---|---|
| 1-2-3 | Batterie 12V + diode BY255 | 10 | Alimentation (pas GPIO) |
| 7-8-9 | EV2 — Poumon | 2 | GPIO 26 |
| 10-11-12 | EV1 — Vanne canon | 1 | GPIO 25 |
| 15-16 | Sécurité enroulement | 5 | GPIO 32 |
| 17-18 | Fin d'enroulement (fin de course canon) | 4 | GPIO 35 |
| 19-20 | Détecteur disque bobine (10 pastilles) | 8 | GPIO 34 |
| 21-22 | Capteur de pression (pressostat) | 6 | GPIO 27 |
| 23-24 | Détecteur vitesse réducteur | 9 | **Non branché** sur ST.1 Bis |

> Bornes 4-5-6 (EV3) et 13-14 (capteur pluie option) : absents sur cette machine.
> Bornes 23-24 : présentes sur machines à turbine uniquement — non utilisées.

### Contact poumon plein
Le schéma bornier ne montre pas de borne dédiée au contact poumon plein.
Ce contact est câblé **en série avec EV2** dans le circuit hydraulique original
ou remonte via un câble non numéroté.

**Action avant codage** : vérifier sur la machine si le micro-switch poumon
a un câble indépendant vers le boîtier ou s'il est intégré au circuit EV2.

---

## MàJ 6 — Machine d'états — ajout phase vidange

La séquence `ETAT_EN_COURS` est affinée pour le nouveau modèle poumon :

```
ETAT_EN_COURS — sous-états internes du cycle poumon :

  SOUS_ETAT_REMPLISSAGE
    t_start = timestamp EV2=ON
    EV2 = ON
    Attend contact_poumon_plein
    → détecté : t_remplissage = now - t_start
                EV2 = OFF
                → SOUS_ETAT_VIDANGE

  SOUS_ETAT_VIDANGE
    t_start = timestamp EV2=OFF
    EV2 = OFF (mécanique)
    Compte impulsions capteur vitesse
    Attend T_vidange_s (NVS)
    → écoulé : dist_cycle = impulsions × dist_pulse
               mise à jour moyenne glissante dist_par_cycle
               calcul T_attente feedforward
               → SOUS_ETAT_ATTENTE

  SOUS_ETAT_ATTENTE
    EV2 = OFF
    Attend T_attente_s calculé
    Applique correction Kp si N cycles ≥ n_cycles_calib
    → écoulé : → SOUS_ETAT_REMPLISSAGE (nouveau cycle)
```

---

## MàJ 7 — Web UI — ajout affichage onglet Stats

Nouveaux éléments à afficher dans l'onglet Stats :

```
T_remplissage dernier cycle : xx.x s   ← indicateur santé pression
T_vidange mécanique         : xx.x s   ← depuis NVS
T_attente calculé           : xx.x s   ← régulation
Cycles poumon session       : xxxx     ← compteur
Dist/cycle mesurée          : x.xx m   ← auto-calibration
Alerte pression             : si T_attente = 0 → "⚠️ Pression insuffisante"
```

---

## MàJ 8 — Nouvelles fonctions calculs_mecanique.c

Remplacent ou complètent les fonctions du SPECS.md initial :

```c
// SUPPRIMÉE :
// float calcul_freq_poumon(float vitesse_cible, float dist_poumon);

// AJOUTÉES :

/**
 * Mise à jour de la moyenne glissante de dist_par_cycle.
 * Appelée à chaque fin de phase VIDANGE.
 * Window = 5 cycles.
 */
void update_dist_par_cycle(float nouvelle_mesure_m);
float get_dist_par_cycle_m(void);

/**
 * Calcul T_attente par feedforward.
 * Retourne 0.0f si vitesse max insuffisante (+ set flag alerte).
 */
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_s,
                          float t_vidange_s);

/**
 * Correction proportionnelle sur T_attente.
 * Appelée après n_cycles_calib cycles.
 */
float correction_vitesse(float t_attente_actuel_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp);
```

---

## MàJ 9 — NVS namespace irri_machine — liste complète corrigée

| Clé NVS | Type | Valeur | Statut |
|---|---|---|---|
| `longueur_tuyau` | float | **330.0** | ✅ Connu |
| `diam_int_tuyau` | float | **0.082** | ✅ Connu |
| `d_tuyau_ext` | float | **0.094** | ✅ Calculé |
| `r_tambour_vide` | float | **0.648** | ✅ Calculé |
| `nb_etages` | int | **4** | ✅ Connu |
| `t_vidange_s` | float | **0.0** | ⚠️ À mesurer |
| `kp_regulation` | float | **0.1** | Défaut terrain |
| `n_cycles_calib` | int | **3** | Défaut |
| `perte_enrouleur` | float | **2.5** | Indicatif (affichage P_canon) |
| `denivele_m` | float | **0.0** | Terrain plat — portabilité |
| ~~`alpha_poumon`~~ | ~~float~~ | ~~supprimé~~ | ❌ Auto-calibré |
| ~~`duree_rempl_s`~~ | ~~float~~ | ~~supprimé~~ | ❌ Mesuré en temps réel |

---

## MàJ 10 — Plan de développement — PRs impactées

Les PRs suivantes du SPECS.md initial sont modifiées :

| PR | Changement |
|---|---|
| **PR-03** | Remplacer table UASA46 par table constructeur + fonction `lookup_vitesse_cible()` |
| **PR-04** | Supprimer `calcul_freq_poumon()`, ajouter `calcul_t_attente_s()`, `update_dist_par_cycle()`, `correction_vitesse()` |
| **PR-05** | Ajouter sous-états REMPLISSAGE/VIDANGE/ATTENTE dans ETAT_EN_COURS, chronométrage T_remplissage |
| **PR-06** | Retirer `alpha_poumon` du NVS, ajouter `t_vidange_s`, `kp_regulation`, `n_cycles_calib` |
| **PR-08** | Ajouter T_remplissage, T_attente, dist/cycle, alerte pression dans onglet Stats |

PRs **non impactées** : PR-01, PR-02, PR-07, PR-09, PR-10.

---

## Action requise avant PR-02 — vérification terrain

Avant de coder le module poumon, vérifier :

1. **Contact poumon plein** — câble indépendant vers bornier ou intégré EV2 ?
2. **Sens de tous les contacteurs** — NO ou NC au repos (multimètre continuité)
3. **T_vidange poumon** — chronométrer depuis EV2=OFF jusqu'à détection mouvement capteur vitesse

---

*SPECS_MISEAJOUR.md — v1.0*
*Basé sur : schéma bornier, fiche technique Structure 2 AT/P,*
*tableau valeurs théoriques ST.1 Bis, discussion régulation poumon*
