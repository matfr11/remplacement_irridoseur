#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Namespaces NVS
#define NVS_NS_MACHINE  "irri_machine"   // Paramètres fixes installation
#define NVS_NS_PROG     "irri_prog"      // 5 programmes d'arrosage
#define NVS_NS_STATE    "irri_state"     // État persistant (programme actif)

// Nombre de programmes configurables
#define NB_PROGRAMMES  5

// Longueur max nom programme (caractères + '\0')
#define NOM_PROGRAMME_MAX  21

// --- Paramètres machine (fixes à l'installation, modifiables via web UI) ---
//
// Valeurs géométriques : calculées depuis fiche technique constructeur
//   Irrifrance Structure 2 AT/P, Ø82mm int, épaisseur 6mm, 330m
//   d_tuyau_ext = 82 + 2×6 = 94mm = 0.094m
//   r_tambour_vide = R_couche4 - (3.5 × d_tuyau_ext) = 0.977 - 0.329 = 0.648m
//   Source calcul : tableau spires ST.1 Bis, longueur spire couche 4 = 6.14m
typedef struct {
    // Géométrie tuyau et bobine
    float  longueur_tuyau_m;   // Longueur tuyau PE (m),            défaut 330.0
    float  diam_int_tuyau_m;   // Diamètre intérieur tuyau (m),     défaut 0.082
    float  d_tuyau_ext_m;      // Diamètre extérieur tuyau (m),     défaut 0.094 (82mm+2×6mm — modifiable UI)
    float  r_tambour_vide_m;   // Rayon tambour nu (m),             défaut 0.648 (fiche tech — modifiable UI)
    int    nb_etages;          // Nombre de couches de spires,       défaut 4

    // Mécanique poumon
    float  t_vidange_s;        // Durée vidange mécanique (ressort), défaut 5.0 ⚠️ à mesurer
    float  kp_regulation;      // Gain proportionnel correction vitesse, défaut 0.1
    int    n_cycles_calib;     // Nb cycles auto-calibration avant correction active, défaut 3

    // Hydraulique (pour calcul indicatif P_canon — onglet Stats)
    float  perte_enroul_bar;   // Perte de charge enrouleur (bar),  défaut 2.5
    float  denivele_m;         // Dénivelé bobine→canon (m),        défaut 0.0 (terrain plat)

    // Sens des contacteurs — dépend du câblage réel (NO ou NC)
    // true  = Contact NO (Normalement Ouvert) avec pull-up → actif bas (GPIO LOW = actif)
    // false = Contact NC (Normalement Fermé) avec pull-up → actif haut (GPIO HIGH = actif)
    // Défaut : true (NO) pour tous — à vérifier au multimètre avant mise en service
    bool   contact_no_fin_course;       // Fin de course canon (GPIO 35)
    bool   contact_no_securite_spires;  // Sécurité débordement bobine (GPIO 32)
    bool   contact_no_poumon;           // Contact poumon plein (GPIO 33)
    bool   contact_no_pressostat;       // Pressostat RP1 (GPIO 27)
} config_machine_t;

// --- Programme d'arrosage ---
typedef struct {
    char   nom[NOM_PROGRAMME_MAX]; // Nom personnalisé
    float  dose_mm;                // Dose cible (mm = L/m²),  défaut 25.0
    float  largeur_m;              // Largeur de position (m), défaut 18.0
    int    d_buse_mm;              // Diamètre buse (mm),      défaut 18
    float  p_borne_bar;            // Pression à la borne (bar) — entrée table constructeur, défaut 6.0
    bool   tempo_depart_on;        // Temporisation avant démarrage activée
    int    tempo_depart_s;         // Durée tempo départ (s)
    bool   tempo_arrivee_on;       // Temporisation arrivée canon activée
    int    tempo_arrivee_s;        // Durée tempo arrivée (s)
} config_programme_t;

// --- Prototypes ---
esp_err_t config_nvs_init(void);

esp_err_t config_nvs_lire_machine(config_machine_t *cfg);
esp_err_t config_nvs_sauver_machine(const config_machine_t *cfg);

esp_err_t config_nvs_lire_programme(int idx, config_programme_t *prog);
esp_err_t config_nvs_sauver_programme(int idx, const config_programme_t *prog);

esp_err_t config_nvs_lire_prog_actif(int *idx);
esp_err_t config_nvs_sauver_prog_actif(int idx);

// Retourne true si les paramètres critiques permettent de démarrer
// Avec les valeurs par défaut issues de la fiche technique, retourne true
bool config_machine_est_valide(const config_machine_t *cfg);

// Retourne true si t_vidange_s est encore à sa valeur par défaut (non mesurée terrain)
bool config_machine_t_vidange_par_defaut(const config_machine_t *cfg);
