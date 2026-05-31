# ROBUSTESSE — Irrifrance ESP32
## Gestion des pannes, coupures et plantages

> Ce document complète SPECS_FINAL_v3.md.
> Il couvre la robustesse du système face aux défaillances
> matérielles et logicielles.

---

## 1. Analyse des scénarios de défaillance

### 1.1 Coupure d'alimentation pendant un cycle

```
Cause : batterie déchargée, fil arraché, orage
Durée typique d'un cycle : 12 à 20 heures

Séquence physique :
  1. ESP32 s'éteint instantanément
  2. MOSFETs sans alimentation → gate à 0V
  3. EV_CANON et EV_POUMON se ferment (si NC) ✅
  4. Poumon hydraulique passif → bobine s'arrête ✅
  5. Canon s'arrête d'arroser ✅

Risques résiduels :
  - Progression (longueur enroulée) perdue en RAM → ⚠️
  - Au redémarrage sans info → risque sur-dose reprise
  - Si EV normalement ouvertes (NO) → restent ouvertes ⚠️
```

### 1.2 Plantage ESP32 (exception, stack overflow, deadlock)

```
Cause : bug logiciel, corruption mémoire, stack insuffisant
Durée : jusqu'au reboot automatique par watchdog (~3s)

Séquence :
  1. ESP32 gelé → plus de tick state_machine
  2. EV figées dans leur dernier état pendant ~3s
  3. Watchdog hardware déclenche reboot
  4. Reboot → ETAT_VEILLE → EV=OFF ✅

Risques résiduels :
  - 3s avec EV dans état inconnu → faible impact
  - Progression perdue → même problème que coupure
  - Si watchdog désactivé → EV figées indéfiniment 🔴
```

### 1.3 Coupure WiFi / perte connexion téléphone

```
Cause : téléphone trop loin, déconnexion navigateur
Impact : AUCUN sur le cycle en cours ✅
  La machine d'états tourne indépendamment du WiFi
  Le cycle continue normalement
  La connexion reprend quand le téléphone est à portée
```

### 1.4 Corruption NVS

```
Cause : coupure pendant une écriture NVS
ESP-IDF gère nativement ce cas :
  NVS utilise un système de journalisation
  Résistant aux coupures pendant l'écriture ✅
  Rollback automatique sur la dernière valeur valide ✅
```

---

## 2. Solutions logicielles

### 2.1 Sauvegarde progression en NVS

#### Fréquence de sauvegarde — tous les 5 mètres enroulés (déjà implémenté ✅)

```
IMPORTANT : cette sauvegarde est déjà en production depuis PR-10.
La section ci-dessous documente l'implémentation existante.

Granularité choisie : 5m (pire cas : 5m perdus si coupure)

Durée réelle d'un cycle : 12 à 20 heures
Longueur tuyau : 330m
→ 330m / 5m = 66 sauvegardes max par cycle

Calcul usure NVS (wear leveling ESP-IDF ~500 000 écritures) :
  66 sauvegardes × 60 cycles/saison = 3 960 écritures/saison
  → 500 000 / 3 960 ≈ 126 saisons ✅ largement suffisant

Comparaison avec sauvegarde toutes les 30s :
  Cycle 20h × 120 sauvegardes/h = 2 400 sauvegardes/cycle
  × 60 cycles/saison = 144 000 écritures/saison
  → 500 000 / 144 000 ≈ 3.5 saisons ⚠️ trop juste

Logique basée sur la progression réelle (mètres)
plutôt que sur le temps → plus cohérente avec la machine
et indépendante de la vitesse d'enroulement.
```

#### Ce qui est déjà implémenté — state_machine.c

```c
// Déjà présent dans tick_state_machine() — ETAT_EN_COURS
// Variable : s_longueur_derniere_nvs (static float, initialisée à 0)

if (s_longueur_enroulee - s_longueur_derniere_nvs >= 5.0f) {
    config_nvs_sauver_longueur(s_longueur_enroulee);  // namespace irri_state
    config_nvs_sauver_deroule(s_longueur_deroule_m);  // namespace irri_state
    s_longueur_derniere_nvs = s_longueur_enroulee;
    ESP_LOGI(TAG, "Session sauvegardee — %.1fm", s_longueur_enroulee);
}
```

#### Ce qui manque — flag de détection coupure

La longueur est sauvée, mais le firmware ne distingue pas un ARRET_FINAL propre
d'une coupure de courant (les deux laissent `longueur_m > 0` en NVS).

**Ajout minimal** : une clé NVS `session_active` dans `irri_state` :
- Mise à `1` à l'entrée de ETAT_EN_COURS / ETAT_REMPLISSAGE_POUMON
- Mise à `0` à l'entrée de ETAT_ARRET_FINAL et lors d'un RESET

```c
// config_nvs.h — ajout
esp_err_t config_nvs_sauver_session_active(bool actif);
bool config_nvs_lire_session_active(void);
```

#### Clés NVS — namespace `irri_state` (existant + ajout)

| Clé | Type | État | Description |
|---|---|---|---|
| `longueur_m` | blob (float) | ✅ existant | Longueur enroulée (m) |
| `deroule_m` | blob (float) | ✅ existant | Longueur déployée (m) |
| `urgence` | str | ✅ existant | Raison arrêt urgence |
| `prog_actif` | i32 | ✅ existant | Index programme actif |
| `session_act` | uint8 | 🆕 à ajouter | 1 = session était en cours |

### 2.2 Détection coupure au redémarrage

La récupération de longueur au boot est **déjà implémentée** dans `state_machine_init()`.
Ce qui manque : utiliser le flag `session_active` pour distinguer une coupure d'un arrêt propre,
et afficher l'alerte UI correspondante.

```c
// Dans state_machine_init() — après config_nvs_lire_longueur/deroule
// (à ajouter à l'implémentation existante)

bool session_etait_active = config_nvs_lire_session_active();
if (session_etait_active && s_longueur_enroulee > 0.0f) {
    ESP_LOGW(TAG, "Coupure detectee pendant session (enroule=%.1fm)",
             s_longueur_enroulee);
    s_coupure_detectee = true;  // exposé dans machine_status_t → JSON WebSocket
    // NE PAS effacer — l'operateur choisit depuis l'UI
}
// Si session_active=0 mais longueur>0 → arrêt propre (ARRET_FINAL ou RESET)
// → comportement normal, pas d'alerte
```

#### Comportement web UI après coupure détectée

```
Au redémarrage si coupure détectée :

┌────────────────────────────────────────┐
│  ⚠️ COUPURE DÉTECTÉE                   │
│                                        │
│  Cycle interrompu le 29/05 à 14:32     │
│  Programme    : Maïs                   │
│  Longueur     : 127.5 m enroulés       │
│  Durée        : 01:23 écoulée          │
│  Surface      : 2 295 m²               │
│                                        │
│  [▶ Reprendre depuis 127.5m]           │
│  [↺ Réinitialiser (repartir de 0)]     │
└────────────────────────────────────────┘

Reprendre :
  → Charge longueur_enroulee depuis NVS
  → Repart en ETAT_VEILLE avec compteurs restaurés
  → Attend pression pour relancer automatiquement

Réinitialiser :
  → Efface session NVS
  → Repart de zéro compteurs
```

### 2.3 Watchdog logiciel (TWDT)

```c
// Dans state_machine.c

#include "esp_task_wdt.h"

#define TWDT_TIMEOUT_S  3   // Reboot si bloqué > 3s

void state_machine_task(void *pv) {

    // Enregistrer cette tâche au Task Watchdog Timer
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI("state_machine", "TWDT activé — timeout %ds",
             TWDT_TIMEOUT_S);

    while (1) {
        securites_watchdog();   // TOUJOURS EN PREMIER
        tick_state_machine();
        esp_task_wdt_reset();   // Nourrir le chien
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

#### Configuration dans sdkconfig.defaults

```
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_INIT=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=3
CONFIG_ESP_TASK_WDT_PANIC=y    ← reboot au lieu de hang
```

### 2.4 Surveillance tâche telemetry

```c
// La tâche telemetry_task (webserver.c) ne pilote pas les EV
// Son blocage peut indiquer un problème mémoire

static uint32_t s_telemetry_heartbeat = 0;

void telemetry_task(void *pv) {        // nom réel dans le code
    while (1) {
        webserver_broadcast_status();
        s_telemetry_heartbeat++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Vérifier dans state_machine_task (toutes les 10s) :
// Si s_telemetry_heartbeat n'a pas changé → log warning
// (pas de reboot — juste indication de problème)
```

---

## 3. Solution hardware — Circuit fail-safe RC

Protection absolue indépendante du logiciel.
Coupe les EV si l'ESP32 est gelé, même sans reboot.

> **Statut** : composants non encore disponibles.
> Le heartbeat logiciel est activable depuis l'UI Config → Machine (défaut OFF).
> Le circuit hardware sera câblé ultérieurement.

### Principe

```
ESP32 GPIO 2 → signal "je suis vivant" (toggle 1Hz)
             → Circuit RC
             → Signal absent > 2s
               → Transistor coupe alimentation EV
               → EV se ferment (si NC) ✅

Indépendant du firmware — fonctionne même si
l'ESP32 est totalement gelé sans reboot.
GPIO 2 = LED bleue intégrée ESP32 → indicateur visuel bonus.
```

### Option configurable UI — Config → Machine

```
Toggle : "Circuit RC heartbeat (GPIO 2)"
  OFF (défaut) : GPIO 2 à 0, LED bleue éteinte
  ON           : toggle 1Hz, LED bleue clignote

Activer uniquement quand le circuit RC est câblé.
Si circuit présent mais option OFF → transistor bloqué → EV coupées.
```

Implémentation :
- Champ `heartbeat_rc_on` (bool) dans `config_machine_t`
- Clé NVS `"hb_rc"` dans namespace `irri_machine` (défaut false)
- Exposé dans `machine_status_t` et WebSocket JSON → UI peut afficher l'état

### Schéma circuit RC

```
GPIO 2 ──[4.7kΩ]──┬── Base NPN (BC337)
                   │
         [100µF] ──┤ (vers GND)
                   │
         Collecteur NPN ── Contrôle alim EV (en série)
         Émetteur NPN   ── GND

Temporisation : τ = R × C = 4 700 × 100×10⁻⁶ = 0.47s
  Signal absent 2s ≈ 4τ → condensateur à ~2% → NPN bloqué ✅

Fonctionnement :
  Signal 1Hz actif   → condensateur rechargé chaque 500ms ✅
  Signal absent > 2s → condensateur déchargé → NPN bloqué
                      → alimentation EV coupée ✅
```

### Code heartbeat GPIO

```c
// Dans state_machine_task — à la fin de chaque tick
// now_ms() = esp_timer_get_time() / 1000  (déjà défini dans state_machine.c)

static bool     s_heartbeat_level   = false;
static int64_t  s_last_heartbeat_ms = 0;

if (s_cfg_machine.heartbeat_rc_on &&
    now_ms() - s_last_heartbeat_ms > 500) {
    s_heartbeat_level = !s_heartbeat_level;
    gpio_set_level(PIN_HEARTBEAT, s_heartbeat_level);
    s_last_heartbeat_ms = now_ms();
} else if (!s_cfg_machine.heartbeat_rc_on) {
    gpio_set_level(PIN_HEARTBEAT, 0);  // LED éteinte si désactivé
}
```

### Composants circuit RC

| Composant | Valeur | Prix |
|---|---|---|
| Résistance | 4.7 kΩ | < 0.10€ |
| Condensateur électrolytique | 100 µF / 16V | 0.20€ |
| Transistor NPN | BC337 ou 2N2222 | 0.10€ |
| **Total** | | **~0.40€** |

> Ce circuit est optionnel si vos EV sont NC.
> Il devient OBLIGATOIRE si vos EV sont NO.

---

## 4. Vérification type EV — point critique

```
⚠️ À VÉRIFIER IMPÉRATIVEMENT sur votre machine

Test simple au multimètre (EV débranchée de l'ESP32) :
  Bobine EV non alimentée :
    Continuité entre les deux ports hydrauliques ?
    OUI → EV Normalement Ouverte (NO) ⚠️
    NON → EV Normalement Fermée (NC) ✅

EV Normalement Fermée (NC) — comportement sûr :
  Sans alimentation → circuit hydraulique fermé
  Perte d'alim → canon s'arrête ✅
  Plantage ESP32 → EV se ferment au reboot ✅

EV Normalement Ouverte (NO) — comportement à risque :
  Sans alimentation → circuit hydraulique ouvert
  Perte d'alim → canon continue d'arroser ⚠️
  → Dans ce cas : circuit RC hardware OBLIGATOIRE
```

---

## 5. Résumé des mesures de robustesse

### Tableau complet

| Scénario | Risque | Mesure logicielle | Mesure hardware |
|---|---|---|---|
| Coupure alim (EV NC) | Perte progression | Sauvegarde NVS / 10m | — |
| Coupure alim (EV NO) | Canon reste ouvert | — | Circuit RC obligatoire |
| Plantage + reboot TWDT | 3s EV figées | TWDT 3s | Circuit RC optionnel |
| Plantage sans reboot | EV figées indéfiniment | TWDT obligatoire | Circuit RC recommandé |
| Perte WiFi | Aucun | Architecture indépendante ✅ | — |
| Corruption NVS | Config perdue | NVS journalisée ✅ | — |
| Batterie faible | Coupure imminente | Alerte web UI | — |

### Priorités d'implémentation

```
Priorité 1 — OBLIGATOIRE (à implémenter en PR-14)
  ✅ Sauvegarde session NVS / 5m — déjà en production
  ✅ Récupération longueur au boot — déjà en production (partielle)
  🔲 Flag session_active NVS (clé "session_act" dans irri_state)
  🔲 TWDT sur state_machine_task (timeout 3s)
  🔲 Détection coupure au redémarrage + alerte UI

Priorité 2 — FORTEMENT RECOMMANDÉ
  ⚠️ Vérifier type EV (NC ou NO) au multimètre
  ⚠️ Circuit RC si EV normalement ouvertes
  🔲 Heartbeat GPIO 2 avec option UI (défaut OFF)

Priorité 3 — OPTIONNEL
  ○ Circuit RC même avec EV NC (sécurité maximale)
  ○ Surveillance heartbeat tâche telemetry
```

---

## 6. GPIO ajouté

| GPIO | Signal | Note |
|---|---|---|
| **2** | Heartbeat circuit RC | Toggle 1Hz si `heartbeat_rc_on=true` — LED bleue ESP32 intégrée. Défaut OFF (circuit non câblé). |

---

## 7. Fichiers à créer / modifier

```
main/
├── state_machine.c     ← TWDT + flag session_active + heartbeat (si hb_rc=true)
├── config_nvs.c/h      ← clé "session_act" dans irri_state (existant) +
│                          clé "hb_rc" dans irri_machine (existant)
├── machine_status_t    ← ajouter champ bool coupure_detectee + bool heartbeat_rc_on
├── webserver.c         ← alerte coupure dans JSON WebSocket
├── webui/index.html    ← popup coupure détectée + boutons reprendre/reinitialiser
│                          toggle heartbeat RC dans Config → Machine
└── gpio_config.h       ← #define PIN_HEARTBEAT 2
```

> Note : il n'existe pas de `webui.h`. L'UI est dans `main/webui/index.html` (fichier embarqué via EMBED_FILES).

---

## 8. PR associée

```
PR-14 : Robustesse — coupures et plantages
  Après PR-13 (reprendre apres securite debordement + 3-tap)
  Commits :
    feat: flag session_active NVS (detection coupure propre vs arrachage)
    feat: alerte coupure web UI + boutons reprendre/reinitialiser
    feat: TWDT sur state_machine_task (3s)
    feat: heartbeat GPIO 2 pour circuit RC (option configurable UI)
    feat: toggle heartbeat_rc_on dans Config Machine
    docs: schema circuit RC fail-safe
    test: scenarios 14-16 dans SPECS_TESTS
```

---

## 9. Scénarios de test à ajouter dans SPECS_TESTS.md

```
Scénario 14 — Coupure pendant EN_COURS
  Simuler : state_machine_reset() après 30m enroulés
  Vérifier : session sauvegardée tous les 10m en NVS
  Vérifier : au redémarrage → alerte coupure affichée
  Vérifier : [Reprendre] → longueur restaurée ✅

Scénario 15 — TWDT déclenché
  Simuler : bloquer state_machine_task 4s
  Vérifier : reboot après 3s ✅
  Vérifier : EV=OFF après reboot ✅

Scénario 16 — Reprise après coupure
  Simuler : coupure à 127m enroulés (sauvegarde à 120m)
  Note    : précision ±10m (granularité sauvegarde)
  Action  : [Reprendre depuis 120m]
  Vérifier : compteurs restaurés depuis NVS
  Vérifier : cycle repart correctement depuis 120m
```

---

*ROBUSTESSE.md — Irrifrance ESP32 v3*
*Sauvegarde session : tous les 5m enroulés (déjà en prod depuis PR-10)*
*Cycles terrain : 12 à 20 heures*
*Usure NVS estimée : ~126 saisons (calcul 5m × 60 cycles/saison)*
