---
type: claude-context
updated: 2026-05-31
---

# Régulation — Irrifrance ESP32

## Chaîne de calcul complète

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       CHAÎNE DE CALCUL VITESSE CIBLE                       │
│                                                                             │
│  Inputs NVS :                                                               │
│    prog_pression_bar, prog_buse_mm, prog_dose_mm, prog_largeur_m           │
│    profil machine : r_tambour_vide_m, nb_etages, spires_par_etage          │
│                     d_tuyau_ext_m                                           │
│    config : cycles_par_tour (NVS), facteur_correction                      │
│                                                                             │
│  1. ÉTAGE COURANT                                                           │
│     s_etage = calcul_etage_courant(s_longueur_enroulee, &s_profil)         │
│     → accumule longueur_etage(n) = spires_par_etage × 2π × R_n            │
│        jusqu'à atteindre s_longueur_enroulee                               │
│                                                                             │
│  2. RAYON DE L'ÉTAGE                                                        │
│     R_n = calcul_rayon_etage(s_etage, &s_profil)                           │
│         = r_tambour_vide + (n - 0.5) × d_tuyau_ext_m                      │
│                                                                             │
│  3. DISTANCE PAR CYCLE                                                      │
│     a) Si cycles_par_tour > 0 (mode impulsions connues) :                  │
│          dist_cycle_m = calcul_dist_cycle_m(R_n, cycles_par_tour)          │
│                       = (2π × R_n) / cycles_par_tour                       │
│     b) Sinon (première session) : utilise dist_cycle_nvs                   │
│        Puis auto-calibration via regulation_update_dist_par_cycle()        │
│                                                                             │
│  4. DISTANCE PAR IMPULSION (pour comptage longueur)                        │
│     dist_pulse = calcul_dist_pulse_m(R_n)                                  │
│               = (2π × R_n) / NB_PASTILLES                                  │
│     → mis à jour dans gpio_handler via gpio_handler_set_dist_pulse_m()    │
│                                                                             │
│  5. VITESSE CIBLE                                                           │
│     vitesse_cible = lookup_vitesse_cible(&ABAQUE_SR150C,                   │
│                       prog_pression_bar, prog_buse_mm, prog_dose_mm,       │
│                       &debit_m3h, &p_buse_bar)                             │
│     → Double interpolation :                                                │
│        i.  Trouve 2 entrées abaque les plus proches (p_enrouleur + buse)  │
│        ii. Interpole linéairement entre colonnes Dxx (dose)               │
│        iii. Interpolation pondérée entre les 2 entrées                     │
│                                                                             │
│  6. T_CYCLE_CIBLE (feedforward)                                             │
│     vitesse_cible_m_s = vitesse_cible_m_h / 3600.0                        │
│     T_attente = calcul_t_attente_s(dist_cycle_m, vitesse_cible_m_s,       │
│                   t_remplissage_mesure_s, t_vidange_s,                     │
│                   &alerte_pression_insuff)                                  │
│     = (dist_cycle / vitesse) - t_remplissage - t_vidange                  │
│                                                                             │
│  7. CORRECTION Kp (après n_cycles_calib cycles)                             │
│     si regulation_get_nb_cycles() >= n_cycles_calib :                     │
│       T_attente = correction_vitesse(T_attente, vitesse_reelle,            │
│                     vitesse_cible, kp)                                     │
│       correction = kp × (erreur / vitesse_cible) × T_attente               │
│       erreur = vitesse_cible - vitesse_reelle                              │
│                                                                             │
│  8. CALCULS AGRONOMIQUES (affichage UI)                                    │
│     surface_m2  = calcul_surface_m2(longueur_enroulee, largeur_m)         │
│                 = longueur_enroulee × largeur_m                            │
│     dose_inst   = calcul_dose_inst_mm(debit_m3h, vitesse_m_h, largeur_m)  │
│                 = (debit / (vitesse × largeur)) × 1000                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Séquence d'un cycle poumon (SOUS_VIDANGE → SOUS_ATTENTE → SOUS_REMPLISSAGE)

```
Entrée SOUS_VIDANGE :
  → EV_POUMON=OFF (déjà OFF en fin SOUS_REMPLISSAGE)
  → gpio_reset_impulsions_cycle()   ← raz compteur impulsions
  → Durée = t_vidange_s (NVS, défaut 0.0s)

Entrée SOUS_ATTENTE :
  → EV_POUMON=OFF
  → Durée = s_t_attente_s (calculé feedforward + Kp)

Entrée SOUS_REMPLISSAGE :
  → EV_POUMON=ON
  → s_t_rempl_debut_ms = now_ms()
  → Attend poumon_plein (ou timer t_rempl_fixe_s si mode dégradé B)

Fin SOUS_REMPLISSAGE (poumon_plein détecté) :
  → EV_POUMON=OFF
  → t_remplissage = now_ms() - s_t_rempl_debut_ms
  → nb_impulsions = gpio_get_impulsions()
  → dist_mesuree = nb_impulsions × dist_pulse_m (avec facteur_correction)
  → s_longueur_enroulee += dist_mesuree
  → regulation_update_dist_par_cycle(dist_mesuree) → moyenne glissante
  → Recalcul étage si changé : mise à jour dist_pulse, dist_cycle
  → Recalcul T_attente (feedforward + Kp éventuel)
  → entrer_etat(SOUS_VIDANGE)
```

## Formules avec noms de variables du code

### Rayon étage n
```c
// calculs_mecanique.c : calcul_rayon_etage()
float R_n = profil->r_tambour_vide_m + (n - 0.5f) * profil->d_tuyau_ext_m;
```

### Distance par impulsion capteur vitesse
```c
// calculs_mecanique.c : calcul_dist_pulse_m()
float dist = (2.0f * M_PI * r_etage_m) / (float)NB_PASTILLES;
// Avec facteur_correction appliqué dans state_machine.c
float dist_corrigee = dist * s_cfg_machine.facteur_correction;
```

### Distance par cycle poumon
```c
// calculs_mecanique.c : calcul_dist_cycle_m()
float dist = (2.0f * M_PI * r_etage_m) / cycles_par_tour;
```

### T_attente feedforward
```c
// regulation.c : calcul_t_attente_s()
float t_cycle = dist_par_cycle_m / vitesse_cible_m_s;
float t_attente = t_cycle - t_remplissage_mesure_s - t_vidange_s;
// Guard : t_attente < 0 → 0 + alerte_pression_insuff = true
// Guard : t_attente > 300s → log warning
```

### Correction Kp
```c
// regulation.c : correction_vitesse()
float erreur = vitesse_cible_m_h - vitesse_reelle_m_h;
float correction = kp * (erreur / vitesse_cible_m_h) * t_attente_s;
float t_corrige = t_attente_s + correction;
// Guard : t_corrige < 0 → 0
```

### Vitesse estimée mode dégradé A (capteur vitesse HS)
```c
// state_machine.c, tick EN_COURS
float v_estimee = s_dist_par_cycle_m * cycles_par_min_reel * 60.0f;
// cycles_par_min_reel = nb_cycles_depuis_debut / duree_min
gpio_handler_set_vitesse_estimee(v_estimee);
```

### Dose instantanée
```c
// calculs_hydraulique.c : calcul_dose_inst_mm()
float dose = (debit_m3h / (vitesse_m_h * largeur_m)) * 1000.0f;
```

### Cycles par minute
```c
// regulation.c : calcul_cycles_par_min()
float cpm = (v_m_h / 60.0f) / dist_par_cycle_m;
```

### Vitesse batterie (ADC)
```c
// batterie.c : batterie_lire_voltage()
float v_adc = adc_moyenne_16_samples();           // 0..3.3V
float v_bat = v_adc * (BATT_R1_KOHM + BATT_R2_KOHM) / BATT_R2_KOHM;
// v_bat = v_adc × (100 + 27) / 27 = v_adc × 4.703...
// Plage : 0V → 0.0V, 3.3V → 15.52V (BATT_V_MAX = 14.0V garde)
```

## Paramètres NVS influençant la régulation

| Paramètre NVS | Variable code | Impact |
|---|---|---|
| `t_vidange` | `s_cfg_machine.t_vidange_s` | Soustrait dans calcul T_attente → T_attente réduit |
| `kp` | `s_cfg_machine.kp_regulation` | Amplitude correction proportionnelle vitesse |
| `n_calib` | `s_cfg_machine.n_cycles_calib` | Nb cycles avant activation correction Kp |
| `fenetre_v` | `s_cfg_machine.fenetre_vitesse` | Nb impulsions fenêtre glissante vitesse (gpio_handler) |
| `max_cycles` | `s_cfg_machine.max_cycles_si` | Seuil cycles sans impulsion → vitesse = 0 |
| `dist_cycle` | `s_cfg_machine.dist_cycle_nvs` | Valeur initiale dist/cycle (avant calibration) |
| `facteur_cor` | `s_cfg_machine.facteur_correction` | Multiplié sur dist_pulse_m pour étalonnage |
| `cycles_tour` | `s_cfg_machine.cycles_par_tour` | Si > 0 : dist_cycle calculé depuis géométrie |
| `mode_deg_b` | `s_cfg_machine.mode_deg_poumon` | Timer fixe `t_rempl_fixe_s` à la place du contact |
| `t_rempl_fix` | `s_cfg_machine.t_rempl_fixe_s` | Durée remplissage si mode dégradé B |
| `denivele` | `s_cfg_machine.denivele_m` | [À IMPLÉMENTER] — champ stocké, non utilisé dans calcul |

## Modes dégradés et impact calculs

### Mode dégradé A — capteur vitesse HS (cycles_par_tour > 0)

Activé automatiquement si `s_cfg_machine.cycles_par_tour > 0`.
```c
gpio_handler_set_mode_degrade_a(true);
```
- Vitesse affichée = estimée depuis cycles poumon (pas des impulsions capteur)
- `gpio_get_vitesse_m_h()` retourne `s_vitesse_estimee` au lieu de mesurer les impulsions
- Compteur impulsions toujours actif (pour longueur si capteur OK)
- Alerte UI : aucune (transparent pour opérateur)

### Mode dégradé B — contact poumon HS

Activé si `s_cfg_machine.mode_deg_poumon = true`.
- SOUS_REMPLISSAGE se termine sur timer `t_rempl_fixe_s` au lieu de `poumon_plein`
- Si `t_rempl_fixe_s = 0.0` → risque blocage infini en SOUS_REMPLISSAGE
- Alerte UI : "Mode dégradé : contact poumon" (id `alert-deg-poumon`)

### Bypass SEC-2 — capteur débordement HS

Activé si `s_cfg_machine.mode_deg_spires = true` → `securites_set_bypass_spires(true)`.
- `securites_watchdog()` logue WARN mais ne déclenche pas ARRET_URGENCE
- Alerte UI : bandeau rouge fixe "SECURITE SPIRES DESACTIVEE" (id `alert-deg-spires`)
- **Danger** : risque débordement bobine si capteur physique défaillant

## Auto-calibration dist_par_cycle

```
regulation_reset_calibration()  ←── appelé à chaque entree en VEILLE

Buffer circulaire CALIB_WINDOW = 5 valeurs :
  s_dist_buf[0..4], s_buf_idx, s_buf_count, s_dist_moy

À chaque fin de SOUS_REMPLISSAGE (poumon_plein) :
  1. dist_mesuree = gpio_get_impulsions() × dist_pulse_m × facteur_correction
  2. regulation_update_dist_par_cycle(dist_mesuree)
     → s_dist_buf[s_buf_idx] = dist_mesuree
     → s_dist_moy = moyenne(s_dist_buf[0..s_buf_count-1])
     → s_buf_count++, s_buf_idx = (s_buf_idx+1) % 5

Avant n_cycles_calib cycles : correction Kp inactive
Après n_cycles_calib cycles : correction Kp active sur T_attente

dist_par_cycle sauvée NVS à chaque tick (clé "dist_cycle") pour
continuité en cas de reboot entre deux sessions.
```

## Étalonnage longueur (facteur_correction)

```c
// calculs_mecanique.c : calcul_facteur_etalonnage()
// Appelé via cmd "etalonner" depuis WebSocket

// Conditions de validation :
//   C1 : nb_impulsions > 50
//   C2 : 0.5 < facteur < 2.0
//   C3 : |facteur - 1.0| < 0.30

float facteur = longueur_reelle_m / longueur_theorique_m;
// Si toutes conditions OK : facteur_out = facteur, retourne true
// Sinon : retourne false, facteur inchangé
```

Après validation, `facteur_correction` est sauvé en NVS (clé `facteur_cor`) et rechargé
dans `s_cfg_machine.facteur_correction`. Appliqué dès le cycle suivant sur `dist_pulse_m`.
