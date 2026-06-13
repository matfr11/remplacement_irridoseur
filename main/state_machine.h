#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "machines/machines.h"
#include "abaques/abaques.h"
#include "config_nvs.h"

// =============================================================================
// state_machine.h — Machine d'états Irrifrance ESP32
// =============================================================================

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
    ETAT_DEROULE,             // Mesure longueur déployée par pastilles (avant session)
} etat_machine_t;

// Sous-états cycle poumon (ETAT_EN_COURS uniquement)
typedef enum {
    SOUS_VIDANGE,       // EV_POUMON=OFF, ressort retracte cliquet (bobine immobile)
    SOUS_ATTENTE,       // EV_POUMON=OFF, pause régulation
    SOUS_REMPLISSAGE,   // EV_POUMON=ON, cliquet avance bobine (impulsions ici)
} sous_etat_poumon_t;

// Statut complet diffusé via WebSocket toutes les 500ms
typedef struct {
    // État machine
    etat_machine_t  etat;
    sous_etat_poumon_t sous_etat;
    char            raison_arret[64];

    // Identifiants session
    char            prog_nom[21];
    char            machine_nom[32];
    char            abaque_nom[32];

    // Programme actif — valeurs courantes (pour affichage UI)
    float           prog_dose_mm;
    float           prog_largeur_m;
    int             prog_buse_mm;
    float           prog_pression_bar;
    bool            prog_tempo_depart_on;
    int             prog_tempo_depart_s;
    bool            prog_tempo_arrivee_on;
    int             prog_tempo_arrivee_s;

    // Longueurs
    float           longueur_deroulee_m;    // Longueur totale tuyau
    float           longueur_enroulee_m;    // Enroulé depuis départ session

    // Vitesse
    float           vitesse_m_h;
    float           vitesse_cible_m_h;

    // Temps
    int32_t         duree_s;
    int64_t         heure_arrivee_unix;
    int32_t         heure_arrivee_relative_s;
    bool            heure_synchro;

    // Agronomie
    float           surface_m2;
    float           dose_inst_mm;
    float           debit_m3h;
    float           p_enrouleur_bar;
    float           p_buse_bar;

    // Mécanique bobine
    int             etage_courant;
    int             nb_etages;

    // GPIO
    bool            ev_canon;
    bool            ev_poumon;
    bool            pression_ok;
    bool            fin_course;
    bool            secu_spires;
    bool            poumon_plein;

    // Déroulement (ETAT_DEROULE)
    float           mesure_deroule_m;

    // Régulation poumon
    int32_t         t_remplissage_ms;
    int32_t         t_attente_ms;
    float           dist_par_cycle_m;
    float           cycles_par_min_cible;
    float           cycles_par_min_reel;
    uint32_t        cycles_total;

    // Modes dégradés / alertes
    bool            alerte_pression_insuff;
    bool            mode_degrade_poumon;
    bool            mode_degrade_spires;
    float           facteur_correction;

    // Campagne (cumulatif NVS)
    float           camp_surface_ha;
    float           camp_volume_m3;
    float           camp_dose_moy_mm;
    float           camp_vitesse_moy_m_h;
    uint32_t        camp_nb_sessions;
    float           camp_duree_h;

    // Batterie
    float           batterie_v;
    int             batterie_pct;
    int             batterie_etat;      // batt_etat_t cast en int
    float           cfg_batt_warn_v;
    float           cfg_batt_crit_v;

    // Validité config
    bool            cfg_valide;

    // Coupure détectée au boot (session interrompue sans arrêt propre)
    bool            coupure_detectee;
    float           cfg_fin_course_seuil_m;

    // Vitesse max et dose corrigée (valides si alerte_pression_insuff = true)
    float           vitesse_max_m_h;
    float           dose_corrigee_mm;

    // Paramètres machine (pour initialisation UI Config)
    float           cfg_t_vidange_s;
    float           cfg_kp_regulation;
    int             cfg_n_cycles_calib;
    float           cfg_t_rempl_fixe_s;
    float           cfg_denivele_m;
    int             cfg_machine_active;
    int             cfg_abaque_idx;
    float           cfg_cycles_par_tour;
    bool            cfg_heartbeat_rc_on;

} machine_status_t;

// Session summary pour télémétrie fin de session
typedef struct {
    float    longueur_m;
    float    surface_m2;
    float    dose_moy_mm;
    float    volume_m3;
    int32_t  duree_s;
    uint32_t nb_cycles;
    int32_t  duree_pause_pression_s;
} session_summary_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------
void state_machine_init(void);
void tick_state_machine(void);
void state_machine_get_status(machine_status_t *status);
etat_machine_t state_machine_get_etat(void);

// Commandes opérateur (depuis WebSocket)
void state_machine_cmd_start(void);
void state_machine_cmd_stop(void);
void state_machine_cmd_reset(void);
void state_machine_cmd_etalonner(float longueur_reelle_m);
void state_machine_cmd_set_longueur(float longueur_m);
void state_machine_set_time(int64_t timestamp_unix);

// Mode IRRITESTEUR (panel Config — hors cycle uniquement)
void state_machine_cmd_ev_canon_set(bool actif);
void state_machine_cmd_ev_poumon_set(bool actif);

// Reprise session interrompue (ARRET_URGENCE/FINAL → VEILLE, longueurs préservées)
void state_machine_cmd_resume(void);

// Déclenchement manuel ETAT_DEROULE (si fin_course déjà false en entrée VEILLE)
void state_machine_cmd_start_deroule(void);

// Déclenchement urgence (appelé par securites.c)
void state_machine_declencher_urgence(const char *raison);

// Recharge la config depuis NVS dans les variables internes (VEILLE uniquement).
// Appelé depuis webserver.c après save_programme / save_machine / select_programme.
void state_machine_recharger_config(void);

// Expose le bilan de la dernière session terminée.
void state_machine_get_session_summary(session_summary_t *out);

// Remise à zéro stats campagne NVS.
void state_machine_cmd_reset_campagne(void);

// Retourne true si longueur restante <= seuil fin_course_seuil_m (fin de bobine normale).
// Appelable sans mutex — depuis securites_watchdog() dans le contexte du tick uniquement.
bool state_machine_fin_course_est_normale(void);

// Retourne true si longueur_session_m dépasse longueur_deroule_m + seuil (capteur fin de course défaillant).
// Appelable sans mutex — depuis securites_watchdog() dans le contexte du tick uniquement.
bool state_machine_longueur_sec_depassee(void);

// Calcul vitesse cible (lookup abaque) — utilisé par endpoint HTTP /api/vitesse.
float state_machine_calc_vitesse(float pression_bar, float buse_mm, float dose_mm,
                                  float largeur_m,
                                  float *debit_out, float *p_buse_out);

// Vitesse max theorique basee sur t_rempl_min_s historique + t_vidange + dist_cycle.
// Retourne 0 si dist_cycle non encore calibre.
float state_machine_get_vitesse_max(void);

// Preview complet pour /api/vitesse — toutes les donnees pour le formulaire programme.
typedef struct {
    float vitesse_m_h;
    float debit_ls;         // debit en L/s (= m3/h / 3.6)
    float p_buse_bar;
    float portee_m;         // rayon du jet (sens constructeur Irrifrance)
    float esp_nominal_m;    // espacement recommande = portee x esp_factor
    float esp_pos_min;      // esp_nominal x 0.75
    float esp_pos_max;      // esp_nominal x 1.10
    float p_min, p_max;     // bornes pression abaque x 0.75 / x 1.25
    float buse_min, buse_max;
    float dose_min, dose_max; // 15 x 0.75 / 40 x 1.25
    bool  w_pression_basse;
    bool  w_pression_haute;
    bool  w_buse_hors_plage;
    bool  w_dose_hors_plage;
    bool  w_esp_pos_chevauchement;
    bool  w_esp_pos_insuf;
    bool  w_vitesse_limite; // V_cible > V_max theorique
    float v_max_m_h;        // pour message warning vitesse
} programme_preview_t;

programme_preview_t state_machine_programme_preview(float pression_bar, float buse_mm,
                                                     float dose_mm, float largeur_m);

#ifdef CONFIG_IRRI_TEST_MODE
// Force l'entrée en ETAT_DEROULE depuis n'importe quel état (simulateur).
// Coupe les EVs, remet s_demarrage_autorise à false pour éviter l'auto-démarrage.
void state_machine_sim_force_deroule(void);
#endif

#ifdef CONFIG_IRRI_WOKWI_MODE
void state_machine_wokwi_set_pression(bool pression_ok);
#endif

#ifdef CONFIG_IRRI_ENABLE_TESTS
void state_machine_test_injecter_etat(etat_machine_t etat);
void state_machine_test_set_pression(bool pression_ok);
void state_machine_test_set_fin_course(bool active);
void state_machine_test_set_secu_spires(bool active);
int  state_machine_test_get_nb_tentatives(void);
void state_machine_test_reset(void);
// Injecte directement les longueurs internes (contourne NVS)
void state_machine_test_set_longueurs(float deroule_m, float session_m);
#endif

