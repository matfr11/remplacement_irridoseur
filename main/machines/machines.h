#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// machines.h — Profils machine Irrifrance ESP32
//
// Ajouter un enrouleur :
//   1. Créer main/machines/[nom].c avec machine_profile_t
//   2. Déclarer extern ici et ajouter à MACHINES_LISTE[]
//   3. Ouvrir une PR sur GitHub
// =============================================================================

typedef struct {
    char    nom[32];
    char    constructeur[32];
    float   longueur_tuyau_m;
    float   d_tuyau_ext_m;
    float   r_tambour_vide_m;
    int     nb_etages;
    int     abaque_idx;

    // Double entrée — renseigner UN SEUL des deux.
    // L'autre est calculé automatiquement au chargement.
    float   largeur_bobine_m;       // Mesure terrain (mètre ruban)
    float   spires_par_etage;       // Valeur constructeur ou comptage

    // Paramètres ajustables terrain (modifiables en NVS)
    float   t_vidange_s;            // ⚠️ À mesurer terrain
    float   facteur_correction;     // Étalonnage longueur (défaut 1.0)
} machine_profile_t;

// Profils disponibles
extern const machine_profile_t MACHINE_ST1BIS_82_330;

// Tableau de tous les profils (terminé par NULL)
extern const machine_profile_t * const MACHINES_LISTE[];
extern const int                        MACHINES_NB;

// Retourne le profil à l'index donné, ou le premier si hors bornes.
const machine_profile_t *machine_get(int idx);

// Calcule spires_par_etage depuis largeur_bobine si nécessaire, et inversement.
// Retourne false si le profil est invalide (les deux champs à 0).
bool machine_resoudre_double_entree(machine_profile_t *profil);
