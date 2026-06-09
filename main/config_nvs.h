#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// =============================================================================
// config_nvs.h — Persistance NVS Irrifrance ESP32
// Namespaces : irri_machine (12 clés) | irri_prog ×5 (9 clés) | irri_state (2 clés)
// =============================================================================

#define NB_PROGRAMMES  5

// -----------------------------------------------------------------------------
// Paramètres machine (namespace irri_machine)
// Valeurs ajustables terrain — la géométrie est dans machine_profile_t (machines.h)
// -----------------------------------------------------------------------------
typedef struct {
    int     machine_active;         // Index profil machine actif
    float   t_vidange_s;            // ⚠️ Durée vidange mécanique (s)
    float   facteur_correction;     // Étalonnage longueur (1.0 = neutre)
    float   dist_cycle_nvs;         // Dernière dist/cycle connue (m)
    float   kp_regulation;          // Gain correction vitesse
    int     n_cycles_calib;         // Cycles avant activation correction
    int     fenetre_vitesse;        // Nb impulsions fenêtre vitesse
    int     max_cycles_si;          // Cycles sans impulsion → v=0
    bool    mode_deg_poumon;        // Mode dégradé contact poumon
    bool    mode_deg_spires;        // Bypass SEC-2 spires (capteur défaillant)
    float   t_rempl_fixe_s;         // Durée remplissage fixe (mode dégradé B)
    float   denivele_m;             // Dénivelé terrain (0 = plat)
    float   cycles_par_tour;        // Nb cycles poumon par tour de bobine (0 = non renseigné)
    bool    heartbeat_rc_on;        // Heartbeat GPIO 2 pour circuit RC fail-safe (défaut OFF)
    float   fin_course_seuil_m;     // Longueur restante en dessous de laquelle fin_course = fin normale (défaut 10m)
    int     abaque_idx;             // Index abaque canon actif (0=SR150C, 1=SR100C)
} config_machine_t;

// -----------------------------------------------------------------------------
// Programme d'arrosage (namespace irri_prog, index 0-4)
// -----------------------------------------------------------------------------
typedef struct {
    char    nom[21];                // 20 chars + null
    float   dose_mm;                // Dose cible (mm) — plage 10-50
    float   largeur_m;              // Largeur position (m)
    int     buse_mm;                // Diamètre buse (mm)
    float   pression_bar;           // Pression enrouleur (bar)
    bool    tempo_depart_on;
    int     tempo_depart_s;
    bool    tempo_arrivee_on;
    int     tempo_arrivee_s;
} config_programme_t;

// Valeurs par défaut
#define CFG_MACHINE_DEFAUT { \
    .machine_active    = 0,     \
    .t_vidange_s       = 2.0f,  \
    .facteur_correction= 1.0f,  \
    .dist_cycle_nvs    = 0.0f,  \
    .kp_regulation     = 0.1f,  \
    .n_cycles_calib    = 3,     \
    .fenetre_vitesse   = 5,     \
    .max_cycles_si     = 15,    \
    .mode_deg_poumon   = false, \
    .mode_deg_spires   = false, \
    .t_rempl_fixe_s    = 0.0f,  \
    .denivele_m        = 0.0f,  \
    .cycles_par_tour   = 0.0f,  \
    .heartbeat_rc_on    = false, \
    .fin_course_seuil_m = 10.0f, \
    .abaque_idx         = 0,     \
}

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------
esp_err_t config_nvs_init(void);

esp_err_t config_nvs_lire_machine(config_machine_t *cfg);
esp_err_t config_nvs_sauver_machine(const config_machine_t *cfg);

esp_err_t config_nvs_lire_programme(int idx, config_programme_t *prog);
esp_err_t config_nvs_sauver_programme(int idx, const config_programme_t *prog);

esp_err_t config_nvs_lire_prog_actif(int *idx);
esp_err_t config_nvs_sauver_prog_actif(int idx);

// Sauvegarde la raison du dernier arrêt urgence (namespace irri_state)
esp_err_t config_nvs_sauver_urgence(const char *raison);
esp_err_t config_nvs_lire_urgence(char *raison, size_t len);

// Flag session active — permet de détecter une coupure de courant au reboot
// Mis à 1 au démarrage d'une session, mis à 0 à l'arrêt propre (ARRET_FINAL/reset)
esp_err_t config_nvs_sauver_session_active(bool actif);
bool      config_nvs_lire_session_active(void);

// Longueur persistante session — survie au reboot (namespace irri_state)
// longueur_m = position absolue interne (calculs mécaniques)
// deroule_m  = longueur déployée en champ (affichage session)
esp_err_t config_nvs_sauver_longueur(float longueur_m);
esp_err_t config_nvs_lire_longueur(float *longueur_m);
esp_err_t config_nvs_sauver_deroule(float deroule_m);
esp_err_t config_nvs_lire_deroule(float *deroule_m);
// Sauvegarde atomique longueur + deroule dans une seule transaction NVS
esp_err_t config_nvs_sauver_position(float longueur_m, float deroule_m);

// Valide qu'un programme est utilisable (dose, largeur, buse, pression > 0)
bool config_programme_est_valide(const config_programme_t *prog);

// -----------------------------------------------------------------------------
// Stats campagne cumulatives (namespace irri_stats)
// -----------------------------------------------------------------------------
typedef struct {
    float    total_surface_ha;
    float    total_volume_m3;
    float    dose_moy_mm;
    float    vitesse_moy_m_h;
    uint32_t nb_sessions;
    float    total_duree_h;
} config_stats_t;

esp_err_t config_nvs_lire_stats(config_stats_t *stats);
esp_err_t config_nvs_sauver_stats(const config_stats_t *stats);
esp_err_t config_nvs_reset_stats(void);

// Seuils batterie (namespace irri_machine, cles separees — pas de migration blob)
esp_err_t config_nvs_lire_batt_seuils(float *warn_v, float *crit_v);
esp_err_t config_nvs_sauver_batt_seuils(float warn_v, float crit_v);

// Temps de remplissage minimum historique (namespace irri_machine, cle separee)
// Defaut 5.0s — mis a jour si nouvelle valeur < courante (evite l'usure flash)
// Remis a 5.0 lors d'un cmd_reset() complet.
esp_err_t config_nvs_lire_t_rempl_min(float *t_s);
esp_err_t config_nvs_sauver_t_rempl_min(float t_s);
