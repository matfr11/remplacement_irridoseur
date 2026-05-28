#include "state_machine.h"
#include "gpio_handler.h"
#include "calculs_hydraulique.h"
#include "calculs_mecanique.h"
#include "config_nvs.h"
#include "telemetry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "state_machine";

static etat_machine_t    s_etat = ETAT_VEILLE;
static machine_status_t  s_status;
static SemaphoreHandle_t s_mutex;
static uint64_t          s_entree_etat_us = 0;

static config_machine_t   s_cfg_machine;
static config_programme_t s_cfg_prog;

// --- Utilitaires internes ---

static void entrer_etat(etat_machine_t nouvel_etat)
{
    s_etat             = nouvel_etat;
    s_entree_etat_us   = esp_timer_get_time();
    s_status.etat      = nouvel_etat;
    s_status.etat_code = (int)nouvel_etat;
    ESP_LOGI(TAG, "→ État : %d", (int)nouvel_etat);
}

// Mise à jour du champ gpio_raw depuis les données capteur et les entrées lues
static void maj_gpio_raw(const entrees_t *entrees)
{
    s_status.gpio_raw.fin_course         = entrees->fin_course;
    s_status.gpio_raw.securite_spires    = entrees->securite_spires;
    s_status.gpio_raw.contact_poumon     = entrees->contact_poumon;
    s_status.gpio_raw.pressostat         = entrees->pressostat;
    s_status.gpio_raw.vitesse_impulsions = g_capteur_vitesse.nb_impulsions;
    s_status.gpio_raw.vitesse_m_h        = s_status.vitesse_m_h;
    s_status.gpio_raw.ev1                = s_status.ev1;
    s_status.gpio_raw.ev2                = s_status.ev2;
}

// Calcul vitesse instantanée depuis le capteur 10 pastilles
static void maj_vitesse(void)
{
    uint64_t now_us     = esp_timer_get_time();
    uint32_t periode_us = g_capteur_vitesse.periode_us;
    bool vitesse_valide = (now_us - g_capteur_vitesse.last_isr_us)
                          < ((uint64_t)TIMEOUT_VITESSE_MS * 1000ULL);

    if (vitesse_valide && periode_us > 0) {
        int   etage   = (s_status.etage_courant > 0) ? s_status.etage_courant : 1;
        float r_etage = calcul_rayon_etage(etage,
                            s_cfg_machine.r_tambour_vide_m,
                            s_cfg_machine.d_tuyau_ext_m);
        float dist_m      = calcul_dist_pulse_m(r_etage);
        float periode_min = (float)periode_us / (1000000.0f * 60.0f);
        s_status.vitesse_m_h = (periode_min > 0.0f)
                               ? (dist_m / periode_min) * 60.0f : 0.0f;
    } else {
        s_status.vitesse_m_h = 0.0f;
    }
}

// --- API publique ---

void state_machine_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_status, 0, sizeof(s_status));
    s_status.heure_arrivee_unix = -1;

    // Chargement config pour calculs de vitesse dès le démarrage
    config_nvs_lire_machine(&s_cfg_machine);
    s_status.nb_etages = s_cfg_machine.nb_etages;

    gpio_all_ev_off();
    s_status.ev1 = false;
    s_status.ev2 = false;

    entrer_etat(ETAT_VEILLE);
    ESP_LOGI(TAG, "Machine d'états initialisée — VEILLE, EV1=OFF EV2=OFF");
}

void tick_state_machine(void)
{
    entrees_t entrees;
    gpio_handler_lire_entrees(&entrees);

    maj_vitesse();
    s_status.pression_ok = entrees.pressostat;

    // Sécurité spires — priorité absolue, active dans TOUS les états y compris IRRITESTEUR
    if (entrees.securite_spires && s_etat != ETAT_ARRET_URGENCE) {
        gpio_all_ev_off();
        s_status.ev1 = false;
        s_status.ev2 = false;
        strncpy(s_status.raison_arret,
                "Sécurité spires — débordement bobine", 63);
        entrer_etat(ETAT_ARRET_URGENCE);
        telemetry_send_alarm(ALARM_SECURITE_SPIRES, s_status.raison_arret);
        ESP_LOGE(TAG, "ARRÊT URGENCE : %s", s_status.raison_arret);
        maj_gpio_raw(&entrees);
        return;
    }

    // En mode IRRITESTEUR, la machine est suspendue (sécurité spires gérée ci-dessus)
    if (s_etat == ETAT_IRRITESTEUR) {
        maj_gpio_raw(&entrees);
        return;
    }

    maj_gpio_raw(&entrees);

    // TODO PR-05 : implémenter toutes les transitions d'état
    // Squelette — la logique complète est dans PR-05
    (void)entrees;
}

void state_machine_get_status(machine_status_t *status)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *status = s_status;
    xSemaphoreGive(s_mutex);
}

void state_machine_cmd_start(void)
{
    if (s_etat == ETAT_VEILLE) {
        int idx = 0;
        config_nvs_lire_prog_actif(&idx);
        config_nvs_lire_programme(idx, &s_cfg_prog);
        config_nvs_lire_machine(&s_cfg_machine);

        s_status.nb_etages  = s_cfg_machine.nb_etages;
        s_status.p_mano_bar = s_cfg_prog.p_borne_bar;
        s_status.cfg_valide = config_machine_est_valide(&s_cfg_machine);

        strncpy(s_status.prog_nom, s_cfg_prog.nom, 20);
        s_status.prog_nom[20] = '\0';

        ESP_LOGI(TAG, "Commande DÉMARRER — prog=%s cfg_valide=%d",
                 s_status.prog_nom, s_status.cfg_valide);
        // TODO PR-05 : transition vers TEMPO_DEPART ou REMPLISSAGE_POUMON
    }
}

void state_machine_cmd_stop(void)
{
    gpio_all_ev_off();
    s_status.ev1 = false;
    s_status.ev2 = false;
    entrer_etat(ETAT_ARRET_FINAL);
    ESP_LOGI(TAG, "Commande ARRÊT — EV1=OFF EV2=OFF");
}

void state_machine_cmd_reset(void)
{
    if (s_etat == ETAT_ARRET_FINAL || s_etat == ETAT_ARRET_URGENCE) {
        memset(&s_status, 0, sizeof(s_status));
        s_status.heure_arrivee_unix = -1;
        gpio_all_ev_off();
        entrer_etat(ETAT_VEILLE);
        ESP_LOGI(TAG, "RESET — retour VEILLE");
    }
}

void state_machine_set_time(int64_t timestamp_unix)
{
    struct timeval tv = { .tv_sec = (time_t)timestamp_unix, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_status.heure_synchro = true;
    ESP_LOGI(TAG, "Heure synchronisée : %lld", (long long)timestamp_unix);
}

// --- Commandes IRRITESTEUR ---

void state_machine_cmd_entrer_irritesteur(void)
{
    if (s_etat != ETAT_VEILLE) {
        ESP_LOGW(TAG, "IRRITESTEUR refusé — état courant non VEILLE (%d)", (int)s_etat);
        return;
    }
    gpio_all_ev_off();
    s_status.ev1 = false;
    s_status.ev2 = false;
    entrer_etat(ETAT_IRRITESTEUR);
    ESP_LOGI(TAG, "=== IRRITESTEUR activé — machine d'états suspendue ===");
}

void state_machine_cmd_quitter_irritesteur(void)
{
    if (s_etat != ETAT_IRRITESTEUR) {
        return;
    }
    // Fail-safe : couper les électrovannes avant de rendre la main
    gpio_all_ev_off();
    s_status.ev1 = false;
    s_status.ev2 = false;
    entrer_etat(ETAT_VEILLE);
    ESP_LOGI(TAG, "=== IRRITESTEUR désactivé — retour VEILLE, EV1=OFF EV2=OFF ===");
}

void state_machine_cmd_ev1_set(bool actif)
{
    if (s_etat != ETAT_IRRITESTEUR) {
        ESP_LOGW(TAG, "cmd_ev1_set ignorée — pas en mode IRRITESTEUR");
        return;
    }
    // Sécurité : ouverture EV interdite si pression détectée au pressostat
    if (actif && s_status.pression_ok) {
        ESP_LOGW(TAG, "IRRITESTEUR EV1 ON refusé — pression détectée au pressostat");
        return;
    }
    gpio_ev1_set(actif);
    s_status.ev1 = actif;
    s_status.gpio_raw.ev1 = actif;
    ESP_LOGI(TAG, "IRRITESTEUR EV1 = %s", actif ? "ON" : "OFF");
}

void state_machine_cmd_ev2_set(bool actif)
{
    if (s_etat != ETAT_IRRITESTEUR) {
        ESP_LOGW(TAG, "cmd_ev2_set ignorée — pas en mode IRRITESTEUR");
        return;
    }
    // Sécurité : ouverture EV interdite si pression détectée au pressostat
    if (actif && s_status.pression_ok) {
        ESP_LOGW(TAG, "IRRITESTEUR EV2 ON refusé — pression détectée au pressostat");
        return;
    }
    gpio_ev2_set(actif);
    s_status.ev2 = actif;
    s_status.gpio_raw.ev2 = actif;
    ESP_LOGI(TAG, "IRRITESTEUR EV2 = %s", actif ? "ON" : "OFF");
}

#ifdef CONFIG_IRRI_ENABLE_TESTS
void state_machine_test_set_pression(bool pression_ok)
{
    s_status.pression_ok = pression_ok;
}
#endif
