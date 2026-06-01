---
type: claude-context
updated: 2026-05-31
---

# Machine d'états — Irrifrance ESP32

## Diagramme ASCII complet

```
                    ┌──────────────────────────────────────────────┐
                    │               ETAT_VEILLE (0)                │
                    │  EV_CANON=OFF EV_POUMON=OFF                  │
                    │  Surveille : cfg_valide, fin_course, deroule  │
                    └──────┬───────────────────────────────────────┘
                           │ C1+C2+C3+C4 (cmd_start)
                           │  C1 : cfg_valide=true
                           │  C2 : pression_ok=true
                           │  C3 : fin_course=false
                           │  C4 : s_demarrage_autorise=true
                           ▼
                    ┌──────────────────────────────────────────────┐
                    │          ETAT_OUVERTURE_CANON (1)            │
                    │  EV_CANON=ON EV_POUMON=OFF                   │
                    │  Attend pression stable 3s                   │
                    │  Timeout 20s → ARRET_URGENCE                 │
                    └──────┬───────────────────────────────────────┘
                           │ pression OK ≥ 3s consécutives
              ┌────────────┴───────────┐
              │ tempo_depart_on=true   │ tempo_depart_on=false
              ▼                        ▼
  ┌─────────────────────────┐  ┌──────────────────────────────────────┐
  │   ETAT_TEMPO_DEPART (2) │  │   ETAT_REMPLISSAGE_POUMON (3)        │
  │  EV_CANON=ON            │  │   EV_CANON=ON EV_POUMON=ON          │
  │  Arrosage sur place      │  │   Attend poumon_plein                │
  │  Durée = tempo_depart_s  │  │   Mode dégradé B : timer fixe        │
  └─────────┬───────────────┘  └──────────┬───────────────────────────┘
            │ timeout                      │ poumon_plein OU timer
            └──────────────┬──────────────┘
                           ▼
               ┌───────────────────────────────────────────────────┐
               │              ETAT_EN_COURS (4)                    │
               │  EV_CANON=ON   Régulation active                  │
               │                                                   │
               │  Sous-état SOUS_VIDANGE :                         │
               │    EV_POUMON=OFF, ressort rétracte cliquet        │
               │    Durée = t_vidange_s                            │
               │                                                   │
               │  Sous-état SOUS_ATTENTE :                         │
               │    EV_POUMON=OFF, pause calculée                  │
               │    Durée = T_attente (feedforward + Kp)           │
               │                                                   │
               │  Sous-état SOUS_REMPLISSAGE :                     │
               │    EV_POUMON=ON, cliquet avance bobine            │
               │    Impulsions comptées ici                        │
               │    Fin : poumon_plein OU timer fixe (mode B)      │
               └──────┬────────────────────────────┬──────────────┘
                      │ fin_course                 │ pression perdue
              ┌───────┴───────────┐     ┌──────────▼─────────────────────────┐
              │ tempo_arrivee_on  │     │       ETAT_PAUSE_PRESSION (5)       │
              ▼         ▼        │     │   EV_CANON=OFF EV_POUMON=OFF        │
      ┌────────────┐   ARRET_    │     │   Attend retour pression            │
      │TEMPO_ARR.  │   FINAL     │     └──────────┬─────────────────────────┘
      │  (6)       │   (7)       │               │ pression_ok
      │EV_CANON=ON │             │               ▼
      │EV_POUMON   │             │          ETAT_EN_COURS (reprise)
      │=OFF        │             │
      └──────┬─────┘             │
             │ timeout           │
             ▼                  │
      ETAT_ARRET_FINAL (7) ◄────┘
      EV=OFF — bilan session
      Attend fin_course LOW → VEILLE
             │ ou cmd_reset ou cmd_resume
             ▼
      ETAT_VEILLE (0)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  ETAT_ARRET_URGENCE (8) ◄── SEC-1 ou SEC-2 (depuis tout état sauf TEMPO_ARR, ARRET_*)
  EV=OFF — s_demarrage_autorise=false
  Raison stockée NVS
       │
       ├── cmd_reset  → reset longueurs → VEILLE
       ├── cmd_resume → longueurs préservées → VEILLE
       │    ⚠ bloqué si raison="Debordement bobine" ET secu_spires encore HIGH
       └── 3-tap poumon_plein → longueurs préservées → VEILLE
            ⚠ bloqué si debordement encore actif

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  ETAT_DEROULE (9) ◄── flanc descendant fin_course en VEILLE
                   ◄── cmd_start_deroule
  EV=OFF — mesure pastilles — compteur impulsions
       │ fin_course LOW (pression ET fin_course libérés)
       ▼
  ETAT_VEILLE — longueur déroulée sauvegardée NVS

```

## Transitions — conditions exactes

### VEILLE → OUVERTURE_CANON

```c
// Condition dans tick ETAT_VEILLE
if (s_demarrage_autorise &&
    s_status.cfg_valide &&
    e.pression_ok &&
    !e.fin_course) {
    s_demarrage_autorise = false;
    gpio_ev_canon_set(true);
    entrer_etat(ETAT_OUVERTURE_CANON);
}
```

### VEILLE → DEROULE

```c
// Flanc descendant fin_course détecté en VEILLE
if (s_fc_deroule_prev && !e.fin_course) {
    gpio_reset_impulsions_cycle();
    entrer_etat(ETAT_DEROULE);
}
// OU cmd_start_deroule depuis WebSocket (VEILLE uniquement)
// OU state_machine_sim_force_deroule() en CONFIG_IRRI_TEST_MODE (tout état)
```

**Bug fix** : `state_machine_cmd_set_longueur()` synchronise `s_mesure_deroule_m`
quand appelé en ETAT_DEROULE, pour que la transition DEROULE→OUVERTURE_CANON
utilise la valeur corrigée (et non 0 si aucune pastille n'avait été comptée).

### OUVERTURE_CANON → TEMPO_DEPART ou REMPLISSAGE_POUMON

```c
// 3 ticks consécutifs pression OK (300ms)
if (s_pression_stable_count >= 3) {
    if (s_cfg_prog.tempo_depart_on && s_cfg_prog.tempo_depart_s > 0)
        entrer_etat(ETAT_TEMPO_DEPART);
    else
        entrer_etat(ETAT_REMPLISSAGE_POUMON);
}
// Timeout 20s (200 ticks) → ARRET_URGENCE "Timeout ouverture canon"
```

### EN_COURS → PAUSE_PRESSION

```c
// Pression perdue dans EN_COURS, TEMPO_DEPART, REMPLISSAGE_POUMON
if (!e.pression_ok) {
    gpio_ev_canon_set(false);
    gpio_ev_poumon_set(false);
    entrer_etat(ETAT_PAUSE_PRESSION);
}
```

### EN_COURS → fin de session

```c
if (e.fin_course) {
    gpio_ev_poumon_set(false);
    if (s_cfg_prog.tempo_arrivee_on && s_cfg_prog.tempo_arrivee_s > 0)
        entrer_etat(ETAT_TEMPO_ARRIVEE);
    else {
        gpio_ev_canon_set(false);
        entrer_etat(ETAT_ARRET_FINAL);
    }
}
```

### ARRET_FINAL → VEILLE

```c
// Automatique : fin_course passe LOW (opérateur redéploie)
if (!e.fin_course) {
    entrer_etat(ETAT_VEILLE);
}
// OU cmd_reset / cmd_resume
```

## Comportement des sécurités par état (tableau)

| État | SEC-1 (fin_course) | SEC-2 (secu_spires) | SEC-P (pression) |
|---|---|---|---|
| VEILLE | Ignorée (déclenche DEROULE si flanc ↓) | **URGENCE** | Ignorée |
| OUVERTURE_CANON | **URGENCE** | **URGENCE** | Si pas OK après 20s → URGENCE |
| TEMPO_DEPART | **URGENCE** | **URGENCE** | → PAUSE_PRESSION |
| REMPLISSAGE_POUMON | **URGENCE** | **URGENCE** | → PAUSE_PRESSION |
| EN_COURS | → TEMPO_ARRIVEE ou ARRET_FINAL (normal) | **URGENCE** | → PAUSE_PRESSION |
| PAUSE_PRESSION | **URGENCE** | **URGENCE** | Attend retour → EN_COURS |
| TEMPO_ARRIVEE | Ignorée (fin de course normal ici) | **URGENCE** | Ignorée |
| ARRET_FINAL | Ignorée | **URGENCE** | Ignorée |
| ARRET_URGENCE | Ignorée (`sec1_applicable=false`) | **URGENCE** (idempotent) | Ignorée |
| DEROULE | Ignorée | **URGENCE** | Ignorée |

**SEC-2 priorité absolue** : tire `gpio_all_ev_off()` + `state_machine_declencher_urgence()` dans tous les états sans exception (sauf si `s_bypass_spires=true`).

## Variables d'état internes (state_machine.c)

| Variable | Type | Fichier | Rôle |
|---|---|---|---|
| `s_etat` | `etat_machine_t` | state_machine.c | État courant |
| `s_sous_etat` | `sous_etat_poumon_t` | state_machine.c | Sous-état cycle poumon |
| `s_status` | `machine_status_t` | state_machine.c | Struct statut diffusé WS |
| `s_cfg_machine` | `config_machine_t` | state_machine.c | Config machine courante |
| `s_cfg_prog` | `config_programme_t` | state_machine.c | Programme actif |
| `s_profil` | `machine_profile_t` | state_machine.c | Profil machine actif |
| `s_abaque` | `const canon_abaque_t*` | state_machine.c | Abaque canon actif |
| `s_longueur_enroulee` | `float` | state_machine.c | Longueur enroulée depuis départ session (m) |
| `s_longueur_session_m` | `float` | state_machine.c | Longueur enroulée totale session (m) |
| `s_longueur_deroule_m` | `float` | state_machine.c | Longueur déployée terrain (m) |
| `s_longueur_derniere_nvs` | `float` | state_machine.c | Dernière valeur sauvée NVS (évite usure flash) |
| `s_demarrage_autorise` | `bool` | state_machine.c | Opérateur a appuyé START |
| `s_vitesse_cible_m_h` | `float` | state_machine.c | Vitesse cible lookup abaque |
| `s_dist_par_cycle_m` | `float` | state_machine.c | Distance par cycle courant (calibrée) |
| `s_t_rempl_debut_ms` | `int64_t` | state_machine.c | Timestamp début SOUS_REMPLISSAGE |
| `s_t_attente_s` | `float` | state_machine.c | T_attente calculé feedforward |
| `s_pression_stable_count` | `int` | state_machine.c | Compteur ticks pression stable (OUVERTURE_CANON) |
| `s_etage_courant` | `int` | state_machine.c | Étage bobine courant (1..nb_etages) |
| `s_etage_precedent` | `int` | state_machine.c | Étage précédent (détection changement) |
| `s_batt` | `batt_status_t` | state_machine.c | Statut batterie (rafraîchi /300 ticks) |
| `s_batt_tick` | `int` | state_machine.c | Compteur ticks batterie |
| `s_mutex` | `SemaphoreHandle_t` | state_machine.c | Mutex récursif (protège tout) |
| `s_fc_deroule_prev` | `bool` | state_machine.c | État fin_course tick précédent (détection flanc) |
| `s_mesure_deroule_m` | `float` | state_machine.c | Longueur mesurée en ETAT_DEROULE |
| `s_bilan_envoye` | `bool` | state_machine.c | Bilan session déjà envoyé (guard double envoi) |
| `s_duree_pause_ms` | `int64_t` | state_machine.c | Durée cumulée PAUSE_PRESSION |
| `s_session` | `session_summary_t` | state_machine.c | Bilan dernière session |
| `s_stats` | `config_stats_t` | state_machine.c | Stats campagne (cache NVS) |
| `s_bypass_spires` | `bool` | securites.c | Bypass SEC-2 spires |

## Réarmement physique 3-tap

Mécanisme indépendant de l'UI WebSocket, opérable terrain sans smartphone.

**Déclencheur** : 3 fronts montants sur `PIN_POUMON_PLEIN` (GPIO 33) en moins de 1.5s,
avec `gpio_get_level(PIN_EV_POUMON) == LOW` (EV_POUMON OFF).

**Comportement selon état** :

| État courant | Condition supplémentaire | Action |
|---|---|---|
| `ETAT_ARRET_URGENCE` | raison="Debordement bobine" ET `secu_spires=HIGH` | **Ignoré** (log WARN) |
| `ETAT_ARRET_URGENCE` | autre cas | REPRENDRE : longueurs préservées → VEILLE, `s_demarrage_autorise=true` |
| Autre état | — | `s_demarrage_autorise=true` seulement (pas de transition) |

**Variables internes 3-tap** (statiques locales dans `tick_state_machine()`) :
- `s_rearm_count` : compteur appuis (reset si timeout)
- `s_rearm_prev` : état précédent poumon_plein (détection front montant)
- `s_rearm_last_ms` : timestamp dernier appui (timeout 1500ms)
