# API WebSocket — Irrifrance ESP32

L'ESP32 expose un WebSocket sur `ws://192.168.4.1/ws`. L'UI web s'y connecte automatiquement.
Le protocole est JSON text (pas binaire).

---

## Messages ESP32 → navigateur (statut machine)

Diffusé toutes les **500ms** à tous les clients connectés.

### Objet complet annoté

```json
{
  // ── État machine ──────────────────────────────────────────────────────────
  "etat": "EN_COURS",       // string, voir tableau états ci-dessous
  "etat_code": 4,           // int 0-9, même signification
  "raison_arret": "",       // string, vide sauf si ARRET_URGENCE

  // ── Identité session ──────────────────────────────────────────────────────
  "prog_nom": "P1 Mais",    // string max 20 chars
  "machine_nom": "ST1 Bis Ø82-330m",
  "abaque_nom": "SR 150C",

  // ── Programme actif ───────────────────────────────────────────────────────
  "prog_dose_mm": 25.0,           // float, mm
  "prog_largeur_m": 66.0,         // float, m
  "prog_buse_mm": 20,             // int, mm
  "prog_pression_bar": 6.5,       // float, bar
  "prog_tempo_depart_on": false,  // bool
  "prog_tempo_depart_s": 0,       // int, secondes
  "prog_tempo_arrivee_on": false, // bool
  "prog_tempo_arrivee_s": 0,      // int, secondes

  // ── Longueurs ─────────────────────────────────────────────────────────────
  "longueur_deroulee_m": 330.0,   // float, longueur déployée (mesurée en DEROULE)
  "longueur_enroulee_m": 45.2,    // float, enroulé depuis départ session

  // ── Vitesse ───────────────────────────────────────────────────────────────
  "vitesse_m_h": 31.4,            // float, vitesse mesurée (0 si arrêt)
  "vitesse_cible_m_h": 32.1,      // float, vitesse visée (lookup abaque)

  // ── Temps ─────────────────────────────────────────────────────────────────
  "duree_s": 5220,                // int, durée session en cours (s)
  "heure_arrivee_unix": 1748606400, // int64, timestamp Unix prévu (0 si inconnu)
  "heure_arrivee_relative_s": 4200, // int, secondes restantes avant arrivée
  "heure_synchro": true,          // bool, true si horloge ESP32 synchronisée

  // ── Agronomie ─────────────────────────────────────────────────────────────
  "surface_m2": 2983,             // float, surface arrosée session (m²)
  "dose_inst_mm": 24.8,           // float, dose instantanée (mm)
  "debit_m3h": 31.9,              // float, débit (m³/h)
  "p_enrouleur_bar": 6.5,         // float, pression manomètre (bar)
  "p_buse_bar": 4.0,              // float, pression effective buse (bar)

  // ── Mécanique bobine ──────────────────────────────────────────────────────
  "etage_courant": 2,             // int, 1..nb_etages
  "nb_etages": 5,                 // int

  // ── GPIO temps réel ───────────────────────────────────────────────────────
  "ev_canon": true,               // bool, true = EV ouverte
  "ev_poumon": false,             // bool
  "pression_ok": true,            // bool, true = pression présente
  "fin_course": false,            // bool, true = canon en position (rentré)
  "secu_spires": false,           // bool, true = débordement bobine actif
  "poumon_plein": true,           // bool, true = poumon plein

  // ── Mesure déroulement ────────────────────────────────────────────────────
  "mesure_deroule_m": 0.0,        // float, valide uniquement si etat="DEROULE"

  // ── Régulation ───────────────────────────────────────────────────────────
  "t_remplissage_ms": 4200,       // int, durée dernier remplissage (ms)
  "t_attente_ms": 8100,           // int, durée attente calculée (ms)
  "dist_par_cycle_m": 0.341,      // float, distance par cycle auto-calibrée (m)
  "cycles_par_min_cible": 1.57,   // float
  "cycles_par_min_reel": 1.52,    // float
  "cycles_total": 132,            // int, cycles depuis début session

  // ── Alertes / modes dégradés ──────────────────────────────────────────────
  "alerte_pression_insuff": false, // bool, T_attente négatif → vitesse trop haute
  "mode_degrade_poumon": false,    // bool, remplissage temporisé (contact HS)
  "mode_degrade_spires": false,    // bool, bypass SEC-2 (capteur spires HS)
  "facteur_correction": 1.0,       // float, étalonnage longueur

  // ── Stats campagne (cumulatif saison) ─────────────────────────────────────
  "camp_surface_ha": 1.24,        // float
  "camp_volume_m3": 310.0,        // float
  "camp_dose_moy_mm": 25.0,       // float
  "camp_vitesse_moy_m_h": 31.0,   // float
  "camp_nb_sessions": 5,          // int
  "camp_duree_h": 12.3,           // float

  // ── Batterie ──────────────────────────────────────────────────────────────
  "batterie_v": 12.5,             // float, tension mesurée (V)
  "batterie_pct": 83,             // int, indicatif 0-100%
  "batterie_etat": "CORRECTE",    // string, voir tableau états batterie
  "batterie_couleur": "#4caf50",  // string, couleur CSS pour l'UI
  "cfg_batt_warn_v": 11.5,        // float, seuil alerte configurable (V)
  "cfg_batt_crit_v": 11.0,        // float, seuil critique configurable (V)

  // ── Config (pour initialiser l'UI Config) ─────────────────────────────────
  "cfg_valide": true,             // bool, programme valide (dose/largeur/buse/pression > 0)
  "cfg_t_vidange_s": 0.0,
  "cfg_kp_regulation": 0.1,
  "cfg_n_cycles_calib": 3,
  "cfg_t_rempl_fixe_s": 0.0,
  "cfg_denivele_m": 0.0,
  "cfg_machine_active": 0,
  "cfg_abaque_idx": 1,
  "cfg_cycles_par_tour": 40.0,

  // ── Robustesse (PR-14) ────────────────────────────────────────────────────
  "coupure_detectee": false,      // bool, true si session interrompue par coupure de courant
  "cfg_heartbeat_rc_on": false,   // bool, heartbeat GPIO 23 (LED carte) pour circuit RC (défaut OFF)

  // ── Fin de course (PR-15) ─────────────────────────────────────────────────
  "cfg_fin_course_seuil_m": 10.0, // float, longueur restante sous laquelle fin_course = fin normale

  // ── Alerte dose trop basse (PR-16) ───────────────────────────────────────
  "vitesse_max_m_h": 0.0,         // float, vitesse max physique si T_attente < 0 (0 sinon)
  "dose_corrigee_mm": 0.0,        // float, dose réelle à vitesse_max (0 sinon)

  // ── Surveillance MOSFETs (PR-19) ──────────────────────────────────────────
  "mosfet_canon_secours": false,  // bool, true si EV_CANON basculée sur MOSFET secours (OUT3)
  "mosfet_poumon_secours": false, // bool, true si EV_POUMON basculée sur MOSFET secours (OUT4)
  "mosfet_canon_etat": "OK",      // string, état du MOSFET principal EV_CANON (voir tableau)
  "mosfet_poumon_etat": "OK"      // string, état du MOSFET principal EV_POUMON (voir tableau)
}
```

### Codes `etat` / `etat_code`

| `etat_code` | `etat` | Description |
|---|---|---|
| 0 | `VEILLE` | Attente démarrage opérateur |
| 1 | `OUVERTURE_CANON` | EV_CANON ouverte, attend pression stable 3s |
| 2 | `TEMPO_DEPART` | Arrosage sur place avant enroulement |
| 3 | `REMPLISSAGE_POUMON` | EV_POUMON ouverte, attend poumon plein |
| 4 | `EN_COURS` | Régulation active — bobine s'enroule |
| 5 | `PAUSE_PRESSION` | Pression perdue — attente retour |
| 6 | `TEMPO_ARRIVEE` | Arrosage final après fin de course |
| 7 | `ARRET_FINAL` | Session terminée normalement |
| 8 | `ARRET_URGENCE` | Incident matériel — voir `raison_arret` |
| 9 | `DEROULE` | Mesure longueur déployée |

> **Note** : le sous-état du cycle poumon (`SOUS_VIDANGE` / `SOUS_ATTENTE` / `SOUS_REMPLISSAGE`)
> est une variable interne du firmware — il n'est **pas** sérialisé dans le JSON WebSocket.
> L'UI déduit l'état du cycle depuis `ev_poumon` et les timings.

### Valeurs `batterie_etat`

| Valeur (string) | Tension |
|---|---|
| `"CHARGE"` | > 13.5V (panneau solaire actif) |
| `"PLEINE"` | 12.4..13.5V |
| `"CORRECTE"` | 11.8..12.4V |
| `"FAIBLE"` | cfg_batt_warn_v..11.8V |
| `"CRITIQUE"` | < cfg_batt_crit_v |

### Valeurs `mosfet_canon_etat` / `mosfet_poumon_etat`

| Valeur (string) | Signification | Cause typique |
|---|---|---|
| `"OK"` | MOSFET sain, secours inactif | — |
| `"grillé CC"` | Court-circuit — tension présente même GPIO=LOW | MOSFET claqué en court-circuit |
| `"HS ouvert"` | Circuit ouvert — pas de tension même GPIO=HIGH | MOSFET grillé en circuit ouvert |
| `"EV débranchée"` | Courant < 50 mA alors que tension présente | Fil EV débranché ou coupé |

> Ces champs contiennent l'état du **MOSFET principal** uniquement. Quand `mosfet_*_secours=false`,
> l'état est toujours `"OK"` (aucune panne détectée).

---

## Messages navigateur → ESP32 (commandes)

Envoyés depuis l'UI via `sendCmd({cmd:'...', ...})`.

### Commandes de contrôle

```json
{"cmd": "start"}
```
Active `s_demarrage_autorise`. La machine passe en OUVERTURE_CANON au prochain tick si
toutes les conditions sont remplies (pression OK, fin_course LOW, cfg_valide).

```json
{"cmd": "stop"}
```
Arrêt immédiat → ARRET_FINAL. EV_POUMON=OFF, EV_CANON=OFF.

```json
{"cmd": "reset"}
```
Depuis ARRET_URGENCE ou ARRET_FINAL → VEILLE. Remet à zéro toutes les longueurs et
la raison d'arrêt NVS.

```json
{"cmd": "resume"}
```
Depuis ARRET_URGENCE ou ARRET_FINAL → VEILLE. **Préserve** les longueurs (reprise session).
**Refusé** si raison="Debordement bobine" ET `secu_spires` encore actif.

### Synchronisation heure

```json
{"cmd": "set_time", "ts": 1748606400}
```
Synchronise l'horloge ESP32 (timestamp Unix). Utilisé pour calculer l'heure d'arrivée prévue.
L'UI envoie ceci automatiquement à la connexion.

### IRRITESTEUR (mode test manuel)

Pilotage direct des EV hors cycle (VEILLE uniquement) :

```json
{"cmd": "ev_canon", "actif": true}
{"cmd": "ev_canon", "actif": false}
{"cmd": "ev_poumon", "actif": true}
{"cmd": "ev_poumon", "actif": false}
```

### Commencer le déroulement

```json
{"cmd": "start_deroule"}
```
Entre en ETAT_DEROULE manuellement (depuis VEILLE uniquement). En mode
simulateur, utiliser la commande sim `start_deroule` via `/ws_test` qui
force l'entrée depuis n'importe quel état.

### Configuration programmes

```json
{
  "cmd": "select_programme",
  "idx": 2
}
```
Change le programme actif (0-4). Recharge immédiatement depuis NVS.

```json
{
  "cmd": "save_programme",
  "idx": 0,
  "nom": "Mais P1",
  "dose_mm": 25.0,
  "largeur_m": 66.0,
  "buse_mm": 20,
  "pression_bar": 6.5,
  "tempo_depart_on": false,
  "tempo_depart_s": 0,
  "tempo_arrivee_on": true,
  "tempo_arrivee_s": 120
}
```

### Configuration machine

```json
{
  "cmd": "save_machine",
  "t_vidange_s": 1.2,
  "kp_regulation": 0.1,
  "n_cycles_calib": 3,
  "mode_deg_poumon": false,
  "mode_deg_spires": false,
  "t_rempl_fixe_s": 0.0,
  "denivele_m": 0.0,
  "cycles_par_tour": 40.0,
  "batt_warn_v": 11.5,
  "batt_crit_v": 11.0
}
```

### Étalonnage et correction longueur

```json
{"cmd": "etalonner", "longueur_m": 285.0}
```
Calcule le facteur de correction depuis la longueur réelle saisie vs longueur firmware.
Conditions de validation : nb_impulsions > 50, facteur entre 0.5 et 2.0, delta < 30%.

```json
{"cmd": "set_longueur", "longueur_m": 247.0}
```
Force la longueur déployée (correction opérateur si mesure déroulement inexacte).

### Stats campagne

```json
{"cmd": "reset_campagne"}
```
Remet à zéro toutes les stats campagne en NVS.

---

## Message spécial : bilan fin de session

Envoyé une seule fois à l'entrée de ETAT_ARRET_FINAL (pas un broadcast périodique) :

```json
{
  "type": "bilan",
  "longueur_m": 285.5,
  "surface_m2": 18843,
  "dose_moy_mm": 24.9,
  "volume_m3": 469.0,
  "duree_s": 7200,
  "nb_cycles": 1320,
  "duree_pause_s": 45
}
```

L'UI affiche un bandeau vert 15s avec ce bilan.

---

## Endpoint HTTP — preview vitesse

```
GET /api/vitesse?p=6.5&b=20&d=25&larg=65
```

| Paramètre | Rôle | Exemple |
|---|---|---|
| `p` | Pression enrouleur (bar) | 6.5 |
| `b` | Diamètre buse (mm) | 20 |
| `d` | Dose cible (mm) | 25 |
| `larg` | Espacement entre positions (m) — obligatoire | 65 |

Réponse étendue (PR-18) :
```json
{
  "vitesse_m_h": 19.3,
  "debit_ls": 8.86,         // débit en L/s (m³/h / 3.6)
  "p_buse_bar": 4.0,
  "portee_m": 42.0,         // rayon du jet (sens constructeur)
  "esp_nominal_m": 65.1,    // espacement recommandé = portée × 1.55
  "esp_pos_min": 48.8,      // esp_nominal × 0.75
  "esp_pos_max": 71.6,      // esp_nominal × 1.10
  "p_min": 3.68,            // bornes pression abaque × 0.75 / × 1.25
  "p_max": 11.88,
  "buse_min": 12.98,
  "buse_max": 31.75,
  "dose_min": 11.25,
  "dose_max": 50.0,
  "warnings": {
    "pression_basse": false,
    "pression_haute": false,
    "buse_hors_plage": false,
    "dose_hors_plage": false,
    "esp_pos_chevauchement": false,  // larg < esp_nominal × 0.75
    "esp_pos_insuf": false,          // larg > esp_nominal × 1.10
    "vitesse_limite": false,         // V_cible > V_max théorique
    "v_max_m_h": 0.0                 // V_max = dist_cycle / (t_rempl_min + t_vidange) × 3600
  }
}
```

Appelé en temps réel dans le formulaire programme (debounce 400ms). Affiche bornes min/max sous chaque champ et alertes jaunes non-bloquantes si warning actif.

---

## Exemple de session commentée

```
// Connexion WebSocket
Client → ESP32 : {"cmd":"set_time","ts":1748606400}
ESP32 → Client : {"etat":"VEILLE","etat_code":0,...}  // broadcast 500ms

// Démarrage
Client → ESP32 : {"cmd":"start"}
ESP32 → Client : {"etat":"OUVERTURE_CANON","etat_code":1,"ev_canon":true,...}

// Après 3s pression stable
ESP32 → Client : {"etat":"REMPLISSAGE_POUMON","etat_code":3,"ev_poumon":true,...}

// Poumon plein
ESP32 → Client : {"etat":"EN_COURS","etat_code":4,"ev_poumon":false,...} // SOUS_VIDANGE/ATTENTE
ESP32 → Client : {"etat":"EN_COURS","ev_poumon":true,...}               // SOUS_REMPLISSAGE
// ... cycles répétés pendant 2h ...

// Fin de course détectée
ESP32 → Client : {"etat":"ARRET_FINAL","etat_code":7,...}
ESP32 → Client : {"type":"bilan","longueur_m":285.5,...}  // bilan une seule fois

// Opérateur choisit de reprendre (session suivante)
Client → ESP32 : {"cmd":"resume"}
ESP32 → Client : {"etat":"VEILLE","etat_code":0,...}

// Reset complet si nouvelle campagne
Client → ESP32 : {"cmd":"reset"}
Client → ESP32 : {"cmd":"reset_campagne"}

// Simulation déroulement (CONFIG_IRRI_TEST_MODE, via /ws_test)
Client → ESP32 : {"sim":"start_deroule","value":1}       // force ETAT_DEROULE
Client → ESP32 : {"sim":"vitesse_deroule","value":60}    // 6.0 km/h (valeur × 100 = m/h)
ESP32 → Client : {"etat":"DEROULE","etat_code":9,"mesure_deroule_m":3.7,...}
Client → ESP32 : {"sim":"vitesse_deroule","value":0}     // arrêt pulses
```

---

## WebSocket simulateur — `/ws_test` (CONFIG_IRRI_TEST_MODE)

Route distincte de `/ws`. Toutes les commandes ont la forme :
`{"sim": "<clé>", "value": <entier>}`

| Clé | Valeur | Effet |
|---|---|---|
| `pressostat` | 0/1 | Simule pression OK/perdue (GPIO 25) |
| `fin_course` | 0/1 | Simule fin de course inactif/actif (GPIO 35) |
| `secu_spires` | 0/1 | Simule débordement bobine (GPIO 32) |
| `poumon_plein` | 0/1 | Simule contact poumon plein (GPIO 33) |
| `vitesse_auto` | m/h | Génère des pulses capteur à la vitesse donnée (enroulement) |
| `vitesse_deroule` | dixièmes de km/h | Génère des pulses à la vitesse tracteur (0–80 = 0–8 km/h). Conversion : valeur × 100 = m/h |
| `start_deroule` | 1 | Force ETAT_DEROULE depuis n'importe quel état (voir `state_machine_sim_force_deroule()`) |
| `longueur` | m | Force `set_longueur` |
| `batt_v` | dixièmes de V | Simule tension batterie (125 = 12.5V, 0 = ADC réel) |

> **Important** : `simulator_init()` doit être appelé avant `simulator_ws_register()` dans
> `webserver_init()` pour que la tâche `task_pulses` (stack 4096 bytes, priorité 5) soit créée.
> Sans cet appel, aucun pulse ne sera généré.
