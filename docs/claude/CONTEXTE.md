---
type: claude-context
updated: 2026-05-31
---

# Contexte rapide — Irrifrance ESP32

## Résumé (10 lignes)

Remplacement de la régulation électronique d'un enrouleur d'irrigation Irrifrance Structure 1 Bis
(tuyau PE Ø82mm, 330m) dont l'Irridoseur 3 d'origine est en panne irréparable. L'ESP32 recrée la
logique de régulation hydraulique par poumon TPI : ouverture canon → remplissage poumon → cliquet
bobine → avance tuyau → vitesse d'enroulement contrôlée. Le firmware tourne sur un ESP32 Quad MOS
Switch Module (4 canaux MOSFET 12V intégrés). L'interface opérateur est un WebSocket/HTML embarqué
accessible en WiFi AP depuis un smartphone. La régulation est feedforward + correction Kp sur la
vitesse mesurée par pastilles. La machine d'états compte 10 états. La persistance (urgences,
longueurs, config) est en NVS flash. Un simulateur WebSocket intégré permet de tester sans matériel.

## Stack technique

- **MCU** : ESP32-D0WD-V3 (ESP32-32E)
- **Framework** : ESP-IDF v5.5.2 (FreeRTOS, LwIP, NVS, ADC, GPIO ISR, OTA)
- **Langage** : C (C11)
- **Build** : CMake + Ninja via idf.py
- **Tests unitaires PC** : Unity (test/host/)
- **Simulateur embarqué** : CONFIG_IRRI_TEST_MODE (WebSocket injection GPIO)
- **Web UI** : HTML/CSS/JS inline (~38 KB), embarqué via EMBED_FILES CMake
- **OTA** : POST /ota/update (httpd_handle_t)
- **WiFi** : AP "IRRIDOSEUR-XXXX" (4 derniers octets MAC), pas de STA

## Carte : ESP32 Quad MOS Switch Module

- 4 canaux MOSFET DC 5-60V intégrés (OUT1..OUT4)
- Buck 12V intégré, alimentation USB-C ou 12V direct
- **⚠️ PIN_EV_CANON=25, PIN_EV_POUMON=26 sont provisoires** — à identifier sur schéma Quad MOS
  (`#warning` présent dans gpio_config.h)

## État d'avancement par PR

| PR | Description | État |
|---|---|---|
| PR-01 | GPIO init, ISR vitesse, électrovannes | ✅ |
| PR-02 | Machine d'états 8 états + sécurités | ✅ |
| PR-03 | Calculs mécaniques (rayon, étage, étalonnage) | ✅ |
| PR-04 | Calculs hydrauliques (abaque SR150C, interpolation) | ✅ |
| PR-05 | Sécurités SEC-1/SEC-2 watchdog | ✅ |
| PR-06 | Régulation feedforward + correction Kp | ✅ |
| PR-07 | Config NVS (4 namespaces, programmes × 5) | ✅ |
| PR-08 | WiFi AP + WebSocket + OTA | ✅ |
| PR-09 | Web UI embarquée 3 onglets | ✅ |
| PR-10 | Intégration complète + stats campagne + ETAT_DEROULE | ✅ |
| PR-11 | Tests unitaires PC (Unity host) + simulateur WS | ✅ |
| PR-12 | Mesure batterie ADC1 GPIO 36 (R1=100k/R2=27k) | ✅ |
| PR-13 | Reprendre après sécu débordement + 3-tap reprendre | ✅ |
| PR-14 | Robustesse : TWDT + détection coupure + heartbeat RC | ✅ |
| PR-15 | Fix fin de course : arrêt normal → ARRET_FINAL (seuil configurable) | ✅ |
| PR-16 | Alerte dose trop basse : vitesse max + dose corrigée affichées | ✅ |
| PR-17 | SEC-L : sécurité longueur enroulée > déployée | ✅ |
| PR-18 | Calculs hydrauliques analytiques (Torricelli) + validation programme live | ✅ |

**Build actuel** : 0xe3680 bytes (~933 KB), 53% flash libre.

## Décisions techniques et raisons

| Décision | Raison |
|---|---|
| Contacts NC partout | Fil coupé = HIGH = sécurité active — fail-safe câblage |
| `gpio_all_ev_off()` en premier dans toute urgence | Coupure matérielle avant tout log ou état |
| `securites_watchdog()` appelé EN PREMIER dans le tick | SEC-2 priorité absolue, avant tout traitement |
| Mutex sur machine d'états | WebSocket et tick tournent dans des tâches séparées |
| JSON inline dans webserver.c (pas cJSON) | Évite malloc/free en tâche réseau → moins de fragmentation heap |
| EMBED_FILES pour index.html | Zéro SPIFFS, zéro partition data, OTA plus simple |
| `s_demarrage_autorise` flag | Empêche redémarrage auto après urgence ou stop — opérateur doit confirmer |
| `regulation_reset_calibration()` à chaque reprise | Évite d'utiliser une dist_par_cycle_m d'une session précédente |
| Bypass SEC-2 (`mode_deg_spires`) | Capteur débordement peut être défaillant — bypass plutôt qu'arrêt définitif |
| 3-tap poumon = REPRENDRE (longueurs préservées) | Réarmement physique terrain sans smartphone (session continue) |
| 3-tap bloqué si debordement actif | SEC-2 re-déclencherait au tick suivant — inutile et trompeur |

## Pièges connus et erreurs à ne pas reproduire

1. **PIN_EV_CANON/POUMON** : valeurs provisoires GPIO 25/26 — `#warning` à résoudre avant prod.
2. **t_vidange_s = 0.0** : non mesurée terrain — T_attente sera sous-estimé si > 0 réel.
3. **cycles_par_tour** : mesuré physiquement = 40 sur cet enrouleur. Si 0 → mode dégradé A inactif.
4. **spires_par_etage** : abaque prédit 289m tuyau mais physique = 330m → possible 5ème étage à confirmer terrain.
5. **SEC-2 dans ARRET_URGENCE** : le watchdog re-déclenche `declencher_urgence()` à chaque tick tant que `secu_spires` est HIGH → c'est voulu, c'est idempotent.
6. **cmd_resume bloqué si debordement actif** : double protection (UI masque bouton + firmware rejette).
7. **`regulation_reset_calibration()`** : doit être appelé partout où on revient en VEILLE — sinon dist_par_cycle stale.
8. **JSON_BUF_SIZE = 3072** : si on ajoute des champs dans machine_status_t, vérifier que le JSON ne déborde pas.
9. **Mutex récursif** : `tick_state_machine()` prend le mutex puis appelle `securites_watchdog()` qui appelle `state_machine_declencher_urgence()` qui prend aussi le mutex → mutex DOIT être récursif (vérifié dans main.c).
10. **Tests PC** : les tests host dans `test/host/` utilisent des stubs — ne pas inclure de code ESP-IDF direct dans les modules testés.

## Conventions de nommage

| Catégorie | Convention | Exemple |
|---|---|---|
| Variables statiques module | `s_` préfixe | `s_etat`, `s_longueur_enroulee` |
| Variables statiques dans fonction | `s_` préfixe (static local) | `s_rearm_count` |
| Constantes GPIO | `PIN_` préfixe MAJUSCULE | `PIN_FIN_COURSE` |
| Paramètres NVS | clé courte sans accent | `"t_vidange"`, `"kp"` |
| Fonctions publiques | `module_verb_objet()` | `gpio_handler_lire_entrees()` |
| Tags ESP_LOG | constante `TAG` dans chaque .c | `static const char *TAG = "securites"` |
| États machine | `ETAT_` préfixe MAJUSCULE | `ETAT_EN_COURS` |
| Sous-états | `SOUS_` préfixe MAJUSCULE | `SOUS_REMPLISSAGE` |
| Strings C | ASCII pur (pas d'accents dans le code .c) | `"Debordement bobine"` |
| Commentaires | Français | — |

## Variables globales et constantes importantes

| Nom | Fichier | Type | Rôle |
|---|---|---|---|
| `s_etat` | state_machine.c | `etat_machine_t` | État courant machine |
| `s_longueur_enroulee` | state_machine.c | `float` | Longueur enroulée session (m) |
| `s_longueur_deroule_m` | state_machine.c | `float` | Longueur déployée terrain (m) |
| `s_longueur_session_m` | state_machine.c | `float` | Longueur enroulée totale session (m) |
| `s_cfg_machine` | state_machine.c | `config_machine_t` | Config machine active (NVS) |
| `s_cfg_prog` | state_machine.c | `config_programme_t` | Programme actif (NVS) |
| `s_profil` | state_machine.c | `machine_profile_t` | Profil machine actif |
| `s_abaque` | state_machine.c | `canon_abaque_t*` | Abaque canon actif |
| `s_status` | state_machine.c | `machine_status_t` | Statut diffusé WebSocket |
| `s_demarrage_autorise` | state_machine.c | `bool` | Opérateur a confirmé démarrage |
| `s_bypass_spires` | securites.c | `bool` | Bypass SEC-2 (mode dégradé) |
| `s_dist_moy` | regulation.c | `float` | Moy glissante dist/cycle (5 valeurs) |
| `s_mutex` | state_machine.c | `SemaphoreHandle_t` | Mutex récursif état machine |
| `NB_PASTILLES` | gpio_config.h | `#define 10` | Pastilles sur couronne bobine |
| `DEBOUNCE_VITESSE_MS` | gpio_config.h | `#define 50` | Anti-rebond ISR vitesse |
| `CALIB_WINDOW` | regulation.c | `#define 5` | Fenêtre moyenne dist/cycle |
