#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

// --- États de la machine ---
typedef enum {
    ETAT_VEILLE = 0,          // Attente programme configuré + pression
    ETAT_TEMPO_DEPART,        // Temporisation avant ouverture EV1
    ETAT_REMPLISSAGE_POUMON,  // EV2=ON, attente contact poumon plein
    ETAT_EN_COURS,            // EV1=ON, régulation fréquence poumon active
    ETAT_TEMPO_ARRIVEE,       // EV1=ON, EV2=OFF, minuteur tempo arrivée
    ETAT_ARRET_FINAL,         // EV1=OFF, EV2=OFF, affichage bilan
    ETAT_ARRET_URGENCE,       // EV1=OFF, EV2=OFF, message erreur, attend RESET manuel
    ETAT_IRRITESTEUR,         // Mode diagnostic — suspend la machine d'états
} etat_machine_t;

// --- États bruts GPIO pour le mode IRRITESTEUR (et monitoring WebSocket) ---
typedef struct {
    uint32_t vitesse_impulsions; // Compteur total impulsions depuis démarrage
    float    vitesse_m_h;        // Vitesse instantanée calculée (m/h)
    bool     fin_course;         // Niveau logique après application sens NO/NC
    bool     securite_spires;
    bool     contact_poumon;
    bool     pressostat;
    bool     ev1;                // État relais EV1 (true = ouvert)
    bool     ev2;                // État relais EV2 (true = ouvert)
} gpio_raw_t;

// --- Statut complet diffusé par WebSocket (toutes les 500ms) ---
typedef struct {
    etat_machine_t etat;
    int            etat_code;
    char           prog_nom[21];
    float          longueur_deroulee_m;
    float          longueur_enroulee_m;
    float          vitesse_m_h;
    uint32_t       duree_s;
    int64_t        heure_arrivee_unix;
    int32_t        heure_arrivee_relative_s;
    bool           heure_synchro;
    float          surface_m2;
    float          dose_inst_mm;
    float          debit_m3h;
    float          p_mano_bar;      // Pression manomètre (affichage Stats)
    float          p_canon_bar;     // Pression calculée au canon (affichage Stats)
    int            etage_courant;
    int            nb_etages;
    bool           ev1;
    bool           ev2;
    bool           pression_ok;
    char           raison_arret[64];
    bool           cfg_valide;
    gpio_raw_t     gpio_raw;        // États bruts — utilisés par IRRITESTEUR et WebSocket
} machine_status_t;

// --- Résumé de session (affiché en ARRET_FINAL et envoyé par telemetry) ---
typedef struct {
    float    surface_m2;
    float    dose_moy_mm;
    uint32_t duree_s;
    float    volume_m3;
} session_summary_t;

// --- Prototypes ---
void state_machine_init(void);
void tick_state_machine(void);
void state_machine_get_status(machine_status_t *status);

// Commandes cycle
void state_machine_cmd_start(void);
void state_machine_cmd_stop(void);
void state_machine_cmd_reset(void);
void state_machine_set_time(int64_t timestamp_unix);

// Commandes IRRITESTEUR — uniquement valides depuis ETAT_VEILLE
void state_machine_cmd_entrer_irritesteur(void);
void state_machine_cmd_quitter_irritesteur(void);
void state_machine_cmd_ev1_set(bool actif);  // uniquement en ETAT_IRRITESTEUR
void state_machine_cmd_ev2_set(bool actif);  // uniquement en ETAT_IRRITESTEUR
