# Changelog — Irrifrance ESP32

Toutes les modifications notables sont documentées ici.
Format : [PR-XX] — date — description

---

## [MàJ post-PR-01] — 2026-05-28 — Intégration SPECS_MISEAJOUR.md

### Modifié
- `config_nvs.h/c` : `config_machine_t` mise à jour
  - Supprimé : `alpha_poumon_deg`, `duree_rempl_s` (auto-calibrés / mesurés en temps réel)
  - Ajouté : `t_vidange_s` (défaut 5.0s), `kp_regulation` (0.1), `n_cycles_calib` (3)
  - Ajouté : `contact_no_*` × 4 — sens NO/NC configurable par contacteur
  - Mis à jour : défauts géométriques depuis fiche technique (`r_tambour_vide=0.648m`, `d_tuyau_ext=0.094m`)
  - `p_mano_bar` renommé `p_borne_bar` dans `config_programme_t` (terminologie table constructeur)
  - `config_machine_est_valide()` : ne vérifie plus que `t_vidange_s > 0`
  - Ajouté : `config_machine_t_vidange_par_defaut()` pour alerte UI
- `gpio_handler.h/c` : ajout `gpio_sens_t` et `gpio_handler_set_sens()`
  - La lecture des 4 contacts respecte maintenant le type NO/NC configuré
- `main.c` : chargement de `config_machine_t` avant `gpio_handler_init()`, puis appel `gpio_handler_set_sens()`

---

## [PR-01] — 2026-05-27 — Structure projet

### Ajouté
- `CMakeLists.txt` racine ESP-IDF v5.x
- `sdkconfig.defaults` : FreeRTOS 1kHz, WiFi AP, NVS, mDNS
- `main/CMakeLists.txt` : enregistrement composant avec toutes les sources
- `main/Kconfig.projbuild` : option `CONFIG_IRRI_ENABLE_TESTS`
- `main/main.c` : `app_main` avec init NVS, GPIO, machine d'états, tâches FreeRTOS
- `main/gpio_handler.h/.c` : config GPIO, ISR capteur vitesse, lecture entrées, fail-safe EV
- `main/state_machine.h/.c` : enum états, structure statut, tick squelette, fail-safe spires
- `main/calculs_hydraulique.h/.c` : table abaque UASA46, calcul P_canon, débit, vitesse cible
- `main/calculs_mecanique.h/.c` : rayon étage, étage courant, dist/pulse, dist/poumon, freq poumon
- `main/config_nvs.h/.c` : structures config_machine_t et config_programme_t, stubs NVS
- `main/webserver.h/.c` : stub HTTP/WebSocket (implémenté PR-07)
- `main/telemetry.h/.c` : couche abstraction LoRa futur, implémentations vides
- `main/webui.h` : placeholder HTML embarqué (implémenté PR-08)
- `main/test/test_calculs_hydraulique.c` : tests table abaque, P_canon, vitesse cible
- `main/test/test_calculs_mecanique.c` : tests rayons étage, dist/pulse, fréquence poumon
- `main/test/test_state_machine.c` : squelette tests (complété PR-05)
- `.github/PULL_REQUEST_TEMPLATE.md` : template PR projet
