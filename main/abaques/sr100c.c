/**
 * @file sr100c.c
 * @brief Abaque constructeur Irrifrance — Canon SR 100C
 *
 * Source : Fiche technique Structure 2 AT/P
 *          Ø82mm / 330m / Arroseur SR 100C
 *          Irrifrance Cofadsi — 34230 Paulhan
 *
 * Table triée par diamètre de buse croissant,
 * puis par pression buse croissante à buse égale.
 *
 * Colonnes :
 *   p_enrouleur : Pression enrouleur (bar)
 *   q_m3h       : Débit (m³/h)
 *   p_buse      : Pression effective à la buse (bar)
 *   buse_mm     : Diamètre buse (mm)
 *   esp_m       : Espacement conseillé (m)
 *   D40..D15    : Vitesse (m/h) pour dose 40/30/25/20/15mm
 *
 * Corrections appliquées vs fiche originale :
 *   - Ligne 6.1 / buse 15.2 / p=5.0 : D20 corrigé 22.1→16.8
 *     (valeur recalculée : Q×1000/(Esp×Dose) = 20.1×1000/(60×20) = 16.75)
 *
 * Coefficients empiriques (dérivés par régression sur portée constructeur) :
 *   k_q              = 0.039  (moyenne Q/(buse²×√p_buse) sur 10 diamètres)
 *   k_portee         = 7.70   (fit portee = k × buse^0.55 × (p/3.5)^0.31)
 *   portee_exp_buse  = 0.55   (ratio portées buse19/buse25.4 à p fixe)
 *   portee_exp_p     = 0.31   (variation portée buse19 entre p=4,5,6 bar)
 *   esp_factor       = 1.53   (moyenne esp_constructeur/portee_calculée)
 */

#include "abaques.h"

const canon_abaque_t ABAQUE_SR100C = {
    .nom              = "SR 100C",
    .constructeur     = "Irrifrance",
    .nb_entrees       = 25,
    .k_q              = 0.039f,
    .k_portee         = 7.70f,
    .portee_exp_buse  = 0.55f,
    .portee_exp_p     = 0.31f,
    .esp_factor       = 1.53f,
    .table = {
     // p_enr   Q      p_buse  buse    esp     D40    D30    D25    D20    D15
        /* ── Buse 12.7mm ─────────────────────────────────────────── */
        {4.5f,  12.2f,  4.0f,  12.7f,  48.0f,  6.4f,  8.5f, 10.2f, 12.7f, 16.9f},
        {6.7f,  15.0f,  6.0f,  12.7f,  54.0f,  6.9f,  9.3f, 11.1f, 13.9f, 18.5f},

        /* ── Buse 14.0mm ─────────────────────────────────────────── */
        {4.7f,  15.3f,  4.0f,  14.0f,  54.0f,  7.1f,  9.4f, 11.3f, 14.2f, 18.9f},
        {5.8f,  17.0f,  5.0f,  14.0f,  54.0f, 10.3f, 13.7f, 16.4f, 20.5f, 27.3f},
        {7.0f,  18.5f,  6.0f,  14.0f,  60.0f,  7.7f, 10.3f, 12.3f, 15.4f, 20.6f},

        /* ── Buse 15.2mm ─────────────────────────────────────────── */
        {4.9f,  18.0f,  4.0f,  15.2f,  54.0f,  8.3f, 11.1f, 13.3f, 16.7f, 22.2f},
        {6.1f,  20.1f,  5.0f,  15.2f,  60.0f,  8.4f, 11.2f, 13.4f, 16.8f, 22.3f}, // D20 corrigé 22.1→16.8
        {7.3f,  22.1f,  6.0f,  15.2f,  63.0f,  8.8f, 11.7f, 14.0f, 17.5f, 23.4f},

        /* ── Buse 16.5mm ─────────────────────────────────────────── */
        {5.2f,  21.1f,  4.0f,  16.5f,  54.0f,  9.8f, 13.0f, 15.6f, 19.5f, 26.0f},
        {7.7f,  25.9f,  6.0f,  16.5f,  66.0f,  9.8f, 13.1f, 15.7f, 19.6f, 26.2f},

        /* ── Buse 17.8mm ─────────────────────────────────────────── */
        {5.6f,  24.6f,  4.0f,  17.8f,  60.0f,  6.3f,  8.4f, 10.1f, 12.6f, 16.8f},
        {6.9f,  27.5f,  5.0f,  17.8f,  66.0f, 10.4f, 13.9f, 16.7f, 20.8f, 27.8f},
        {8.2f,  30.1f,  6.0f,  17.8f,  66.0f, 11.4f, 15.2f, 18.2f, 22.8f, 30.4f},

        /* ── Buse 19.0mm ─────────────────────────────────────────── */
        {5.9f,  27.8f,  4.0f,  19.0f,  63.0f,  7.9f, 10.5f, 12.6f, 15.7f, 21.0f},
        {7.4f,  31.2f,  5.0f,  19.0f,  66.0f, 11.8f, 15.8f, 18.9f, 23.6f, 31.5f},
        {8.8f,  34.3f,  6.0f,  19.0f,  72.0f, 11.9f, 15.9f, 19.1f, 23.8f, 31.8f},

        /* ── Buse 20.3mm ─────────────────────────────────────────── */
        {6.4f,  31.2f,  4.0f,  20.3f,  66.0f, 11.8f, 15.2f, 18.9f, 23.6f, 25.3f}, // D15 à vérifier
        {7.9f,  34.9f,  5.0f,  20.3f,  72.0f, 12.1f, 16.2f, 19.4f, 24.2f, 32.3f},
        {9.4f,  38.2f,  6.0f,  20.3f,  72.0f, 13.3f, 17.7f, 21.2f, 26.5f, 35.4f},

        /* ── Buse 21.6mm ─────────────────────────────────────────── */
        {7.1f,  36.2f,  4.0f,  21.6f,  66.0f, 13.7f, 18.3f, 21.9f, 27.4f, 36.6f},
        {8.8f,  40.5f,  5.0f,  21.6f,  72.0f, 14.1f, 18.7f, 22.5f, 28.1f, 37.5f},

        /* ── Buse 22.9mm ─────────────────────────────────────────── */
        {7.7f,  40.4f,  4.0f,  22.9f,  66.0f, 15.3f, 20.4f, 24.5f, 30.6f, 40.8f},
        {9.5f,  45.2f,  5.0f,  22.9f,  72.0f, 15.7f, 20.9f, 25.1f, 31.4f, 41.9f},

        /* ── Buse 25.4mm ─────────────────────────────────────────── */
        {9.3f,  49.5f,  4.0f,  25.4f,  72.0f, 17.2f, 22.9f, 27.5f, 34.4f, 45.8f},
    },
};
