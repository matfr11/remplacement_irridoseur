# Architecture — Guide développeur

Vue d'ensemble du projet pour un développeur C rejoignant le projet.

---

## Vue d'ensemble

L'Irridoseur 3 (régulation d'origine) est une boîte noire électronique qui contrôle la vitesse
d'enroulement d'un enrouleur d'irrigation agricole. L'ESP32 remplace cette boîte avec :

- La même logique de régulation (poumon hydraulique TPI)
- Une interface web mobile WiFi pour configurer et superviser
- La persistance des sessions en mémoire flash (reboot sans perte)
- Des modes dégradés pour continuer si un capteur tombe en panne

```
[Canon d'arrosage]←[tuyau PE]←[bobine]←[enrouleur]
       ↑                                    ↓
   EV_CANON                           Régulation ESP32
       ↑                                    ↑
   EV_POUMON←[Poumon hydraulique]←[Pression eau]
```

### Principe physique

L'enrouleur tire le tuyau par cycles :
1. EV_POUMON=ON → poumon se remplit → cliquet clique → bobine tourne d'un cran
2. EV_POUMON=OFF → poumon se vide → ressort rétracte cliquet → pause
3. La vitesse d'enroulement = dist_par_cycle × fréquence_cycles

Le firmware contrôle la durée des pauses (T_attente) pour ajuster la vitesse.

---

## Rôle de chaque module

| Module | Ce qu'il fait |
|---|---|
| `state_machine.c` | Chef d'orchestre. Contient les 10 états, la régulation, et toutes les décisions. Les autres modules lui fournissent des données ou exécutent ses ordres. |
| `securites.c` | Watchdog de sécurité. Appelé EN PREMIER à chaque tick (100ms). Coupe tout si danger (débordement bobine ou fin de course inattendu). |
| `gpio_handler.c` | Lit les capteurs et pilote les électrovannes. Contient l'ISR du capteur de vitesse (impulsions magnétiques). |
| `regulation.c` | Calculs de régulation : combien de temps attendre entre deux cycles poumon pour avoir la bonne vitesse. Feedforward + correction proportionnelle. |
| `calculs_hydraulique.c` | Formule analytique Torricelli : Q=k_q×buse²×√p_buse, V=Q×1000/(larg×dose). Validation programme (bornes ±25%, portée, V_max). |
| `calculs_mecanique.c` | Géométrie de la bobine : quel rayon à l'étage courant, quelle distance avance-t-on par impulsion capteur. |
| `config_nvs.c` | Sauvegarde/lecture en flash : configuration machine, programmes d'arrosage, position courante, urgences, statistiques. |
| `webserver.c` | WiFi AP + page web + WebSocket. Reçoit les commandes de l'opérateur, envoie l'état toutes les 500ms. |
| `batterie.c` | Mesure la tension batterie via ADC pour afficher le niveau sur l'UI. |
| `telemetry.c` | Tâche FreeRTOS qui diffuse l'état JSON toutes les 500ms sur le WebSocket. |
| `ota.c` | Mise à jour du firmware via l'UI web (sans câble USB). |
| `machines/` | Profils des enrouleurs supportés (dimensions bobine, longueur tuyau). |
| `abaques/` | Tables débit constructeur pour les canons d'arrosage supportés. |
| `simulator/` | Remplace les capteurs physiques par des curseurs web (pour tests sans matériel). |

---

## Flux de données complet

```
CAPTEURS PHYSIQUES
──────────────────
GPIO 34 (impulsions magnétiques)
  → ISR gpio_handler.c (anti-rebond 50ms)
  → Buffer fenêtre glissante (5 impulsions par défaut)
  → gpio_get_vitesse_m_h(facteur_correction) → float vitesse_m_h

GPIO 35, 32, 33, 27 (contacts NC)
  → gpio_handler_lire_entrees() → entrees_t { fin_course, secu_spires, poumon_plein, pression_ok }

GPIO 36 ADC (tension batterie)
  → batterie_lire_voltage() → float voltage_v → batt_status_t

TICK MACHINE D'ÉTATS (100ms)
─────────────────────────────
tick_state_machine()
  │
  ├── 1. securites_watchdog()          ← PRIORITÉ ABSOLUE
  │         Lit entrees, détecte SEC-1/SEC-2, coupe EV, déclenche urgence
  │
  ├── 2. gpio_handler_lire_entrees()   ← snapshot instantané
  │
  ├── 3. [3-tap check]                 ← réarmement physique
  │
  ├── 4. switch(s_etat)                ← traitement état courant
  │         Chaque état lit 'e' (entrees), décide des transitions et des sorties
  │         EN_COURS : calcule T_attente, pilote EV_POUMON, met à jour longueur
  │
  └── 5. s_status ← mise à jour       ← prépare le JSON 500ms

RÉGULATION (dans SOUS_REMPLISSAGE → SOUS_VIDANGE)
───────────────────────────────────────────────────
fin SOUS_REMPLISSAGE :
  dist_mesuree = gpio_get_impulsions() × dist_pulse_m × facteur_correction
  regulation_update_dist_par_cycle(dist_mesuree)    → moy glissante 5 valeurs
  longueur_enroulee += dist_mesuree
  calcul_etage_courant(longueur_enroulee)           → s_etage
  calcul_rayon_etage(s_etage)                        → R_n
  calcul_dist_cycle_m(R_n, cycles_par_tour)          → dist_cycle_m
  lookup_vitesse_cible(abaque, p, buse, dose)        → vitesse_cible_m_h
  calcul_t_attente_s(dist_cycle, vitesse, t_rempl, t_vidange) → T_attente
  correction_vitesse(T_attente, v_reelle, v_cible, kp)        → T_attente corrigé

DIFFUSION WEBSOCKET (500ms)
────────────────────────────
telemetry_task()
  → webserver_broadcast_status()
  → state_machine_get_status()    ← copie s_status (mutex)
  → status_to_json()              ← sérialise en JSON (~3KB)
  → httpd_ws_send_frame()         ← envoie à tous les clients connectés

COMMANDES OPÉRATEUR (WebSocket)
────────────────────────────────
httpd WS frame reçue
  → webserver.c parse JSON avec strstr()
  → appelle state_machine_cmd_*() ou batterie_set_seuils() etc.
  → state_machine prend le mutex, modifie état, relâche mutex
```

---

## Comment ajouter un nouveau profil machine

Un profil machine décrit la géométrie physique d'un enrouleur (dimensions bobine, longueur tuyau).

### Étape 1 — Créer le fichier profil

Créer `main/machines/mon_enrouleur.c` :

```c
#include "machines.h"

const machine_profile_t MACHINE_MON_ENROULEUR = {
    .nom               = "Mon Enrouleur 100m",
    .constructeur      = "Constructeur",
    .longueur_tuyau_m  = 100.0f,       // longueur tuyau (m)
    .d_tuyau_ext_m     = 0.063f,       // diamètre extérieur tuyau (m)
    .r_tambour_vide_m  = 0.450f,       // rayon tambour vide (m) — mesurer
    .nb_etages         = 3,            // nombre d'étages tuyau
    .abaque_idx        = 0,            // index abaque canon (0 = SR150C)
    .largeur_bobine_m  = 0.0f,         // laisser 0 si spires_par_etage connu
    .spires_par_etage  = 11.0f,        // nombre de spires par étage
    .t_vidange_s       = 0.0f,         // à mesurer terrain
    .facteur_correction = 1.0f,
};
```

> Renseigner SOIT `largeur_bobine_m` SOIT `spires_par_etage`. L'autre est calculé automatiquement
> par `machine_resoudre_double_entree()`.

### Étape 2 — Déclarer dans machines.h

```c
// Ajouter dans main/machines/machines.h
extern const machine_profile_t MACHINE_MON_ENROULEUR;
```

### Étape 3 — Ajouter au tableau

Dans `main/machines/machines.c` :

```c
const machine_profile_t * const MACHINES_LISTE[] = {
    &MACHINE_ST1BIS_82_330,
    &MACHINE_MON_ENROULEUR,   // ← ajouter ici
    NULL,
};
const int MACHINES_NB = 2;   // ← incrémenter
```

### Étape 4 — Ajouter le fichier au build

Dans `main/CMakeLists.txt`, ajouter `machines/mon_enrouleur.c` à la liste des sources.

### Étape 5 — Tester

Dans l'UI web, onglet Config → sélectionner le nouveau profil machine → vérifier les calculs
de vitesse cible dans le preview.

---

## Comment ajouter un nouvel abaque canon

Un abaque est la table de débit constructeur du canon d'arrosage. Il donne la vitesse d'enroulement
selon la pression, la buse et la dose souhaitée.

### Étape 1 — Créer le fichier abaque

Créer `main/abaques/mon_canon.c` :

```c
#include "abaques.h"

const canon_abaque_t ABAQUE_MON_CANON = {
    .nom              = "Mon Canon",
    .constructeur     = "Constructeur",
    .nb_entrees       = 5,
    // Constantes empiriques — a calibrer depuis la table constructeur
    .k_q              = 0.039f,   // Q(m3/h) = k_q * buse_mm^2 * sqrt(p_buse_bar)
    .k_portee         = 7.06f,    // portee(m) = k_portee * buse^portee_exp_buse * (p/3.5)^portee_exp_p
    .portee_exp_buse  = 0.557f,
    .portee_exp_p     = 0.30f,
    .esp_factor       = 1.55f,    // espacement = portee * esp_factor
    .table = {
        // p_enr  Q      p_buse  buse   esp    D40    D30    D25    D20    D15
        { 4.0f, 20.0f,  3.0f,  17.0f, 55.0f,  8.0f, 11.0f, 13.0f, 17.0f, 22.0f },
        { 5.0f, 23.0f,  3.5f,  17.0f, 60.0f,  9.0f, 12.0f, 14.5f, 18.0f, 24.0f },
        // ... autres lignes
    },
};
```

Colonnes obligatoires : `p_enrouleur(bar) | Q(m³/h) | p_buse(bar) | buse(mm) | esp(m)`
Colonnes D40..D15 : données source constructeur, conservées pour référence mais non utilisées dans le calcul (la formule analytique les remplace).

Les 5 constantes empiriques (`k_q`, `k_portee`, `portee_exp_buse`, `portee_exp_p`, `esp_factor`) sont spécifiques à chaque canon. Dériver `k_q` en ajustant sur les valeurs Q de la table : `k_q = Q / (buse_mm² × √p_buse)`.

### Étapes 2, 3, 4

Même principe que pour un profil machine :
- Déclarer `extern const canon_abaque_t ABAQUE_MON_CANON;` dans `abaques.h`
- Ajouter `&ABAQUE_MON_CANON` dans `ABAQUES_LISTE[]` et incrémenter `ABAQUES_NB`
- Ajouter `abaques/mon_canon.c` dans `CMakeLists.txt`

Le profil machine référence l'abaque via `abaque_idx` (index dans `ABAQUES_LISTE`).
