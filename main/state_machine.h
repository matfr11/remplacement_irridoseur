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
} etat_machine_t;

// --- Statut complet diffusé par WebSocket (toutes les 500ms) ---
typedef struct {
    etat_machine_t etat;
    int            etat_code;
    char           prog_nom[21];          // Nom programme actif (20 chars + '\0')
    float          longueur_deroulee_m;   // Longueur restant à enrouler (m)
    float          longueur_enroulee_m;   // Longueur enroulée depuis début cycle (m)
    float          vitesse_m_h;           // Vitesse instantanée (m/h)
    uint32_t       duree_s;               // Durée écoulée depuis démarrage (s)
    int64_t        heure_arrivee_unix;    // Heure estimée d'arrivée (Unix), -1 si inconnue
    int32_t        heure_arrivee_relative_s; // Durée restante estimée (s)
    bool           heure_synchro;         // true si heure synchronisée par navigateur
    float          surface_m2;            // Surface arrosée calculée (m²)
    float          dose_inst_mm;          // Dose instantanée (mm = L/m²)
    float          debit_m3h;             // Débit calculé depuis table abaque (m³/h)
    float          p_mano_bar;            // Pression manomètre configurée (bar)
    float          p_canon_bar;           // Pression effective calculée au canon (bar)
    int            etage_courant;         // Étage de spires courant (1 à nb_etages)
    int            nb_etages;             // Nombre total d'étages
    bool           ev1;                   // true = EV1 ouverte (canon arrose)
    bool           ev2;                   // true = EV2 ouverte (poumon en remplissage)
    bool           pression_ok;           // true = pressostat actif
    char           raison_arret[64];      // Message d'erreur ou raison d'arrêt
    bool           cfg_valide;            // true = paramètres machine mesurés et valides
} machine_status_t;

// --- Résumé de session (affiché en ARRET_FINAL et envoyé par telemetry) ---
typedef struct {
    float    surface_m2;   // Surface totale arrosée (m²)
    float    dose_moy_mm;  // Dose moyenne calculée (mm)
    uint32_t duree_s;      // Durée totale du cycle (s)
    float    volume_m3;    // Volume d'eau consommé estimé (m³)
} session_summary_t;

// --- Prototypes ---
void state_machine_init(void);
void tick_state_machine(void);
void state_machine_get_status(machine_status_t *status);
void state_machine_cmd_start(void);
void state_machine_cmd_stop(void);
void state_machine_cmd_reset(void);
void state_machine_set_time(int64_t timestamp_unix);
