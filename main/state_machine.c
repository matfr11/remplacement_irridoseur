#include "state_machine.h"
#include "telemetry.h"
#include "securites.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "config_nvs.h"
#include "regulation.h"
#include "calculs_mecanique.h"
#include "calculs_hydraulique.h"
#include "machines/machines.h"
#include "abaques/abaques.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>

// Implémentation complète — PR-05 / PR-06
static const char *TAG = "state_machine";

// ---------------------------------------------------------------------------
// État interne
// ---------------------------------------------------------------------------
static SemaphoreHandle_t s_mutex;

static etat_machine_t       s_etat      = ETAT_VEILLE;
static sous_etat_poumon_t   s_sous_etat = SOUS_VIDANGE;
static machine_status_t     s_status;

static config_machine_t     s_cfg_machine;
static config_programme_t   s_cfg_prog;
static const machine_profile_t *s_profil  = NULL;
static const canon_abaque_t    *s_abaque  = NULL;

// Timers états (ms depuis entrée dans l'état)
static int64_t  s_t_etat_ms          = 0;
static int64_t  s_t_sous_etat_ms     = 0;

// Session
static int64_t  s_t_session_debut_ms        = 0;
static float    s_longueur_enroulee         = 0.0f;
static float    s_longueur_derniere_nvs     = 0.0f;  // seuil dernière sauvegarde NVS

// Cycle poumon
static int64_t  s_t_rempl_debut_ms   = 0;
static float    s_t_remplissage_ms   = 0.0f;
static float    s_t_attente_ms       = 0.0f;
static int      s_nb_tentatives      = 0;

// Pause pression
static int64_t  s_t_pause_debut_ms   = 0;
static int32_t  s_duree_pause_ms     = 0;

// Bilan de session
static session_summary_t s_session;
static bool              s_bilan_envoye = false;

#ifdef CONFIG_IRRI_ENABLE_TESTS
static bool s_sim_active      = false;
static bool s_sim_pression    = true;
static bool s_sim_fin_course  = false;
static bool s_sim_secu_spires = false;
#endif

// Heure estimée arrivée
static int64_t  s_heure_base_unix    = 0;
static bool     s_heure_synchro      = false;

static inline int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void entrer_etat(etat_machine_t nouvel_etat)
{
    ESP_LOGI(TAG, "État : %d → %d", s_etat, nouvel_etat);
    s_etat     = nouvel_etat;
    s_t_etat_ms = now_ms();
}

static void entrer_sous_etat(sous_etat_poumon_t ss)
{
    s_sous_etat      = ss;
    s_t_sous_etat_ms = now_ms();
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void state_machine_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_status, 0, sizeof(s_status));

    config_nvs_lire_machine(&s_cfg_machine);
    int prog_idx = 0;
    config_nvs_lire_prog_actif(&prog_idx);
    config_nvs_lire_programme(prog_idx, &s_cfg_prog);
    config_nvs_lire_longueur(&s_longueur_enroulee);
    s_longueur_derniere_nvs = s_longueur_enroulee;
    if (s_longueur_enroulee > 0.0f) {
        ESP_LOGW(TAG, "Longueur restauree depuis NVS : %.1fm", s_longueur_enroulee);
    }

    s_profil = machine_get(s_cfg_machine.machine_active);
    s_abaque = abaque_get(s_profil->abaque_idx);

    // Résolution double entrée profil
    machine_profile_t profil_copy = *s_profil;
    if (!machine_resoudre_double_entree(&profil_copy)) {
        ESP_LOGE(TAG, "Profil machine invalide (largeur et spires tous les deux à 0)");
    }

    s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);

    // Raison urgence persistante depuis NVS
    char raison_nvs[64] = "";
    config_nvs_lire_urgence(raison_nvs, sizeof(raison_nvs));
    if (raison_nvs[0] != '\0') {
        ESP_LOGW(TAG, "Dernier arrêt urgence : %s", raison_nvs);
        strncpy(s_status.raison_arret, raison_nvs, sizeof(s_status.raison_arret) - 1);
    }

    gpio_all_ev_off();
    entrer_etat(ETAT_VEILLE);
    ESP_LOGI(TAG, "Machine d'états initialisée");
}

// ---------------------------------------------------------------------------
// Tick principal — appelé toutes les 100ms depuis state_machine_task
// ---------------------------------------------------------------------------
void tick_state_machine(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // SEC priorité absolue — avant tout traitement état
    securites_watchdog();

    entrees_t e;
    gpio_handler_lire_entrees(&e);
#ifdef CONFIG_IRRI_ENABLE_TESTS
    if (s_sim_active) {
        e.pression_ok  = s_sim_pression;
        e.fin_course   = s_sim_fin_course;
        e.secu_spires  = s_sim_secu_spires;
    }
#endif
    int64_t t_dans_etat = now_ms() - s_t_etat_ms;

    switch (s_etat) {

    // -----------------------------------------------------------------------
    case ETAT_VEILLE:
        s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);
        if (e.pression_ok && !e.fin_course && s_status.cfg_valide) {
            gpio_ev_canon_set(true);
            entrer_etat(ETAT_OUVERTURE_CANON);
        }
        break;

    // -----------------------------------------------------------------------
    case ETAT_OUVERTURE_CANON: {
        static int s_tick_pression_stable = 0;
        if (e.pression_ok) {
            s_tick_pression_stable++;
        } else {
            s_tick_pression_stable = 0;
        }
        if (s_tick_pression_stable >= 30) {  // 30 × 100ms = 3s
            s_tick_pression_stable = 0;
            s_t_session_debut_ms = now_ms();
            s_duree_pause_ms = 0;
            s_bilan_envoye   = false;
            regulation_reset_calibration();
            s_longueur_enroulee = 0.0f;
            if (s_cfg_prog.tempo_depart_on && s_cfg_prog.tempo_depart_s > 0) {
                entrer_etat(ETAT_TEMPO_DEPART);
            } else {
                gpio_ev_poumon_set(true);
                s_nb_tentatives = 0;
                entrer_etat(ETAT_REMPLISSAGE_POUMON);
            }
        }
        break;
    }

    // -----------------------------------------------------------------------
    case ETAT_TEMPO_DEPART:
        if (!e.pression_ok) {
            gpio_ev_canon_set(false);
            entrer_etat(ETAT_PAUSE_PRESSION);
        } else if (t_dans_etat >= (int64_t)s_cfg_prog.tempo_depart_s * 1000) {
            gpio_ev_poumon_set(true);
            s_nb_tentatives = 0;
            entrer_etat(ETAT_REMPLISSAGE_POUMON);
        }
        break;

    // -----------------------------------------------------------------------
    case ETAT_REMPLISSAGE_POUMON: {
        bool canon_deja_ouvert = (s_nb_tentatives > 0) || (s_etat == ETAT_REMPLISSAGE_POUMON);
        // EV_CANON=ON si re-remplissage pendant cycle (poumon déjà allumé depuis avant)
        // EV_CANON=OFF au premier remplissage initial (sera mis ON en entrant EN_COURS)
        (void)canon_deja_ouvert;

        if (!e.pression_ok) {
            gpio_all_ev_off();
            entrer_etat(ETAT_PAUSE_PRESSION);
        } else if (e.poumon_plein) {
            gpio_ev_poumon_set(false);
            gpio_ev_canon_set(true);
            entrer_etat(ETAT_EN_COURS);
            entrer_sous_etat(SOUS_VIDANGE);
        } else if (t_dans_etat >= 20000) {  // timeout 20s tentative
            gpio_ev_poumon_set(false);
            s_nb_tentatives++;
            if (s_nb_tentatives >= 2) {
                gpio_ev_canon_set(false);
                state_machine_declencher_urgence("Remplissage poumon echoue (2/2)");
            } else {
                // Attendre t_vidange puis retenter
                entrer_etat(ETAT_REMPLISSAGE_POUMON);  // ré-entrée — t_vidange géré PR-05
            }
        }
        break;
    }

    // -----------------------------------------------------------------------
    case ETAT_EN_COURS: {
        if (!e.pression_ok) {
            gpio_all_ev_off();
            s_t_pause_debut_ms = now_ms();
            entrer_etat(ETAT_PAUSE_PRESSION);
            break;
        }

        // Fin de course : transition vers arrivée ou arrêt final
        if (e.fin_course) {
            gpio_ev_poumon_set(false);
            if (s_cfg_prog.tempo_arrivee_on && s_cfg_prog.tempo_arrivee_s > 0) {
                entrer_etat(ETAT_TEMPO_ARRIVEE);
            } else {
                gpio_ev_canon_set(false);
                entrer_etat(ETAT_ARRET_FINAL);
            }
            break;
        }

        gpio_handler_tick_cycle();

        switch (s_sous_etat) {

        case SOUS_VIDANGE: {
            int64_t t_vidange_ms = (int64_t)(s_cfg_machine.t_vidange_s * 1000.0f);
            if (t_vidange_ms <= 0) t_vidange_ms = 1;  // sécurité minimum
            if (now_ms() - s_t_sous_etat_ms >= t_vidange_ms) {
                entrer_sous_etat(SOUS_ATTENTE);
            }
            break;
        }

        case SOUS_ATTENTE: {
            int64_t t_attente_ms = (int64_t)s_t_attente_ms;
            if (now_ms() - s_t_sous_etat_ms >= t_attente_ms) {
                gpio_ev_poumon_set(true);
                gpio_reset_impulsions_cycle();
                s_t_rempl_debut_ms = now_ms();
                s_nb_tentatives = 0;
                entrer_sous_etat(SOUS_REMPLISSAGE);
            }
            break;
        }

        case SOUS_REMPLISSAGE: {
            int64_t t_timeout_ms = (int64_t)(s_cfg_machine.t_rempl_fixe_s > 0
                                              ? s_cfg_machine.t_rempl_fixe_s * 1000.0f
                                              : 20000.0f);
            bool fin_rempl = false;

            if (s_cfg_machine.mode_deg_poumon) {
                // Mode dégradé B : temporisé
                fin_rempl = (now_ms() - s_t_rempl_debut_ms >= t_timeout_ms);
            } else {
                // Mode normal : contact poumon plein
                if (e.poumon_plein) {
                    fin_rempl = true;
                } else if (now_ms() - s_t_rempl_debut_ms >= t_timeout_ms) {
                    s_nb_tentatives++;
                    if (s_nb_tentatives >= 2) {
                        gpio_all_ev_off();
                        state_machine_declencher_urgence("Timeout poumon cycle");
                        break;
                    }
                    // Retour VIDANGE pour retenter
                    gpio_ev_poumon_set(false);
                    entrer_sous_etat(SOUS_VIDANGE);
                    break;
                }
            }

            if (fin_rempl) {
                gpio_ev_poumon_set(false);
                s_t_remplissage_ms = (float)(now_ms() - s_t_rempl_debut_ms);

                // Auto-calibration dist_par_cycle
                uint32_t impulsions = gpio_get_impulsions();
                int etage = calcul_etage_courant(s_longueur_enroulee, s_profil);
                float r    = calcul_rayon_etage(etage, s_profil);
                float dpulse = calcul_dist_pulse_m(r) * s_cfg_machine.facteur_correction;
                float dist_cycle = (float)impulsions * dpulse;

                if (dist_cycle > 0.0f) {
                    float dist_moy = regulation_update_dist_par_cycle(dist_cycle);
                    s_cfg_machine.dist_cycle_nvs = dist_moy;
                    s_longueur_enroulee += dist_cycle;
                    if (s_longueur_enroulee - s_longueur_derniere_nvs >= 5.0f) {
                        config_nvs_sauver_longueur(s_longueur_enroulee);
                        s_longueur_derniere_nvs = s_longueur_enroulee;
                    }

                    // Recalcul T_attente feedforward
                    float v_cible_m_h = lookup_vitesse_cible(
                        s_abaque,
                        s_cfg_prog.pression_bar,
                        (float)s_cfg_prog.buse_mm,
                        s_cfg_prog.dose_mm,
                        NULL, NULL);
                    float v_cible_m_s = v_cible_m_h / 3600.0f;
                    bool alerte = false;
                    float t_att = calcul_t_attente_s(
                        dist_moy, v_cible_m_s,
                        s_t_remplissage_ms / 1000.0f,
                        s_cfg_machine.t_vidange_s,
                        &alerte);

                    // Correction Kp après n_cycles_calib
                    if (regulation_get_nb_cycles() >= s_cfg_machine.n_cycles_calib) {
                        float v_reel = gpio_get_vitesse_m_h(s_cfg_machine.facteur_correction);
                        t_att = correction_vitesse(t_att, v_reel, v_cible_m_h, s_cfg_machine.kp_regulation);
                    }

                    s_t_attente_ms = t_att * 1000.0f;
                    s_status.alerte_pression_insuff = alerte;
                }

                entrer_sous_etat(SOUS_VIDANGE);
            }
            break;
        }
        }  // switch sous_etat
        break;
    }

    // -----------------------------------------------------------------------
    case ETAT_PAUSE_PRESSION:
        s_duree_pause_ms += 100;
        // Reprise automatique au retour pression
        if (e.pression_ok) {
            gpio_ev_canon_set(true);
            entrer_etat(ETAT_EN_COURS);
            entrer_sous_etat(SOUS_VIDANGE);
        }
        break;

    // -----------------------------------------------------------------------
    case ETAT_TEMPO_ARRIVEE:
        if (!e.pression_ok) {
            gpio_ev_canon_set(false);
            entrer_etat(ETAT_ARRET_FINAL);
        } else if (t_dans_etat >= (int64_t)s_cfg_prog.tempo_arrivee_s * 1000) {
            gpio_ev_canon_set(false);
            entrer_etat(ETAT_ARRET_FINAL);
        }
        break;

    // -----------------------------------------------------------------------
    case ETAT_ARRET_FINAL:
        if (!s_bilan_envoye) {
            s_session.longueur_m              = s_longueur_enroulee;
            s_session.surface_m2              = s_status.surface_m2;
            s_session.dose_moy_mm             = s_status.dose_inst_mm;
            s_session.volume_m3               = s_status.surface_m2 * s_status.dose_inst_mm / 1000.0f;
            s_session.duree_s                 = s_status.duree_s;
            s_session.nb_cycles               = (uint32_t)regulation_get_nb_cycles();
            s_session.duree_pause_pression_s  = s_duree_pause_ms / 1000;
            telemetry_envoyer_bilan();
            s_bilan_envoye = true;
        }
        // Retour VEILLE automatique quand l'opérateur déroule (fin_course disparaît)
        if (!e.fin_course) {
            s_longueur_enroulee = 0.0f;
            s_longueur_derniere_nvs = 0.0f;
            config_nvs_sauver_longueur(0.0f);
            regulation_reset_calibration();
            config_nvs_sauver_machine(&s_cfg_machine);
            memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
            entrer_etat(ETAT_VEILLE);
        }
        break;

    // -----------------------------------------------------------------------
    case ETAT_ARRET_URGENCE:
        // Rien — cmd_reset manuel requis
        break;

    }  // switch etat

    // Mode dégradé A — estimation vitesse depuis cycles poumon
    if (s_cfg_machine.mode_deg_vitesse && s_etat == ETAT_EN_COURS) {
        float t_cycle_s = (s_t_remplissage_ms + s_t_attente_ms) / 1000.0f
                        + s_cfg_machine.t_vidange_s;
        float v_est = (t_cycle_s > 0.5f)
                    ? (s_cfg_machine.dist_cycle_nvs * 60.0f / t_cycle_s)
                    : 0.0f;
        gpio_handler_set_vitesse_estimee(v_est);
    }

    // Mise à jour statut diffusé
    s_status.etat         = s_etat;
    s_status.sous_etat    = s_sous_etat;
    s_status.ev_canon     = gpio_get_level(PIN_EV_CANON)  != 0;
    s_status.ev_poumon    = gpio_get_level(PIN_EV_POUMON) != 0;
    s_status.pression_ok  = e.pression_ok;
    s_status.fin_course   = e.fin_course;
    s_status.secu_spires  = e.secu_spires;
    s_status.poumon_plein = e.poumon_plein;
    s_status.longueur_enroulee_m = s_longueur_enroulee;
    if (s_t_session_debut_ms > 0) {
        s_status.duree_s = (int32_t)((now_ms() - s_t_session_debut_ms) / 1000);
    }
    s_status.t_remplissage_ms = (int32_t)s_t_remplissage_ms;
    s_status.t_attente_ms     = (int32_t)s_t_attente_ms;
    s_status.cycles_total     = (uint32_t)regulation_get_nb_cycles();
    s_status.facteur_correction   = s_cfg_machine.facteur_correction;
    s_status.heure_synchro        = s_heure_synchro;
    s_status.mode_degrade_vitesse = s_cfg_machine.mode_deg_vitesse;
    s_status.mode_degrade_poumon  = s_cfg_machine.mode_deg_poumon;

    // Champs config machine (pour initialisation de l'UI)
    s_status.cfg_t_vidange_s      = s_cfg_machine.t_vidange_s;
    s_status.cfg_kp_regulation    = s_cfg_machine.kp_regulation;
    s_status.cfg_n_cycles_calib   = s_cfg_machine.n_cycles_calib;
    s_status.cfg_fenetre_vitesse  = s_cfg_machine.fenetre_vitesse;
    s_status.cfg_max_cycles_si    = s_cfg_machine.max_cycles_si;
    s_status.cfg_t_rempl_fixe_s   = s_cfg_machine.t_rempl_fixe_s;
    s_status.cfg_denivele_m       = s_cfg_machine.denivele_m;
    s_status.cfg_machine_active   = s_cfg_machine.machine_active;

    xSemaphoreGive(s_mutex);
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------
void state_machine_get_status(machine_status_t *status)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGive(s_mutex);
}

etat_machine_t state_machine_get_etat(void)
{
    return s_etat;
}

void state_machine_cmd_start(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "cmd_start reçu");
        // Les conditions seront évaluées au prochain tick
    }
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_stop(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_etat != ETAT_VEILLE &&
        s_etat != ETAT_ARRET_FINAL &&
        s_etat != ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "cmd_stop reçu depuis état %d", s_etat);
        gpio_all_ev_off();
        entrer_etat(ETAT_ARRET_FINAL);
    }
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_reset(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "cmd_reset — sortie urgence");
        memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
        config_nvs_sauver_urgence("");
        s_longueur_enroulee = 0.0f;
        s_longueur_derniere_nvs = 0.0f;
        config_nvs_sauver_longueur(0.0f);
        regulation_reset_calibration();
        config_nvs_sauver_machine(&s_cfg_machine);
        entrer_etat(ETAT_VEILLE);
    }
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_set_longueur(float longueur_m)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE ||
        s_etat == ETAT_ARRET_FINAL ||
        s_etat == ETAT_ARRET_URGENCE) {
        s_longueur_enroulee     = longueur_m;
        s_longueur_derniere_nvs = longueur_m;
        config_nvs_sauver_longueur(longueur_m);
        ESP_LOGI(TAG, "Longueur forcee : %.1fm", longueur_m);
    } else {
        ESP_LOGW(TAG, "cmd_set_longueur ignoree en etat %d", s_etat);
    }
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_etalonner(float longueur_reelle_m)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    float facteur = 0.0f;
    bool ok = calcul_facteur_etalonnage(
        s_longueur_enroulee,
        longueur_reelle_m,
        (int)gpio_get_impulsions(),
        &facteur);
    if (ok) {
        s_cfg_machine.facteur_correction = facteur;
        config_nvs_sauver_machine(&s_cfg_machine);
        ESP_LOGI(TAG, "Étalonnage sauvegardé : facteur=%.3f", facteur);
    }
    xSemaphoreGive(s_mutex);
}

void state_machine_set_time(int64_t timestamp_unix)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_heure_base_unix = timestamp_unix;
    s_heure_synchro   = true;
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_ev_canon_set(bool actif)
{
    if (s_etat == ETAT_VEILLE) {
        gpio_ev_canon_set(actif);
    }
}

void state_machine_cmd_ev_poumon_set(bool actif)
{
    if (s_etat == ETAT_VEILLE) {
        gpio_ev_poumon_set(actif);
    }
}

void state_machine_declencher_urgence(const char *raison)
{
    gpio_all_ev_off();
    ESP_LOGE(TAG, "ARRÊT URGENCE : %s", raison);
    if (s_etat != ETAT_ARRET_URGENCE) {
        entrer_etat(ETAT_ARRET_URGENCE);
        strncpy(s_status.raison_arret, raison, sizeof(s_status.raison_arret) - 1);
        config_nvs_sauver_urgence(raison);
    }
}

void state_machine_recharger_config(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_etat != ETAT_VEILLE) {
        xSemaphoreGive(s_mutex);
        return;
    }
    int prog_idx = 0;
    config_nvs_lire_prog_actif(&prog_idx);
    config_nvs_lire_machine(&s_cfg_machine);
    config_nvs_lire_programme(prog_idx, &s_cfg_prog);
    s_profil = machine_get(s_cfg_machine.machine_active);
    s_abaque = abaque_get(s_profil->abaque_idx);
    s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);
    strncpy(s_status.prog_nom,    s_cfg_prog.nom,             sizeof(s_status.prog_nom) - 1);
    strncpy(s_status.machine_nom, s_profil ? s_profil->nom : "", sizeof(s_status.machine_nom) - 1);
    gpio_handler_set_params(s_cfg_machine.fenetre_vitesse, s_cfg_machine.max_cycles_si);
    gpio_handler_set_mode_degrade_a(s_cfg_machine.mode_deg_vitesse);
    s_status.mode_degrade_vitesse = s_cfg_machine.mode_deg_vitesse;
    s_status.mode_degrade_poumon  = s_cfg_machine.mode_deg_poumon;
    xSemaphoreGive(s_mutex);
}

void state_machine_get_session_summary(session_summary_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_session;
    xSemaphoreGive(s_mutex);
}

// ---------------------------------------------------------------------------
// Hooks de test
// ---------------------------------------------------------------------------
#ifdef CONFIG_IRRI_ENABLE_TESTS
void state_machine_test_injecter_etat(etat_machine_t etat)
{
    s_sim_active      = true;
    s_sim_pression    = true;
    s_sim_fin_course  = false;
    s_sim_secu_spires = false;
    entrer_etat(etat);
}
void state_machine_test_set_pression(bool pression_ok)  { s_sim_pression    = pression_ok; }
void state_machine_test_set_fin_course(bool active)      { s_sim_fin_course  = active; }
void state_machine_test_set_secu_spires(bool active)     { s_sim_secu_spires = active; }
int  state_machine_test_get_nb_tentatives(void)          { return s_nb_tentatives; }

void state_machine_test_reset(void)
{
    s_etat              = ETAT_VEILLE;
    s_sous_etat         = SOUS_VIDANGE;
    s_longueur_enroulee = 0.0f;
    s_nb_tentatives     = 0;
    s_bilan_envoye      = false;
    gpio_ev_canon_set(false);
    gpio_ev_poumon_set(false);
}
#endif

