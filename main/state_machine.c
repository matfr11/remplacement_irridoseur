#include "state_machine.h"
#include "batterie.h"
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
#include "esp_system.h"
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
static float    s_longueur_enroulee         = 0.0f;  // position absolue interne (calculs mécaniques)
static float    s_longueur_derniere_nvs     = 0.0f;
static float    s_longueur_deroule_m        = 0.0f;  // longueur déployée en champ — constante pendant session (affiché "Déroulé")
static float    s_longueur_session_m        = 0.0f;  // progression depuis début session — croît de 0 à déroulée (affiché "Enroulé")

// Cycle poumon
static int64_t  s_t_rempl_debut_ms         = 0;
static int64_t  s_t_retry_vidange_fin_ms   = 0;
static float    s_t_remplissage_ms         = 0.0f;
static int32_t  s_t_attente_ms             = 0;
static int      s_nb_tentatives            = 0;

// Pause pression
static int64_t  s_t_pause_debut_ms   = 0;
static int32_t  s_duree_pause_ms     = 0;  // bilan uniquement — déborde après 24,8 jours de pause cumulée (impossible en pratique)

// Déroulement
static float    s_mesure_deroule_m   = 0.0f;  // longueur déployée mesurée en ETAT_DEROULE
static bool     s_fc_deroule_prev    = false;  // front descendant fin_course → entrée DEROULE

// Bilan de session
static session_summary_t s_session;
static bool              s_bilan_envoye = false;
static config_stats_t    s_stats;

// Batterie
static batt_status_t s_batt              = {0};
static float         s_batt_warn_v       = 11.5f;
static float         s_batt_crit_v       = 11.0f;
static int           s_batt_tick         = 299;  // declenche mesure au 1er tick
static bool          s_batt_alerte_active = false; // poll 5s si tension < warn

// Doit être true pour que VEILLE puisse démarrer (protège contre redémarrage auto après stop/urgence)
static bool              s_demarrage_autorise = true;  // false uniquement après ARRET_URGENCE

// Coupure de courant détectée au boot (session_active=1 sans urgence au redémarrage)
static bool              s_coupure_detectee   = false;

// Heartbeat circuit RC (PIN_HEARTBEAT, GPIO 23) — conditionnel sur cfg.heartbeat_rc_on
static bool              s_heartbeat_level    = false;
static int64_t           s_heartbeat_ms       = 0;

// Vitesse max et dose corrigée — calculées quand T_attente < 0 (alerte_pression_insuff)
static float             s_vitesse_max_m_h    = 0.0f;
static float             s_dose_corrigee_mm   = 0.0f;

// Temps de remplissage minimum historique — reference pour V_max theorique
static float             s_t_rempl_min_s      = 5.0f;

#ifdef CONFIG_IRRI_WOKWI_MODE
static volatile bool s_wokwi_pression_ok = true;
void state_machine_wokwi_set_pression(bool ok) { s_wokwi_pression_ok = ok; }
#endif

#ifdef CONFIG_IRRI_ENABLE_TESTS
static bool s_sim_active      = false;
static bool s_sim_pression    = true;
static bool s_sim_fin_course  = false;
static bool s_sim_secu_spires = false;
#endif

// Absence pression OUVERTURE_CANON — remis à 0 dans entrer_etat
static int   s_tick_pression_absente = 0;
static bool  s_reprise_pression      = false;  // entrée OUVERTURE_CANON depuis PAUSE_PRESSION
static bool    s_tempo_depart_effectuee  = false; // tempo départ terminée (ou non configurée)
static int64_t s_tempo_depart_ecoulee_ms = 0;     // temps de tempo déjà écoulé avant interruption — la reprise ne fait que le restant

// Heure estimée arrivée
static int64_t  s_heure_base_unix    = 0;
static bool     s_heure_synchro      = false;
static float    s_vitesse_cible_m_h  = 0.0f;

static inline int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void entrer_etat(etat_machine_t nouvel_etat)
{
    ESP_LOGI(TAG, "%d -> %d", s_etat, nouvel_etat);
    s_etat     = nouvel_etat;
    s_t_etat_ms = now_ms();
    if (nouvel_etat == ETAT_OUVERTURE_CANON) {
        s_tick_pression_absente = 0;
    }
    if (nouvel_etat == ETAT_REMPLISSAGE_POUMON) {
        // Chrono de remplissage : démarre maintenant (1re tentative, EV déjà ouverte
        // par l'appelant). En retry, il est re-armé à la réouverture de l'EV après vidange.
        s_t_rempl_debut_ms = now_ms();
    }
}

static void entrer_sous_etat(sous_etat_poumon_t ss)
{
    s_sous_etat      = ss;
    s_t_sous_etat_ms = now_ms();
}

// ---------------------------------------------------------------------------
// Rechargement config depuis NVS — appelé sans mutex (dans contexte tick ou init)
// ---------------------------------------------------------------------------
static void charger_config_interne(void)
{
    int prog_idx = 0;
    config_nvs_lire_prog_actif(&prog_idx);
    config_nvs_charger_machine(&s_cfg_machine);
    config_nvs_lire_programme(prog_idx, &s_cfg_prog);
    s_profil = machine_get(s_cfg_machine.machine_active);
    s_abaque = abaque_get(s_cfg_machine.abaque_idx);
    // Fallback cycles_par_tour : 0 en NVS → utiliser la valeur du profil machine
    if (s_cfg_machine.cycles_par_tour <= 0.0f && s_profil && s_profil->cycles_par_tour > 0.0f) {
        s_cfg_machine.cycles_par_tour = s_profil->cycles_par_tour;
    }
    s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);
    strncpy(s_status.prog_nom,    s_cfg_prog.nom,             sizeof(s_status.prog_nom) - 1);
    s_status.prog_nom[sizeof(s_status.prog_nom) - 1] = '\0';
    strncpy(s_status.machine_nom, s_profil ? s_profil->nom : "", sizeof(s_status.machine_nom) - 1);
    s_status.machine_nom[sizeof(s_status.machine_nom) - 1] = '\0';
    strncpy(s_status.abaque_nom,  s_abaque ? s_abaque->nom : "", sizeof(s_status.abaque_nom) - 1);
    s_status.abaque_nom[sizeof(s_status.abaque_nom) - 1] = '\0';
    ESP_LOGI(TAG, "Config chargee: prog=%s machine=%s abaque=%s",
             s_status.prog_nom, s_status.machine_nom, s_status.abaque_nom);
    s_status.prog_dose_mm         = s_cfg_prog.dose_mm;
    s_status.prog_largeur_m       = s_cfg_prog.largeur_m;
    s_status.prog_buse_mm         = s_cfg_prog.buse_mm;
    s_status.prog_pression_bar    = s_cfg_prog.pression_bar;
    s_status.prog_tempo_depart_on = s_cfg_prog.tempo_depart_on;
    s_status.prog_tempo_depart_s  = s_cfg_prog.tempo_depart_s;
    s_status.prog_tempo_arrivee_on= s_cfg_prog.tempo_arrivee_on;
    s_status.prog_tempo_arrivee_s = s_cfg_prog.tempo_arrivee_s;
    gpio_handler_set_vitesse_depuis_cycles_poumon(s_cfg_machine.cycles_par_tour > 0.0f);
    s_status.mode_degrade_poumon  = s_cfg_machine.mode_deg_poumon;
    s_status.mode_degrade_spires  = s_cfg_machine.mode_deg_spires;
    securites_set_bypass_spires(s_cfg_machine.mode_deg_spires);
    s_vitesse_cible_m_h = (s_abaque && s_cfg_prog.buse_mm > 0)
        ? lookup_vitesse_cible(s_abaque, s_cfg_prog.pression_bar,
                               (float)s_cfg_prog.buse_mm, s_cfg_prog.dose_mm,
                               s_cfg_prog.largeur_m, NULL, NULL)
        : 0.0f;
    config_nvs_lire_batt_seuils(&s_batt_warn_v, &s_batt_crit_v);
    batterie_set_seuils(s_batt_warn_v, s_batt_crit_v);
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void state_machine_init(void)
{
    s_mutex = xSemaphoreCreateRecursiveMutex();
    memset(&s_status, 0, sizeof(s_status));

    charger_config_interne();
    config_nvs_lire_stats(&s_stats);
    config_nvs_lire_t_rempl_min(&s_t_rempl_min_s);

    config_nvs_lire_longueur(&s_longueur_enroulee);
    config_nvs_lire_deroule(&s_longueur_deroule_m);
    s_longueur_derniere_nvs = s_longueur_enroulee;

    if (s_longueur_deroule_m > 0.0f) {
        // Reboot mid-session : recalcul progression depuis position absolue
        float total = (s_profil && s_profil->longueur_tuyau_m > 0.0f) ? s_profil->longueur_tuyau_m : 0.0f;
        float abs_start = total - s_longueur_deroule_m;
        s_longueur_session_m = s_longueur_enroulee - abs_start;
        if (s_longueur_session_m < 0.0f) s_longueur_session_m = 0.0f;
        ESP_LOGW(TAG, "Reboot mid-session restaure : deroule=%.1fm enroule_session=%.1fm",
                 s_longueur_deroule_m, s_longueur_session_m);
    } else if (s_longueur_enroulee > 0.0f) {
        // Ancien format NVS (pas de clé deroule) : initialisation conservative
        float total = (s_profil && s_profil->longueur_tuyau_m > 0.0f) ? s_profil->longueur_tuyau_m : 0.0f;
        s_longueur_deroule_m = total;
        s_longueur_session_m = s_longueur_enroulee;
        ESP_LOGW(TAG, "Longueur restauree (ancien NVS) : enroule=%.1fm", s_longueur_enroulee);
    }

    // Résolution double entrée profil
    if (s_profil) {
        machine_profile_t profil_copy = *s_profil;
        if (!machine_resoudre_double_entree(&profil_copy)) {
            ESP_LOGE(TAG, "Profil machine invalide (largeur et spires tous les deux à 0)");
        }
    } else {
        ESP_LOGE(TAG, "Profil machine introuvable (machine_active=%d)", s_cfg_machine.machine_active);
    }

    // Détection coupure de courant : session_active=1 sans raison d'urgence
    bool session_etait_active = config_nvs_lire_session_active();

    // Raison urgence persistante depuis NVS
    char raison_nvs[64] = "";
    config_nvs_lire_urgence(raison_nvs, sizeof(raison_nvs));
    if (raison_nvs[0] != '\0') {
        ESP_LOGW(TAG, "Dernier arret urgence : %s", raison_nvs);
        strncpy(s_status.raison_arret, raison_nvs, sizeof(s_status.raison_arret) - 1);
        s_status.raison_arret[sizeof(s_status.raison_arret) - 1] = '\0';
        s_demarrage_autorise = false;  // reboot après urgence : démarrage explicite requis
    } else {
        s_demarrage_autorise = true;   // démarrage auto dès mise en pression
        if (session_etait_active && s_longueur_enroulee > 0.0f) {
            s_coupure_detectee = true;
            if (s_cfg_machine.reprise_auto_on) {
                // Reprise auto seulement sur reboot inattendu (TPL5010/crash).
                // Sur POWERON (opérateur coupe l'alim) ou reboot logiciel : attendre.
                esp_reset_reason_t raison = esp_reset_reason();
                bool reboot_inattendu = (raison == ESP_RST_EXT      ||
                                         raison == ESP_RST_PANIC     ||
                                         raison == ESP_RST_TASK_WDT  ||
                                         raison == ESP_RST_WDT);
                if (reboot_inattendu) {
                    s_demarrage_autorise = true;
                    ESP_LOGW(TAG, "Coupure detectee - reprise auto (raison=%d enroule=%.1fm)",
                             raison, s_longueur_enroulee);
                } else {
                    s_demarrage_autorise = false;
                    ESP_LOGW(TAG, "Coupure detectee - reprise auto ignoree (mise sous tension normale raison=%d)",
                             raison);
                }
            } else {
                s_demarrage_autorise = false;  // attendre choix opérateur (reprendre ou reset)
                ESP_LOGW(TAG, "Coupure detectee - session interrompue (enroule=%.1fm)",
                         s_longueur_enroulee);
            }
        }
    }

    // Initialiser GPIO heartbeat RC (sortie, défaut LOW)
    gpio_config_t hb_cfg = {
        .pin_bit_mask = (1ULL << PIN_HEARTBEAT),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&hb_cfg);
    gpio_set_level(PIN_HEARTBEAT, 0);

    gpio_all_ev_off();
    entrer_etat(ETAT_VEILLE);
    ESP_LOGI(TAG, "machine initialisee");
}

// ---------------------------------------------------------------------------
// Handlers d'état — un par case du switch principal
// ---------------------------------------------------------------------------

static void handle_veille(const entrees_t *e, int64_t t_dans_etat)
{
    (void)t_dans_etat;
    s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);
    if (s_fc_deroule_prev && !e->fin_course) {
        s_mesure_deroule_m = 0.0f;  // nouveau déroulage — mesure repart de 0
        gpio_reset_impulsions_cycle();
        entrer_etat(ETAT_DEROULE);
        s_fc_deroule_prev = e->fin_course;
        return;
    }
    s_fc_deroule_prev = e->fin_course;
    if (s_demarrage_autorise && e->pression_ok && !e->fin_course && s_status.cfg_valide) {
        s_demarrage_autorise = false;
        s_coupure_detectee = false;
        s_reprise_pression = false;  // démarrage initial — séquence complète
        config_nvs_sauver_session_active(true);
        gpio_ev_canon_set(true);
        entrer_etat(ETAT_OUVERTURE_CANON);
    }
}

static void handle_ouverture_canon(const entrees_t *e, int64_t t_dans_etat)
{
    // Pression absente 1s d'affilée : ne pas rester bloqué EV_CANON ouverte.
    // Le pressostat peut "clignoter" pendant la montée en pression — d'où le seuil.
    if (e->pression_ok) {
        s_tick_pression_absente = 0;
    } else {
        if (++s_tick_pression_absente >= 10) {
            gpio_ev_canon_set(false);
            if (s_reprise_pression) {
                // Session en cours — retour pause, la reprise retentera le timer
                entrer_etat(ETAT_PAUSE_PRESSION);
            } else {
                // Le démarrage n'a jamais abouti — retour VEILLE, redémarrage auto
                // (timer complet rejoué) à la prochaine pression
                s_demarrage_autorise = true;
                entrer_etat(ETAT_VEILLE);
            }
            return;
        }
    }
    // Timer configurable : laisser la pression se stabiliser dans le canon
    // avant d'actionner le poumon (défaut 20s).
    int64_t seuil_ms = (int64_t)(s_cfg_machine.t_ouv_canon_s * 1000.0f);
    if (t_dans_etat < seuil_ms) return;

    if (s_reprise_pression) {
        // Reprise après PAUSE_PRESSION : timer rejoué. Session préservée.
        s_reprise_pression = false;
        if (!s_tempo_depart_effectuee
            && s_cfg_prog.tempo_depart_on && s_cfg_prog.tempo_depart_s > 0) {
            // Tempo départ interrompue — reprise pour le temps restant uniquement
            // (s_tempo_depart_ecoulee_ms cumule le temps déjà effectué)
            entrer_etat(ETAT_TEMPO_DEPART);
            return;
        }
        // Tempo déjà effectuée (ou non configurée) — pas de re-tempo
        gpio_ev_poumon_set(true);
        s_nb_tentatives = 0;
        entrer_etat(ETAT_REMPLISSAGE_POUMON);
        return;
    }
    s_t_session_debut_ms = now_ms();
    s_duree_pause_ms = 0;
    s_bilan_envoye   = false;
    regulation_reset_calibration();
    s_tempo_depart_ecoulee_ms = 0;  // nouvelle session — tempo complète
    if (s_cfg_prog.tempo_depart_on && s_cfg_prog.tempo_depart_s > 0) {
        s_tempo_depart_effectuee = false;
        entrer_etat(ETAT_TEMPO_DEPART);
    } else {
        s_tempo_depart_effectuee = true;  // aucune tempo configurée
        gpio_ev_poumon_set(true);
        s_nb_tentatives = 0;
        entrer_etat(ETAT_REMPLISSAGE_POUMON);
    }
}

static void handle_tempo_depart(const entrees_t *e, int64_t t_dans_etat)
{
    if (!e->pression_ok) {
        // Interruption : mémoriser le temps déjà écoulé — la reprise fera le restant
        s_tempo_depart_ecoulee_ms += t_dans_etat;
        gpio_ev_canon_set(false);
        entrer_etat(ETAT_PAUSE_PRESSION);
    } else if (s_tempo_depart_ecoulee_ms + t_dans_etat
               >= (int64_t)s_cfg_prog.tempo_depart_s * 1000) {
        s_tempo_depart_effectuee  = true;  // tempo terminée — pas rejouée après pause pression
        s_tempo_depart_ecoulee_ms = 0;
        gpio_ev_poumon_set(true);
        s_nb_tentatives = 0;
        entrer_etat(ETAT_REMPLISSAGE_POUMON);
    }
}

static void handle_remplissage_poumon(const entrees_t *e, int64_t t_dans_etat)
{
    (void)t_dans_etat;  // chrono basé sur s_t_rempl_debut_ms (exclut l'attente vidange en retry)
    if (s_nb_tentatives > 0 && !gpio_ev_poumon_get()) {
        if (now_ms() < s_t_retry_vidange_fin_ms) return;
        gpio_ev_poumon_set(true);
        s_t_rempl_debut_ms = now_ms();
    }
    // Timeout configurable, même logique que SOUS_REMPLISSAGE (cycles)
    int64_t t_rempl_ms = (s_cfg_machine.t_rempl_fixe_s > 0.0f)
                         ? (int64_t)(s_cfg_machine.t_rempl_fixe_s * 1000.0f)
                         : 20000;
    int64_t t_rempl = now_ms() - s_t_rempl_debut_ms;
    bool poumon_ok = e->poumon_plein ||
                     (s_cfg_machine.mode_deg_poumon && t_rempl >= t_rempl_ms);
    if (!e->pression_ok) {
        gpio_all_ev_off();
        entrer_etat(ETAT_PAUSE_PRESSION);
    } else if (poumon_ok) {
        gpio_ev_poumon_set(false);
        s_nb_tentatives = 0;  // remplissage initial réussi — pas de report dans les cycles
        if (e->fin_course) {
            if (s_cfg_prog.tempo_arrivee_on && s_cfg_prog.tempo_arrivee_s > 0) {
                gpio_ev_canon_set(true);
                entrer_etat(ETAT_TEMPO_ARRIVEE);
            } else {
                gpio_ev_canon_set(false);
                entrer_etat(ETAT_ARRET_FINAL);
            }
        } else {
            gpio_ev_canon_set(true);
            entrer_etat(ETAT_EN_COURS);
            entrer_sous_etat(SOUS_VIDANGE);
        }
    } else if (t_rempl >= t_rempl_ms) {
        gpio_ev_poumon_set(false);
        s_nb_tentatives++;
        if (s_nb_tentatives >= 2) {
            gpio_ev_canon_set(false);
            state_machine_declencher_urgence("Remplissage poumon echoue (2/2)");
        } else {
            s_t_retry_vidange_fin_ms = now_ms() + (int64_t)(s_cfg_machine.t_vidange_s * 1000.0f);
            entrer_etat(ETAT_REMPLISSAGE_POUMON);
        }
    }
}

static void handle_en_cours(const entrees_t *e, int64_t t_dans_etat)
{
    (void)t_dans_etat;
    if (!e->pression_ok) {
        gpio_all_ev_off();
        s_t_pause_debut_ms = now_ms();
        entrer_etat(ETAT_PAUSE_PRESSION);
        return;
    }
    if (e->fin_course && s_sous_etat != SOUS_REMPLISSAGE) {
        gpio_ev_poumon_set(false);
        if (s_cfg_prog.tempo_arrivee_on && s_cfg_prog.tempo_arrivee_s > 0) {
            entrer_etat(ETAT_TEMPO_ARRIVEE);
        } else {
            gpio_ev_canon_set(false);
            entrer_etat(ETAT_ARRET_FINAL);
        }
        return;
    }

    switch (s_sous_etat) {

    case SOUS_VIDANGE: {
        int64_t t_vidange_ms = (int64_t)(s_cfg_machine.t_vidange_s * 1000.0f);
        if (t_vidange_ms <= 0) t_vidange_ms = 1;
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
            // NE PAS remettre s_nb_tentatives à 0 ici : il compte les timeouts
            // consécutifs et n'est remis à 0 que sur remplissage réussi.
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
            fin_rempl = (now_ms() - s_t_rempl_debut_ms >= t_timeout_ms);
        } else {
            if (e->poumon_plein) {
                fin_rempl = true;
            } else if (now_ms() - s_t_rempl_debut_ms >= t_timeout_ms) {
                s_nb_tentatives++;
                if (s_nb_tentatives >= 2) {
                    gpio_all_ev_off();
                    state_machine_declencher_urgence("Timeout poumon cycle");
                    break;
                }
                gpio_ev_poumon_set(false);
                entrer_sous_etat(SOUS_VIDANGE);
                break;
            }
        }

        if (fin_rempl) {
            gpio_ev_poumon_set(false);
            s_nb_tentatives = 0;  // remplissage réussi — compteur de timeouts consécutifs
            s_t_remplissage_ms = (float)(now_ms() - s_t_rempl_debut_ms);

            float t_rempl_s = s_t_remplissage_ms / 1000.0f;
            if (t_rempl_s > 1.5f && t_rempl_s < s_t_rempl_min_s) {
                s_t_rempl_min_s = t_rempl_s;
                config_nvs_sauver_t_rempl_min(s_t_rempl_min_s);
            }

            if (!s_profil) {
                // Config machine invalide — déjà signalé en LOGE à l'init.
                // On boucle sur SOUS_VIDANGE plutôt que déclencher une urgence :
                // le canon continue d'avancer, l'opérateur peut stopper proprement via cmd_stop.
                ESP_LOGE(TAG, "fin_rempl : profil NULL - accumulation longueur impossible");
                entrer_sous_etat(SOUS_VIDANGE);
                break;
            }
            int etage = calcul_etage_courant(s_longueur_enroulee, s_profil);
            float r   = calcul_rayon_etage(etage, s_profil);
            float dist_cycle = calcul_dist_cycle_m(r, s_cfg_machine.cycles_par_tour);

            if (dist_cycle > 0.0f) {
                float dist_moy = regulation_update_dist_par_cycle(dist_cycle);
                s_cfg_machine.dist_cycle_nvs = dist_moy;
                s_longueur_enroulee  += dist_cycle;
                s_longueur_session_m += dist_cycle;
                if (s_longueur_enroulee - s_longueur_derniere_nvs >= 5.0f) {
                    if (config_nvs_sauver_longueur(s_longueur_enroulee) == ESP_OK) {
                        s_longueur_derniere_nvs = s_longueur_enroulee;
                    } else {
                        ESP_LOGW(TAG, "NVS write longueur echoue (%.1fm) — retry au prochain cycle",
                                 s_longueur_enroulee);
                    }
                }

                float debit_tmp = 0.0f;
                float v_cible_m_h = lookup_vitesse_cible(
                    s_abaque,
                    s_cfg_prog.pression_bar,
                    (float)s_cfg_prog.buse_mm,
                    s_cfg_prog.dose_mm,
                    s_cfg_prog.largeur_m,
                    &debit_tmp, NULL);
                s_vitesse_cible_m_h = v_cible_m_h;
                float v_cible_m_s = v_cible_m_h / 3600.0f;
                bool alerte = false;
                float t_att = calcul_t_attente_s(
                    dist_moy, v_cible_m_s,
                    s_t_remplissage_ms / 1000.0f,
                    s_cfg_machine.t_vidange_s,
                    &alerte);

                if (regulation_get_nb_cycles() >= s_cfg_machine.n_cycles_calib) {
                    float v_reel = gpio_get_vitesse_m_h();
                    t_att = correction_vitesse(t_att, v_reel, v_cible_m_h, s_cfg_machine.kp_regulation);
                }

                // t_att < 0 = pression insuffisante pour atteindre la vitesse cible :
                // pas d'attente, cycle au maximum. vitesse_max_m_h + dose_corrigee_mm
                // sont calculés ci-dessous et exposés dans le statut (alerte_pression_insuff).
                s_t_attente_ms = (int32_t)(t_att > 0.0f ? t_att * 1000.0f : 0.0f);
                s_status.alerte_pression_insuff = alerte;

                if (alerte) {
                    float t_min_s = s_t_remplissage_ms / 1000.0f + s_cfg_machine.t_vidange_s;
                    s_vitesse_max_m_h = (t_min_s > 0.0f)
                                        ? (dist_moy / t_min_s) * 3600.0f : 0.0f;
                    s_dose_corrigee_mm = (s_vitesse_max_m_h > 0.0f && s_cfg_prog.largeur_m > 0.0f)
                                         ? calcul_dose_inst_mm(debit_tmp, s_vitesse_max_m_h,
                                                               s_cfg_prog.largeur_m)
                                         : 0.0f;
                } else {
                    s_vitesse_max_m_h  = 0.0f;
                    s_dose_corrigee_mm = 0.0f;
                }
            }

            entrer_sous_etat(SOUS_VIDANGE);
        }
        break;
    }
    }  // switch sous_etat
}

static void handle_pause_pression(const entrees_t *e)
{
    s_duree_pause_ms += 100;
    if (e->pression_ok) {
        // Reprise via OUVERTURE_CANON : la stabilisation 30 ticks est rejouée
        // (pression possiblement instable au redémarrage de la pompe), mais pas
        // la tempo départ. EV_POUMON sera rouverte après stabilisation ;
        // REMPLISSAGE_POUMON vérifie fin_course dès son premier tick.
        gpio_ev_canon_set(true);
        s_reprise_pression = true;
        entrer_etat(ETAT_OUVERTURE_CANON);
    }
}

static void handle_tempo_arrivee(const entrees_t *e, int64_t t_dans_etat)
{
    if (!e->pression_ok) {
        gpio_ev_canon_set(false);
        entrer_etat(ETAT_ARRET_FINAL);
    } else if (t_dans_etat >= (int64_t)s_cfg_prog.tempo_arrivee_s * 1000) {
        gpio_ev_canon_set(false);
        entrer_etat(ETAT_ARRET_FINAL);
    }
}

static void handle_arret_final(const entrees_t *e)
{
    if (!s_bilan_envoye) {
        // Volume et dose moyenne calculés depuis le débit abaque × durée effective
        // (hors pauses pression) — PAS depuis dose_inst_mm, qui vaut 0 à l'arrêt
        // (plus d'impulsions → vitesse instantanée nulle).
        float duree_irrig_h = ((float)s_status.duree_s - (float)(s_duree_pause_ms / 1000))
                            / 3600.0f;
        if (duree_irrig_h < 0.0f) duree_irrig_h = 0.0f;
        float vol_m3  = s_status.debit_m3h * duree_irrig_h;
        float dose_mm = (s_status.surface_m2 > 0.0f)
                      ? vol_m3 / s_status.surface_m2 * 1000.0f : 0.0f;

        s_session.longueur_m              = s_longueur_enroulee;
        s_session.surface_m2              = s_status.surface_m2;
        s_session.dose_moy_mm             = dose_mm;
        s_session.volume_m3               = vol_m3;
        s_session.duree_s                 = s_status.duree_s;
        s_session.nb_cycles               = (uint32_t)regulation_get_nb_cycles();
        s_session.duree_pause_pression_s  = s_duree_pause_ms / 1000;
        {
            float surf_ha = s_status.surface_m2 / 10000.0f;
            float dur_h   = (float)s_status.duree_s / 3600.0f;
            float tot_surf = s_stats.total_surface_ha + surf_ha;
            if (tot_surf > 0.0f)
                s_stats.dose_moy_mm = (s_stats.dose_moy_mm * s_stats.total_surface_ha
                                     + dose_mm * surf_ha) / tot_surf;
            float tot_dur = s_stats.total_duree_h + dur_h;
            if (tot_dur > 0.0f)
                s_stats.vitesse_moy_m_h = (s_stats.vitesse_moy_m_h * s_stats.total_duree_h
                                          + s_status.vitesse_cible_m_h * dur_h) / tot_dur;
            s_stats.total_surface_ha += surf_ha;
            s_stats.total_volume_m3  += vol_m3;
            s_stats.total_duree_h    += dur_h;
            s_stats.nb_sessions++;
            config_nvs_sauver_stats(&s_stats);
        }
        telemetry_envoyer_bilan(&s_session);
        s_bilan_envoye = true;
        s_t_session_debut_ms = 0;  // fige s_status.duree_s sur la durée finale de session
    }
    if (!e->fin_course) {
        s_longueur_enroulee     = 0.0f;
        s_longueur_derniere_nvs = 0.0f;
        s_longueur_session_m    = 0.0f;
        s_longueur_deroule_m    = 0.0f;
        config_nvs_sauver_longueur(0.0f);
        config_nvs_sauver_deroule(0.0f);   // sinon le boot suivant croit à un reboot mid-session
        config_nvs_sauver_session_active(false);
        regulation_reset_calibration();
        config_nvs_sauver_machine(&s_cfg_machine);
        memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
        charger_config_interne();
        entrer_etat(ETAT_VEILLE);
    }
}

static void handle_deroule(const entrees_t *e)
{
    uint32_t impl = gpio_get_impulsions();
    if (impl > 0 && s_profil && s_profil->longueur_tuyau_m > 0.0f) {
        gpio_reset_impulsions_cycle();
        float pos_abs = s_profil->longueur_tuyau_m - s_mesure_deroule_m;
        int   etage = calcul_etage_courant(pos_abs, s_profil);
        float r     = calcul_rayon_etage(etage, s_profil);
        float d     = calcul_dist_pulse_m(r) * s_cfg_machine.facteur_correction * (float)impl;
        if (s_mesure_deroule_m + d <= s_profil->longueur_tuyau_m)
            s_mesure_deroule_m += d;
    } else if (impl > 0) {
        gpio_reset_impulsions_cycle();
    }
    s_status.cfg_valide = config_programme_est_valide(&s_cfg_prog);
    if (s_demarrage_autorise && e->pression_ok && !e->fin_course && s_status.cfg_valide) {
        s_demarrage_autorise = false;
        s_reprise_pression = false;  // démarrage initial — séquence complète
        float total = s_profil ? s_profil->longueur_tuyau_m : 0.0f;
        s_longueur_deroule_m = s_mesure_deroule_m;
        s_longueur_enroulee  = (total > 0.0f) ? total - s_mesure_deroule_m : 0.0f;
        s_longueur_session_m = 0.0f;
        s_longueur_derniere_nvs = s_longueur_enroulee;
        config_nvs_sauver_position(s_longueur_enroulee, s_longueur_deroule_m);
        gpio_ev_canon_set(true);
        entrer_etat(ETAT_OUVERTURE_CANON);
    }
}

// ---------------------------------------------------------------------------
// Tick principal — appelé toutes les 100ms depuis state_machine_task
// ---------------------------------------------------------------------------
void tick_state_machine(void)
{
    // Poll 30s normal, 5s si tension < seuil warn (alerte imminente)
    int batt_periode = s_batt_alerte_active ? 50 : 300;
    if (++s_batt_tick >= batt_periode) {
        s_batt_tick = 0;
        s_batt = batterie_get_status();
        s_batt_alerte_active = (s_batt.etat == BATT_ETAT_FAIBLE ||
                                 s_batt.etat == BATT_ETAT_CRITIQUE);
    }

    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);

    // SEC priorité absolue — avant tout traitement état
    securites_watchdog();

    // Batterie critique en session active → fermeture EVs + arrêt d'urgence
    if (s_batt.etat == BATT_ETAT_CRITIQUE && s_batt.voltage_v > 0.0f) {
        switch (s_etat) {
            case ETAT_OUVERTURE_CANON:
            case ETAT_TEMPO_DEPART:
            case ETAT_REMPLISSAGE_POUMON:
            case ETAT_EN_COURS:
            case ETAT_PAUSE_PRESSION:
            case ETAT_TEMPO_ARRIVEE: {
                char msg[48];
                snprintf(msg, sizeof(msg),
                         "Batterie critique (%.1fV)", s_batt.voltage_v);
                state_machine_declencher_urgence(msg);
                break;
            }
            default: break;
        }
    }

    entrees_t e;
    gpio_handler_lire_entrees(&e);
#ifdef CONFIG_IRRI_WOKWI_MODE
    e.pression_ok = s_wokwi_pression_ok;
#endif
#ifdef CONFIG_IRRI_ENABLE_TESTS
    if (s_sim_active) {
        e.pression_ok  = s_sim_pression;
        e.fin_course   = s_sim_fin_course;
        e.secu_spires  = s_sim_secu_spires;
    }
#endif
    int64_t t_dans_etat = now_ms() - s_t_etat_ms;

    // Réarmement physique : 3 appuis sur poumon_plein quand EV_POUMON=OFF
    {
        static int     s_rearm_count   = 0;
        static bool    s_rearm_prev    = false;
        static int64_t s_rearm_last_ms = 0;

        bool ev_poumon_on = gpio_ev_poumon_get();
        if (!ev_poumon_on) {
            bool p = e.poumon_plein;
            if (p && !s_rearm_prev) {  // front montant
                int64_t t = now_ms();
                if (s_rearm_count == 0) {
                    s_rearm_last_ms = t;  // démarre la fenêtre au 1er appui
                } else if (t - s_rearm_last_ms > 5000) {
                    s_rearm_count   = 0;  // séquence trop longue — repart à 1
                    s_rearm_last_ms = t;
                }
                s_rearm_count++;
                if (s_rearm_count >= 3) {
                    s_rearm_count = 0;
                    ESP_LOGW(TAG, "Rearmement physique (3x poumon) etat=%d", s_etat);
                    if (s_etat == ETAT_ARRET_URGENCE) {
                        if (strstr(s_status.raison_arret, "Debordement") && e.secu_spires) {
                            ESP_LOGW(TAG, "Rearm physique ignore - debordement toujours actif");
                        } else {
                            // Reprendre : longueurs et calibration PID préservées, comme cmd_resume
                            memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
                            config_nvs_sauver_urgence("");
                            s_demarrage_autorise = true;
                            config_nvs_sauver_machine(&s_cfg_machine);
                            entrer_etat(ETAT_VEILLE);
                        }
                    }
                }
            }
            s_rearm_prev = p;
        } else {
            s_rearm_count = 0;
            s_rearm_prev  = false;
        }
    }

    switch (s_etat) {
    case ETAT_VEILLE:             handle_veille(&e, t_dans_etat);             break;
    case ETAT_OUVERTURE_CANON:    handle_ouverture_canon(&e, t_dans_etat);    break;
    case ETAT_TEMPO_DEPART:       handle_tempo_depart(&e, t_dans_etat);       break;
    case ETAT_REMPLISSAGE_POUMON: handle_remplissage_poumon(&e, t_dans_etat); break;
    case ETAT_EN_COURS:           handle_en_cours(&e, t_dans_etat);           break;
    case ETAT_PAUSE_PRESSION:     handle_pause_pression(&e);                  break;
    case ETAT_TEMPO_ARRIVEE:      handle_tempo_arrivee(&e, t_dans_etat);      break;
    case ETAT_ARRET_FINAL:        handle_arret_final(&e);                     break;
    case ETAT_ARRET_URGENCE:                                                   break;
    case ETAT_DEROULE:            handle_deroule(&e);                         break;
    }  // switch etat

    // Vitesse depuis timing des cycles poumon (voie principale quand cycles_par_tour calibré)
    if (s_cfg_machine.cycles_par_tour > 0.0f && s_etat == ETAT_EN_COURS) {
        float t_cycle_s = (s_t_remplissage_ms + s_t_attente_ms) / 1000.0f
                        + s_cfg_machine.t_vidange_s;
        float v_reel = (t_cycle_s > 0.5f)
                     ? (s_cfg_machine.dist_cycle_nvs * 3600.0f / t_cycle_s)
                     : 0.0f;
        gpio_handler_set_vitesse_estimee(v_reel);
    }

    // Mise à jour statut diffusé
    s_status.etat         = s_etat;
    s_status.sous_etat    = s_sous_etat;
    s_status.ev_canon     = gpio_ev_canon_get();
    s_status.ev_poumon    = gpio_ev_poumon_get();
    s_status.pression_ok  = e.pression_ok;
    s_status.fin_course   = e.fin_course;
    s_status.secu_spires  = e.secu_spires;
    s_status.poumon_plein = e.poumon_plein;
    s_status.longueur_enroulee_m  = s_longueur_session_m;   // progression session 0→déroulée
    s_status.longueur_deroulee_m  = s_longueur_deroule_m;   // longueur déployée, constante session
    if (s_t_session_debut_ms > 0) {
        s_status.duree_s = (int32_t)((now_ms() - s_t_session_debut_ms) / 1000);
    }
    s_status.t_remplissage_ms = (int32_t)s_t_remplissage_ms;
    s_status.t_attente_ms     = s_t_attente_ms;
    s_status.cycles_total     = (uint32_t)regulation_get_nb_cycles();
    s_status.facteur_correction   = s_cfg_machine.facteur_correction;
    s_status.heure_synchro        = s_heure_synchro;
    s_status.mode_degrade_poumon  = s_cfg_machine.mode_deg_poumon;
    s_status.mode_degrade_spires  = s_cfg_machine.mode_deg_spires;
    s_status.mesure_deroule_m     = s_mesure_deroule_m;
    s_status.vitesse_cible_m_h    = s_vitesse_cible_m_h;
    s_status.vitesse_m_h          = gpio_get_vitesse_m_h();

    // Hydraulique, agronomie, mécanique
    {
        float debit_out = 0.0f, p_buse_out = 0.0f;
        if (s_abaque && s_cfg_prog.buse_mm > 0) {
            lookup_vitesse_cible(s_abaque, s_cfg_prog.pression_bar,
                                 (float)s_cfg_prog.buse_mm, s_cfg_prog.dose_mm,
                                 s_cfg_prog.largeur_m,
                                 &debit_out, &p_buse_out);
        }
        s_status.debit_m3h       = debit_out;
        s_status.p_buse_bar      = p_buse_out;
        s_status.p_enrouleur_bar = s_cfg_prog.pression_bar;
        s_status.surface_m2      = calcul_surface_m2(s_longueur_session_m, s_cfg_prog.largeur_m);
        s_status.dose_inst_mm    = (s_status.vitesse_m_h > 0.0f && s_cfg_prog.largeur_m > 0.0f)
            ? calcul_dose_inst_mm(debit_out, s_status.vitesse_m_h, s_cfg_prog.largeur_m)
            : 0.0f;
        s_status.etage_courant    = s_profil ? calcul_etage_courant(s_longueur_enroulee, s_profil) : 1;
        s_status.nb_etages        = s_profil ? s_profil->nb_etages : 0;
        s_status.dist_par_cycle_m = s_cfg_machine.dist_cycle_nvs;
        float t_cyc_s = (s_t_remplissage_ms + s_t_attente_ms) / 1000.0f + s_cfg_machine.t_vidange_s;
        s_status.cycles_par_min_cible = (s_vitesse_cible_m_h > 0.0f && s_cfg_machine.dist_cycle_nvs > 0.0f)
            ? (s_vitesse_cible_m_h / s_cfg_machine.dist_cycle_nvs) / 60.0f : 0.0f;
        s_status.cycles_par_min_reel  = (s_etat == ETAT_EN_COURS && t_cyc_s > 0.5f)
                                        ? 60.0f / t_cyc_s : 0.0f;
    }

    // Campagne
    s_status.camp_surface_ha      = s_stats.total_surface_ha;
    s_status.camp_volume_m3       = s_stats.total_volume_m3;
    s_status.camp_dose_moy_mm     = s_stats.dose_moy_mm;
    s_status.camp_vitesse_moy_m_h = s_stats.vitesse_moy_m_h;
    s_status.camp_nb_sessions     = s_stats.nb_sessions;
    s_status.camp_duree_h         = s_stats.total_duree_h;

    s_status.batterie_v           = s_batt.voltage_v;
    s_status.batterie_pct         = s_batt.pourcentage;
    s_status.batterie_etat        = (int)s_batt.etat;
    s_status.cfg_batt_warn_v      = s_batt_warn_v;
    s_status.cfg_batt_crit_v      = s_batt_crit_v;

    // Estimation heure arrivée (valide depuis DEROULE jusqu'à TEMPO_ARRIVEE)
    {
        float longueur_restante_m = 0.0f;
        if (s_etat == ETAT_DEROULE && s_mesure_deroule_m > 0.0f) {
            longueur_restante_m = s_mesure_deroule_m;
        } else if (s_longueur_deroule_m > s_longueur_session_m) {
            longueur_restante_m = s_longueur_deroule_m - s_longueur_session_m;
        }
        if (s_vitesse_cible_m_h > 0.0f && longueur_restante_m > 0.0f) {
            float enroulement_s = (longueur_restante_m / s_vitesse_cible_m_h) * 3600.0f;
            int32_t tempo_s = s_cfg_prog.tempo_arrivee_on ? (int32_t)s_cfg_prog.tempo_arrivee_s : 0;
            s_status.heure_arrivee_relative_s = (int32_t)enroulement_s + tempo_s;
            if (s_heure_synchro && s_heure_base_unix > 0) {
                int64_t now_s = s_heure_base_unix + (int64_t)(now_ms() / 1000);
                s_status.heure_arrivee_unix = now_s + (int64_t)s_status.heure_arrivee_relative_s;
            } else {
                s_status.heure_arrivee_unix = 0;
            }
        } else {
            s_status.heure_arrivee_relative_s = 0;
            s_status.heure_arrivee_unix       = 0;
        }
    }

    // Champs config machine (pour initialisation de l'UI)
    s_status.cfg_t_vidange_s      = s_cfg_machine.t_vidange_s;
    s_status.cfg_kp_regulation    = s_cfg_machine.kp_regulation;
    s_status.cfg_n_cycles_calib   = s_cfg_machine.n_cycles_calib;
    s_status.cfg_t_rempl_fixe_s   = s_cfg_machine.t_rempl_fixe_s;
    s_status.cfg_denivele_m       = s_cfg_machine.denivele_m;
    s_status.cfg_machine_active   = s_cfg_machine.machine_active;
    s_status.cfg_abaque_idx       = s_cfg_machine.abaque_idx;
    s_status.cfg_cycles_par_tour  = s_cfg_machine.cycles_par_tour;
    s_status.cfg_heartbeat_rc_on    = s_cfg_machine.heartbeat_rc_on;
    s_status.cfg_fin_course_seuil_m = s_cfg_machine.fin_course_seuil_m;
    s_status.cfg_t_ouv_canon_s      = s_cfg_machine.t_ouv_canon_s;
    s_status.cfg_reprise_auto_on    = s_cfg_machine.reprise_auto_on;
    s_status.coupure_detectee       = s_coupure_detectee;
    s_status.vitesse_max_m_h        = s_vitesse_max_m_h;
    s_status.dose_corrigee_mm       = s_dose_corrigee_mm;

    // Heartbeat PIN_HEARTBEAT (GPIO 23) pour circuit RC fail-safe (conditionnel)
    if (s_cfg_machine.heartbeat_rc_on) {
        if (now_ms() - s_heartbeat_ms >= 500) {
            s_heartbeat_level = !s_heartbeat_level;
            gpio_set_level(PIN_HEARTBEAT, s_heartbeat_level ? 1 : 0);
            s_heartbeat_ms = now_ms();
        }
    } else {
        if (s_heartbeat_level) {
            s_heartbeat_level = false;
            gpio_set_level(PIN_HEARTBEAT, 0);
        }
    }

    xSemaphoreGiveRecursive(s_mutex);
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------
void state_machine_get_status(machine_status_t *status)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGiveRecursive(s_mutex);
}

etat_machine_t state_machine_get_etat(void)
{
    return s_etat;
}

void state_machine_cmd_start(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE || s_etat == ETAT_DEROULE) {
        ESP_LOGI(TAG, "cmd_start recu - demarrage autorise (etat=%d)", s_etat);
        s_demarrage_autorise = true;
    } else {
        ESP_LOGI(TAG, "cmd_start ignore (etat=%d)", s_etat);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_stop(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_DEROULE) {
        ESP_LOGI(TAG, "cmd_stop depuis DEROULE - retour VEILLE");
        entrer_etat(ETAT_VEILLE);
    } else if (s_etat != ETAT_VEILLE &&
               s_etat != ETAT_ARRET_FINAL &&
               s_etat != ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "cmd_stop recu depuis etat %d", s_etat);
        s_demarrage_autorise = false;  // arrêt opérateur : ne pas redémarrer seul
        gpio_all_ev_off();
        entrer_etat(ETAT_ARRET_FINAL);
    } else {
        ESP_LOGI(TAG, "cmd_stop ignore (etat=%d)", s_etat);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_reset(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "cmd_reset - sortie urgence");
        s_demarrage_autorise = false;
        s_coupure_detectee = false;
        memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
        config_nvs_sauver_urgence("");
        config_nvs_sauver_session_active(false);
        s_longueur_enroulee     = 0.0f;
        s_longueur_derniere_nvs = 0.0f;
        s_longueur_session_m    = 0.0f;
        s_longueur_deroule_m    = 0.0f;
        config_nvs_sauver_longueur(0.0f);
        config_nvs_sauver_deroule(0.0f);
        regulation_reset_calibration();
        s_t_rempl_min_s = 5.0f;
        config_nvs_sauver_t_rempl_min(5.0f);
        config_nvs_sauver_machine(&s_cfg_machine);
        s_t_session_debut_ms = 0;  // fige s_status.duree_s (session interrompue par urgence)
        entrer_etat(ETAT_VEILLE);
    } else if (s_etat == ETAT_ARRET_FINAL) {
        ESP_LOGI(TAG, "cmd_reset - sortie arret final (manuel)");
        s_demarrage_autorise = true;
        s_coupure_detectee = false;
        s_longueur_enroulee     = 0.0f;
        s_longueur_derniere_nvs = 0.0f;
        s_longueur_session_m    = 0.0f;
        s_longueur_deroule_m    = 0.0f;
        config_nvs_sauver_longueur(0.0f);
        config_nvs_sauver_deroule(0.0f);
        config_nvs_sauver_session_active(false);
        regulation_reset_calibration();
        s_t_rempl_min_s = 5.0f;
        config_nvs_sauver_t_rempl_min(5.0f);
        config_nvs_sauver_machine(&s_cfg_machine);
        memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
        entrer_etat(ETAT_VEILLE);
    } else {
        ESP_LOGI(TAG, "cmd_reset ignore (etat=%d)", s_etat);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_set_longueur(float longueur_deroule_m)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    float total = (s_profil && s_profil->longueur_tuyau_m > 0.0f)
                  ? s_profil->longueur_tuyau_m : 0.0f;
    if (longueur_deroule_m < 0.0f) longueur_deroule_m = 0.0f;
    if (total > 0.0f && longueur_deroule_m > total) longueur_deroule_m = total;

    // Position absolue interne pour les calculs d'étage/rayon
    float abs_enroulee = (total > 0.0f) ? total - longueur_deroule_m : 0.0f;

    s_longueur_deroule_m    = longueur_deroule_m;  // affiché "Déroulé" — constant session
    if (s_etat == ETAT_DEROULE)
        s_mesure_deroule_m = longueur_deroule_m;  // sync pour que la transition DEROULE->CANON soit correcte
    s_longueur_session_m    = 0.0f;                // progression repart de 0
    s_longueur_enroulee     = abs_enroulee;        // interne, calculs mécaniques
    s_longueur_derniere_nvs = abs_enroulee;
    config_nvs_sauver_longueur(abs_enroulee);
    config_nvs_sauver_deroule(longueur_deroule_m);
    ESP_LOGI(TAG, "Longueur forcee : deroule=%.1fm session=0 (abs=%.1fm etat=%d)",
             longueur_deroule_m, abs_enroulee, s_etat);
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_etalonner(float longueur_reelle_m)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
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
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_set_time(int64_t timestamp_unix)
{
    // Mutex requis : s_heure_base_unix et s_heure_synchro sont lus dans tick_state_machine()
    // (mutex tenu). Pas de race — les deux chemins sont sérialisés par le même mutex récursif.
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    s_heure_base_unix = timestamp_unix;
    s_heure_synchro   = true;
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_ev_canon_set(bool actif)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE) gpio_ev_canon_set(actif);
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_ev_poumon_set(bool actif)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE) gpio_ev_poumon_set(actif);
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_start_deroule(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_VEILLE) {
        s_mesure_deroule_m = 0.0f;  // nouvelle séquence de déroulage — mesure repart de 0
        gpio_reset_impulsions_cycle();
        entrer_etat(ETAT_DEROULE);
        ESP_LOGI(TAG, "cmd_start_deroule : entree DEROULE manuel");
    } else {
        ESP_LOGI(TAG, "cmd_start_deroule ignore (etat=%d)", s_etat);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_declencher_urgence(const char *raison)
{
    // Mutex récursif : safe depuis tick (mutex déjà tenu) et depuis securites_watchdog (sans mutex).
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    gpio_all_ev_off();
    if (s_etat != ETAT_ARRET_URGENCE) {
        ESP_LOGE(TAG, "ARRET URGENCE : %s", raison);
        entrer_etat(ETAT_ARRET_URGENCE);
        strncpy(s_status.raison_arret, raison, sizeof(s_status.raison_arret) - 1);
        s_status.raison_arret[sizeof(s_status.raison_arret) - 1] = '\0';
        config_nvs_sauver_urgence(raison);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_recharger_config(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat != ETAT_VEILLE) {
        xSemaphoreGiveRecursive(s_mutex);
        return;
    }
    charger_config_interne();
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_get_session_summary(session_summary_t *out)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    *out = s_session;
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_resume(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    if (s_etat == ETAT_ARRET_URGENCE && strstr(s_status.raison_arret, "Debordement")) {
        entrees_t eg;
        gpio_handler_lire_entrees(&eg);
        if (eg.secu_spires) {
            ESP_LOGW(TAG, "cmd_resume ignore - debordement toujours actif");
            xSemaphoreGiveRecursive(s_mutex);
            return;
        }
    }
    if (s_etat == ETAT_ARRET_URGENCE || s_etat == ETAT_ARRET_FINAL) {
        ESP_LOGI(TAG, "cmd_resume - reprise session (enroule=%.1fm deroule=%.1fm)",
                 s_longueur_session_m, s_longueur_deroule_m);
        s_demarrage_autorise = false;
        s_coupure_detectee = false;
        memset(s_status.raison_arret, 0, sizeof(s_status.raison_arret));
        config_nvs_sauver_urgence("");
        // longueurs préservées intentionnellement
        regulation_reset_calibration();
        config_nvs_sauver_machine(&s_cfg_machine);
        entrer_etat(ETAT_VEILLE);
    } else {
        ESP_LOGI(TAG, "cmd_resume ignore (etat=%d)", s_etat);
    }
    xSemaphoreGiveRecursive(s_mutex);
}

void state_machine_cmd_reset_campagne(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    config_nvs_reset_stats();
    memset(&s_stats, 0, sizeof(s_stats));
    xSemaphoreGiveRecursive(s_mutex);
    ESP_LOGI(TAG, "Stats campagne remises a zero");
}

bool state_machine_fin_course_est_normale(void)
{
    // Si la longueur déployée est inconnue (0), on retourne true intentionnellement :
    // on ne peut pas distinguer fin normale d'anomalie, et retourner false déclencherait
    // SEC-1 (urgence) même lors d'une arrivée réelle, bloquant la temporisation arrivée.
    // Dans les états où la bobine ne peut pas avancer (OUVERTURE_CANON, TEMPO_DEPART,
    // PAUSE_PRESSION), securites.c ne consulte pas cette fonction — SEC-1 y est toujours active.
    if (s_longueur_deroule_m <= 0.0f) return true;
    float restant = s_longueur_deroule_m - s_longueur_session_m;
    if (restant < 0.0f) restant = 0.0f;
    return restant <= s_cfg_machine.fin_course_seuil_m;
}

bool state_machine_longueur_sec_depassee(void)
{
    if (s_longueur_deroule_m <= 0.0f) return false;  // longueur inconnue — pas de securite
    float delta = s_longueur_session_m - s_longueur_deroule_m;
    return delta > s_cfg_machine.fin_course_seuil_m;
}

float state_machine_calc_vitesse(float pression_bar, float buse_mm, float dose_mm,
                                  float largeur_m,
                                  float *debit_out, float *p_buse_out)
{
    if (debit_out)  *debit_out  = 0.0f;
    if (p_buse_out) *p_buse_out = 0.0f;
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    float v = (s_abaque && buse_mm > 0.0f && dose_mm > 0.0f)
        ? lookup_vitesse_cible(s_abaque, pression_bar, buse_mm, dose_mm, largeur_m, debit_out, p_buse_out)
        : 0.0f;
    xSemaphoreGiveRecursive(s_mutex);
    return v;
}

programme_preview_t state_machine_programme_preview(float pression_bar, float buse_mm,
                                                     float dose_mm, float largeur_m)
{
    programme_preview_t pr;
    memset(&pr, 0, sizeof(pr));

    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    const canon_abaque_t *ab = s_abaque;

    if (!ab || buse_mm <= 0.0f || dose_mm <= 0.0f) {
        xSemaphoreGiveRecursive(s_mutex);
        return pr;
    }

    // Bornes abaque
    float p_min = ab->table[0].p_enrouleur, p_max = p_min;
    float b_min = ab->table[0].buse_mm,     b_max = b_min;
    for (int i = 1; i < ab->nb_entrees; i++) {
        float p = ab->table[i].p_enrouleur;
        float b = ab->table[i].buse_mm;
        if (p < p_min) { p_min = p; }
        if (p > p_max) { p_max = p; }
        if (b < b_min) { b_min = b; }
        if (b > b_max) { b_max = b; }
    }
    pr.p_min    = p_min * 0.75f;  pr.p_max    = p_max * 1.25f;
    pr.buse_min = b_min * 0.75f;  pr.buse_max = b_max * 1.25f;
    pr.dose_min = DOSE_MIN_MM * 0.75f;  pr.dose_max = DOSE_MAX_MM * 1.25f;

    // Vitesse et debit
    float debit_m3h = 0.0f, p_buse = 0.0f;
    pr.vitesse_m_h = lookup_vitesse_cible(ab, pression_bar, buse_mm, dose_mm,
                                           largeur_m, &debit_m3h, &p_buse);
    pr.debit_ls    = debit_m3h / 3.6f;
    pr.p_buse_bar  = p_buse;

    // Portee et espacement
    pr.esp_nominal_m = calcul_esp_nominal_m(ab, pression_bar, buse_mm);
    pr.portee_m      = (ab->esp_factor > 0.0f) ? pr.esp_nominal_m / ab->esp_factor : 0.0f;
    pr.esp_pos_min   = pr.esp_nominal_m * 0.75f;
    pr.esp_pos_max   = pr.esp_nominal_m * 1.10f;

    // Warnings hydrauliques
    hydro_warnings_t hw = valider_params_programme(ab, pression_bar, buse_mm, dose_mm, largeur_m);
    pr.w_pression_basse       = hw.pression_basse;
    pr.w_pression_haute       = hw.pression_haute;
    pr.w_buse_hors_plage      = hw.buse_hors_plage;
    pr.w_dose_hors_plage      = hw.dose_hors_plage;
    pr.w_esp_pos_chevauchement= hw.esp_pos_chevauchement;
    pr.w_esp_pos_insuf        = hw.esp_pos_insuf;

    // Warning vitesse limite
    float dist  = s_cfg_machine.dist_cycle_nvs;
    float t_min = s_t_rempl_min_s + s_cfg_machine.t_vidange_s;
    pr.v_max_m_h     = (dist > 0.0f && t_min > 0.0f) ? (dist / t_min) * 3600.0f : 0.0f;
    pr.w_vitesse_limite = (pr.v_max_m_h > 0.0f && pr.vitesse_m_h > pr.v_max_m_h);

    xSemaphoreGiveRecursive(s_mutex);
    return pr;
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
    s_etat               = ETAT_VEILLE;
    s_sous_etat          = SOUS_VIDANGE;
    s_t_sous_etat_ms     = 0;
    s_longueur_enroulee  = 0.0f;
    s_longueur_session_m = 0.0f;
    s_longueur_deroule_m = 0.0f;
    s_nb_tentatives      = 0;
    s_bilan_envoye       = false;
    s_demarrage_autorise = true;
    s_batt_tick          = 299;   // 1er tick du prochain test → lecture immédiate
    s_batt_alerte_active = false;
    s_batt               = (batt_status_t){0};
    gpio_ev_canon_set(false);
    gpio_ev_poumon_set(false);
}

void state_machine_test_set_longueurs(float deroule_m, float session_m)
{
    s_longueur_deroule_m = deroule_m;
    s_longueur_session_m = session_m;
    float total = (s_profil && s_profil->longueur_tuyau_m > 0.0f)
                  ? s_profil->longueur_tuyau_m : 0.0f;
    s_longueur_enroulee  = (total > 0.0f) ? total - deroule_m : 0.0f;
    s_longueur_derniere_nvs = s_longueur_enroulee;
}
#endif // CONFIG_IRRI_ENABLE_TESTS

#ifdef CONFIG_IRRI_TEST_MODE
void state_machine_sim_force_deroule(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    gpio_ev_canon_set(false);
    gpio_ev_poumon_set(false);
    s_mesure_deroule_m   = 0.0f;
    s_demarrage_autorise = false;   // evite l'auto-demarrage depuis DEROULE
    gpio_reset_impulsions_cycle();
    entrer_etat(ETAT_DEROULE);
    ESP_LOGI(TAG, "sim_force_deroule : entree DEROULE depuis etat=%d", s_etat);
    xSemaphoreGiveRecursive(s_mutex);
}
#endif // CONFIG_IRRI_TEST_MODE

// ---------------------------------------------------------------------------
// Vitesse max theorique — basee sur t_rempl_min_s + t_vidange + dist_cycle
// ---------------------------------------------------------------------------
float state_machine_get_vitesse_max(void)
{
    xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
    float dist  = s_cfg_machine.dist_cycle_nvs;
    float t_min = s_t_rempl_min_s + s_cfg_machine.t_vidange_s;
    float v_max = (dist > 0.0f && t_min > 0.0f) ? (dist / t_min) * 3600.0f : 0.0f;
    xSemaphoreGiveRecursive(s_mutex);
    return v_max;
}
