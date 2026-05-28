#pragma once

#include <stdint.h>
#include <stddef.h>

// =============================================================================
// abaques.h — Tables débit constructeur Irrifrance ESP32
//
// Ajouter un canon :
//   1. Créer main/abaques/[canon].c avec canon_abaque_t
//   2. Déclarer extern ici et ajouter à ABAQUES_LISTE[]
//   3. Ouvrir une PR sur GitHub
// =============================================================================

// Une ligne de la table constructeur
typedef struct {
    float p_enrouleur;  // Pression manomètre enrouleur (bar)
    float q_m3h;        // Débit (m³/h)
    float p_buse;       // Pression effective à la buse (bar)
    float buse_mm;      // Diamètre buse (mm)
    float esp_m;        // Espacement recommandé entre passages (m)
    float D40;          // Vitesse (m/h) pour dose 40mm
    float D30;          // Vitesse (m/h) pour dose 30mm
    float D25;          // Vitesse (m/h) pour dose 25mm
    float D20;          // Vitesse (m/h) pour dose 20mm
    float D15;          // Vitesse (m/h) pour dose 15mm
} canon_entry_t;

typedef struct {
    char          nom[32];
    char          constructeur[32];
    canon_entry_t table[20];    // Max 20 lignes par abaque
    int           nb_entrees;
} canon_abaque_t;

// Abaques disponibles
extern const canon_abaque_t ABAQUE_SR150C;

// Tableau de tous les abaques (terminé par NULL)
extern const canon_abaque_t * const ABAQUES_LISTE[];
extern const int                     ABAQUES_NB;

// Retourne l'abaque à l'index donné, ou le premier si hors bornes.
const canon_abaque_t *abaque_get(int idx);
