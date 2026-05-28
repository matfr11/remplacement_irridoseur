#include "machines.h"
#include <string.h>

// Tableau de tous les profils — terminé par NULL
const machine_profile_t * const MACHINES_LISTE[] = {
    &MACHINE_ST1BIS_82_330,
    NULL,
};
const int MACHINES_NB = 1;

const machine_profile_t *machine_get(int idx)
{
    if (idx < 0 || idx >= MACHINES_NB) {
        return MACHINES_LISTE[0];
    }
    return MACHINES_LISTE[idx];
}

bool machine_resoudre_double_entree(machine_profile_t *profil)
{
    if (profil->largeur_bobine_m > 0.0f && profil->spires_par_etage == 0.0f) {
        profil->spires_par_etage = profil->largeur_bobine_m / profil->d_tuyau_ext_m;
        return true;
    }
    if (profil->spires_par_etage > 0.0f && profil->largeur_bobine_m == 0.0f) {
        profil->largeur_bobine_m = profil->spires_par_etage * profil->d_tuyau_ext_m;
        return true;
    }
    if (profil->largeur_bobine_m > 0.0f && profil->spires_par_etage > 0.0f) {
        return true;  // les deux fournis — on garde tels quels
    }
    return false;  // les deux à 0 — profil invalide
}
