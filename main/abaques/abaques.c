#include "abaques.h"

// Tableau de tous les abaques — terminé par NULL
const canon_abaque_t * const ABAQUES_LISTE[] = {
    &ABAQUE_SR150C,
    NULL,
};
const int ABAQUES_NB = 1;

const canon_abaque_t *abaque_get(int idx)
{
    if (idx < 0 || idx >= ABAQUES_NB) {
        return ABAQUES_LISTE[0];
    }
    return ABAQUES_LISTE[idx];
}
