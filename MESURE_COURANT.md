# MESURE_COURANT — Irrifrance ESP32
## Surveillance MOSFETs EV + tension batterie via INA3221

> Ce document remplace la version précédente de MESURE_COURANT.md.
> Il couvre la détection de défaillance des MOSFETs EV_CANON
> et EV_POUMON, le basculement automatique sur les canaux de secours
> (OUT3/OUT4) via modules relais, et la mesure tension batterie —
> le tout via un unique module INA3221 3 canaux I2C.

---

## 1. Principe

```
Chaque MOSFET peut défaillir de deux façons :

  Court-circuit (la plus courante) :
    Canal toujours conducteur
    EV alimentée en permanence même GPIO=LOW ⚠️

  Circuit ouvert :
    Canal ne conduit plus jamais
    EV jamais alimentée même GPIO=HIGH ⚠️

Détection via INA3221 :
  Mesure tension + courant sur EV_CANON et EV_POUMON
  Cohérence tension/courant mesurés vs état GPIO commandé
  Mesure tension batterie sur CH3

Basculement :
  Si anomalie → relais SPDT bascule COM de NC vers NO
  MOSFET principal isolé → MOSFET secours prend le relais
  Cycle continue sans interruption ✅
```

---

## 2. Module INA3221

### Identification

```
MCU-3221 / INA3221
3 canaux tension + courant
Interface I2C
Shunts R100 (0.1Ω) intégrés sur CH1, CH2, CH3
Pins : VPU, GND, GND, SDA, SCL, VS, A0, GND
Prix : ~2.40€
```

### Affectation des canaux

| Canal | Usage | Mesures |
|---|---|---|
| **CH1** | EV_CANON | Tension + courant |
| **CH2** | EV_POUMON | Tension + courant |
| **CH3** | Batterie 12V | Tension uniquement |

### Adresse I2C

```
Pin A0 du module → configure l'adresse :
  A0 → GND  : 0x40 (défaut)
  A0 → VCC  : 0x41
  A0 → SDA  : 0x42
  A0 → SCL  : 0x43

→ Laisser A0 non connecté ou sur GND → adresse 0x40
```

### Câblage INA3221

```
INA3221           ESP32 / Circuit
─────────────────────────────────
VPU  ──────────── 3.3V carte
GND  ──────────── GND
SDA  ──────────── GPIO 21
SCL  ──────────── GPIO 22
VS   ──────────── 12V (alimentation mesure)
A0   ──────────── GND (adresse 0x40)

CH1+ ──────────── 12V après relais 1
CH1- ──────────── Drain MOSFET OUT1 (EV_CANON côté + avant EV)

CH2+ ──────────── 12V après relais 2
CH2- ──────────── Drain MOSFET OUT2 (EV_POUMON côté + avant EV)

CH3+ ──────────── 12V batterie (+)
CH3- ──────────── GND (mesure tension batterie uniquement)
```

---

## 3. Architecture matérielle complète

### Vue d'ensemble

```
Carte Quad MOS :
  GPIO16 → MOSFET OUT1 ──→ NC relais 1 ──┐
  GPIO26 → MOSFET OUT3 ──→ NO relais 1 ──┴──[INA3221 CH1]── EV_CANON

  GPIO17 → MOSFET OUT2 ──→ NC relais 2 ──┐
  GPIO27 → MOSFET OUT4 ──→ NO relais 2 ──┴──[INA3221 CH2]── EV_POUMON

  INA3221 CH3 ── Batterie 12V (remplace diviseur résistif)

Modules relais 1 canal 12V × 2 (cavalier HIGH level trigger) :
  GPIO2  → module relais #1 → bobine relais EV_CANON
  GPIO4  → module relais #2 → bobine relais EV_POUMON

I2C :
  GPIO21 (SDA) + GPIO22 (SCL) → INA3221 addr 0x40
```

### Schéma détaillé EV_CANON

```
12V ──────────────── COM relais 1
                           │
               ┌───────────┴───────────┐
              NC                      NO
               │                       │
        [MOSFET OUT1]           [MOSFET OUT3]
         GPIO16                  GPIO26
         principal                secours
               │                       │
               └───────────┬───────────┘
                            │
                      [INA3221 CH1+]
                      [INA3221 CH1-]
                            │
                         EV_CANON

État normal (GPIO2=LOW → relais repos, cavalier HIGH trigger) :
  COM → NC → MOSFET OUT1 → INA3221 CH1 → EV_CANON ✅
  Relais non alimenté → aucune consommation ✅

Panne MOSFET OUT1 (INA3221 détecte incohérence) :
  GPIO2=HIGH → relais actif → COM → NO
  MOSFET OUT3 → INA3221 CH1 → EV_CANON ✅

Si ESP32 plante (GPIO2 retombe LOW) :
  Relais repos → COM → NC → principal
  Pas de basculement intempestif ✅
```

### Schéma identique pour EV_POUMON

```
12V ─── COM relais 2
  NC → MOSFET OUT2 (GPIO17) principal
  NO → MOSFET OUT4 (GPIO27) secours
  → INA3221 CH2 → EV_POUMON

GPIO4 → module relais #2
```

---

## 4. Matériel

| Composant | Détail | Qté | ~Prix |
|---|---|---|---|
| **INA3221 3 canaux** | I2C, shunts 0.1Ω intégrés, MCU-3221 | 1 | 2.40€ |
| Module relais 1 canal 12V | Optocouplé, NC/NO/COM, cavalier trigger | 2 | 2.00€ |
| **Total** | | | **~4.40€** |

**Ce qui est supprimé vs versions précédentes :**
```
❌ Diviseur 100kΩ/27kΩ batterie (GPIO36 libéré)
❌ Diviseur 100kΩ/27kΩ EV_CANON
❌ Diviseur 100kΩ/27kΩ EV_POUMON
❌ Problème ADC2/WiFi (GPIO13 abandonné)
❌ Problème double init ADC1
❌ batterie.c modifié (gpio_config.h seul suffit)
```

### Configuration cavalier relais — HIGH level trigger

```
GPIO=LOW  → relais repos → COM→NC → MOSFET principal ✅
GPIO=HIGH → relais actif → COM→NO → MOSFET secours ✅

Alimentation séparée :
  VCC signal : 3.3V depuis carte ESP32
  JD-VCC bobine : 12V depuis batterie
```

---

## 5. GPIO — mise à jour complète

| GPIO | Direction | Signal | Note |
|---|---|---|---|
| **2** | OUTPUT | Relais #1 EV_CANON | HIGH=secours actif |
| **4** | OUTPUT | Relais #2 EV_POUMON | HIGH=secours actif |
| **21** | I2C SDA | INA3221 | Partageable avec autres I2C |
| **22** | I2C SCL | INA3221 | Partageable avec autres I2C |
| ~~36~~ | ~~ADC batterie~~ | ~~Libéré~~ | ← Remplacé par INA3221 CH3 |
| ~~39~~ | ~~Mesure EV_CANON~~ | ~~Libéré~~ | ← Remplacé par INA3221 CH1 |
| ~~13~~ | ~~Mesure EV_POUMON~~ | ~~Abandonné~~ | ← ADC2 incompatible WiFi |

---

## 6. Stratégie de vérification

### Trois moments

```
1. AU DÉMARRAGE (avant ETAT_OUVERTURE_CANON)
   Test statique  : EV=OFF → tension doit être 0V, courant 0mA
   Test dynamique : EV=ON 100ms → tension doit être ~12V, courant > seuil
                    EV=OFF → tension revient à 0V
   Durée : ~300ms
   → Diagnostic complet avant tout cycle

2. AVANT chaque changement d'état
   Lire tension + courant INA3221
   Cohérence avec état précédent ?
   → Si non → basculer AVANT la commutation

3. APRÈS chaque changement d'état (délai 100ms)
   Lire tension + courant
   Cohérence avec nouvel état commandé ?
   → Si non → basculer IMMÉDIATEMENT
```

### Logique de détection

```
GPIO=LOW  ET tension > 6V → MOSFET grillé CC ⚠️
GPIO=HIGH ET tension < 1V → MOSFET HS ouvert ⚠️
GPIO=HIGH ET courant < 50mA → EV débranchée / HS ⚠️
GPIO=HIGH ET tension ≈ 12V ET courant > 100mA → OK ✅
GPIO=LOW  ET tension ≈ 0V  ET courant ≈ 0mA  → OK ✅
```

---

## 7. Code

### gpio_config.h — ajouts/corrections

```c
// MOSFET secours (carte Quad MOS OUT3/OUT4)
#define PIN_MOSFET_SECOURS_CANON   26   // QMOS OUT3
#define PIN_MOSFET_SECOURS_POUMON  27   // QMOS OUT4

// Relais de basculement (cavalier HIGH level trigger)
#define PIN_RELAIS_CANON    2    // LOW=NC=principal, HIGH=NO=secours
#define PIN_RELAIS_POUMON   4

// I2C — INA3221
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22
#define INA3221_I2C_ADDR        0x40   // A0 sur GND

// Canaux INA3221
#define INA3221_CH_EV_CANON     1      // CH1
#define INA3221_CH_EV_POUMON    2      // CH2
#define INA3221_CH_BATTERIE     3      // CH3

// Seuils détection
#define SEUIL_TENSION_EV_V      6.0f   // > 6V = EV alimentée
#define SEUIL_COURANT_EV_MA    50.0f   // < 50mA = circuit suspect
#define SEUIL_BATT_FAIBLE_V    11.5f
#define SEUIL_BATT_CRITIQUE_V  11.0f

// SUPPRIMÉ :
// #define PIN_BATT_ADC   36   ← remplacé par INA3221 CH3
// #define PIN_MESURE_EV_CANON   39
// #define PIN_MESURE_EV_POUMON  13 (ou 38)
```

### ina3221.h

```c
#pragma once
#include "esp_err.h"
#include <stdbool.h>

/**
 * Mesure d'un canal INA3221
 */
typedef struct {
    float tension_v;    // Tension bus (V)
    float courant_ma;   // Courant (mA), 0 si pas de shunt utilisé
} ina3221_mesure_t;

/**
 * Initialise le bus I2C et le module INA3221.
 * À appeler dans app_main() après gpio_handler_init().
 */
esp_err_t ina3221_init(void);

/**
 * Lit tension et courant d'un canal (1, 2 ou 3).
 */
ina3221_mesure_t ina3221_lire_canal(int canal);

/**
 * Lecture rapide tension uniquement (plus rapide).
 */
float ina3221_lire_tension(int canal);
```

### mosfet_surveillance.h

```c
#pragma once
#include <stdbool.h>

typedef enum {
    MOSFET_OK,
    MOSFET_GRILLE_CC,    // Court-circuit — tension haute GPIO bas
    MOSFET_HS_OUVERT,    // Circuit ouvert — tension basse GPIO haut
    MOSFET_EV_DEBRANCHEE,// Courant nul GPIO haut → EV absente
} mosfet_etat_t;

typedef struct {
    mosfet_etat_t etat_principal;
    mosfet_etat_t etat_secours;
    bool          secours_actif;
} ev_canal_t;

void           mosfet_surveillance_init(void);
bool           mosfet_test_demarrage(void);
void           mosfet_verifier_avant(int pin_ev, bool etat_actuel);
void           mosfet_verifier_apres(int pin_ev, bool nouvel_etat);
ev_canal_t     mosfet_get_etat(int pin_ev);
bool           mosfet_secours_actif(int pin_ev);
```

### mosfet_surveillance.c — points clés

```c
#include "mosfet_surveillance.h"
#include "ina3221.h"
#include "gpio_config.h"

// Obtenir le canal INA3221 selon l'EV
static int get_ina_canal(int pin_ev) {
    return (pin_ev == PIN_EV_CANON)
        ? INA3221_CH_EV_CANON     // CH1
        : INA3221_CH_EV_POUMON;   // CH2
}

// Vérification cohérence GPIO commandé vs mesure INA3221
static mosfet_etat_t verifier_coherence(int pin_ev,
                                         bool gpio_commande) {
    ina3221_mesure_t m = ina3221_lire_canal(get_ina_canal(pin_ev));

    bool ev_alimentee = (m.tension_v > SEUIL_TENSION_EV_V);
    bool courant_ok   = (m.courant_ma > SEUIL_COURANT_EV_MA);

    if (!gpio_commande && ev_alimentee)
        return MOSFET_GRILLE_CC;

    if (gpio_commande && !ev_alimentee)
        return MOSFET_HS_OUVERT;

    if (gpio_commande && ev_alimentee && !courant_ok)
        return MOSFET_EV_DEBRANCHEE;

    return MOSFET_OK;
}

// Basculement sur secours
static void basculer_sur_secours(int pin_ev, mosfet_etat_t cause) {
    ev_canal_t *c = (pin_ev == PIN_EV_CANON) ? &s_canon : &s_poumon;

    if (c->secours_actif) {
        // Les deux MOSFETs défaillants
        state_machine_declencher_urgence(
            "Défaillance MOSFETs — principal ET secours HS");
        return;
    }

    // Synchroniser OUT3/OUT4 avec état courant avant bascule
    // → évite glitch sur l'EV
    int pin_secours = (pin_ev == PIN_EV_CANON)
                    ? PIN_MOSFET_SECOURS_CANON
                    : PIN_MOSFET_SECOURS_POUMON;
    gpio_set_level(pin_secours, gpio_get_level(pin_ev));

    // Basculer le relais (HIGH = NO = secours)
    c->etat_principal = cause;
    c->secours_actif  = true;
    gpio_set_level(get_pin_relais(pin_ev), 1);

    ESP_LOGW("mosfet", "%s : %s — secours actif",
             pin_ev == PIN_EV_CANON ? "EV_CANON" : "EV_POUMON",
             cause == MOSFET_GRILLE_CC ? "grillé CC" :
             cause == MOSFET_HS_OUVERT ? "HS ouvert" : "EV débranchée");
}
```

### Intégration batterie — batterie.c

```c
/**
 * Remplace la lecture ADC par INA3221 CH3.
 * batterie_lire_voltage() utilise désormais ina3221_lire_tension()
 * au lieu de l'ADC GPIO36.
 *
 * Avantages :
 *   Précision ±1% vs ±5% ADC ✅
 *   GPIO36 libéré ✅
 *   Pas de diviseur résistif ✅
 */
float batterie_lire_voltage(void) {
    return ina3221_lire_tension(INA3221_CH_BATTERIE);
}
```

### Intégration gpio_handler.c

```c
/**
 * Toutes les commandes EV passent par ces fonctions.
 * Surveillance MOSFET intégrée.
 */
void gpio_ev_canon_set(bool actif) {
    bool etat_actuel = gpio_get_level(PIN_EV_CANON);
    mosfet_verifier_avant(PIN_EV_CANON, etat_actuel);

    int pin_reel = mosfet_secours_actif(PIN_EV_CANON)
                 ? PIN_MOSFET_SECOURS_CANON
                 : PIN_EV_CANON;
    gpio_set_level(pin_reel, actif ? 1 : 0);

    mosfet_verifier_apres(PIN_EV_CANON, actif);
}

void gpio_ev_poumon_set(bool actif) {
    bool etat_actuel = gpio_get_level(PIN_EV_POUMON);
    mosfet_verifier_avant(PIN_EV_POUMON, etat_actuel);

    int pin_reel = mosfet_secours_actif(PIN_EV_POUMON)
                 ? PIN_MOSFET_SECOURS_POUMON
                 : PIN_EV_POUMON;
    gpio_set_level(pin_reel, actif ? 1 : 0);

    mosfet_verifier_apres(PIN_EV_POUMON, actif);
}

/**
 * Arrêt d'urgence — DIRECT et INCONDITIONNEL.
 * Ne passe pas par la surveillance MOSFET.
 * Coupe principal ET secours.
 */
void gpio_all_ev_off(void) {
    gpio_set_level(PIN_EV_CANON,              0);
    gpio_set_level(PIN_EV_POUMON,             0);
    gpio_set_level(PIN_MOSFET_SECOURS_CANON,  0);
    gpio_set_level(PIN_MOSFET_SECOURS_POUMON, 0);
}
```

### Ordre d'appel dans app_main()

```c
// Ordre obligatoire :
gpio_handler_init();        // Init EV + OUT3/OUT4 + relais GPIO
ina3221_init();             // Init I2C + INA3221
mosfet_surveillance_init(); // Configure canaux INA3221
batterie_init();            // Utilise INA3221 CH3 (ina3221 déjà init)

if (!mosfet_test_demarrage()) {
    ESP_LOGE("main", "Défaillance MOSFETs critique au démarrage");
    // state_machine gère → ETAT_ARRET_URGENCE
}

state_machine_init();
wifi_ap_init();
webserver_init();
```

---

## 8. Web UI — affichage

```
Onglet Accueil :
  EV_CANON  ● OUVERTE ✅
  EV_CANON  ● OUVERTE ⚠️ SECOURS  ← MOSFET principal défaillant
  EV_POUMON ● FERMÉE  ✅
  Batterie  12.4V ✅              ← via INA3221 CH3

Onglet Stats :
  MOSFET EV_CANON  : ✅ Principal / ⚠️ Secours (grillé CC)
  MOSFET EV_POUMON : ✅ Principal / ⚠️ Secours (HS ouvert)
  Batterie         : 12.4V — Pleine (87%)
```

### Nouveaux champs JSON WebSocket

```json
{
  "mosfet_canon_secours":  false,
  "mosfet_poumon_secours": false,
  "mosfet_canon_etat":     "OK",
  "mosfet_poumon_etat":    "OK",
  "batterie_v":            12.4,
  "batterie_pct":          87,
  "batterie_etat":         "Pleine"
}
```

---

## 9. Bornier — mise à jour finale

| Borne | Signal | Vers |
|---|---|---|
| 1 | 12V | Bornier alim carte + VS INA3221 |
| 2 | GND | GND carte + GND INA3221 |
| 3 | Capteur vitesse | GPIO34 via diviseur |
| 4 | Fin de course | GPIO35 pull-up NC |
| 5 | Sécurité spires | GPIO32 pull-up NC |
| 6 | Contact poumon | GPIO33 pull-up NC |
| 7 | Pressostat | GPIO25 pull-up NC |
| 8 | GND capteurs | GND carte |
| 9 | EV_CANON + | COM relais 1 → NC→OUT1 / NO→OUT3 → INA3221 CH1 |
| 10 | EV_CANON − | GND |
| 11 | EV_POUMON + | COM relais 2 → NC→OUT2 / NO→OUT4 → INA3221 CH2 |
| 12 | EV_POUMON − | GND |

> GPIO36 (batterie ADC) supprimé du bornier — remplacé par INA3221 CH3
> directement câblé sur la borne 1 (12V) et borne 2 (GND).
> Bornier reste à 12 voies ✅

---

## 10. Nouveaux fichiers à créer

```
main/
├── ina3221.c/h              ← nouveau — driver I2C INA3221
├── mosfet_surveillance.c/h  ← nouveau — détection + basculement
```

## Fichiers à modifier

```
main/
├── gpio_config.h    ← ajout PIN_MOSFET_SECOURS, PIN_RELAIS,
│                      I2C pins, canaux INA3221, seuils
│                      suppression PIN_BATT_ADC, PIN_MESURE_EV_*
├── gpio_handler.c/h ← gpio_ev_canon_set/poumon_set avec surveillance
│                      gpio_all_ev_off coupe principal + secours
│                      init OUT3/OUT4 + relais en LOW
├── batterie.c/h     ← lire_voltage() utilise INA3221 CH3
│                      suppression init ADC
├── main.c           ← ordre init + mosfet_test_demarrage()
├── state_machine.c  ← machine_status_t + champs mosfet_*_secours
├── webserver.c      ← JSON status + champs mosfet + batterie
├── CMakeLists.txt   ← ajouter ina3221.c + mosfet_surveillance.c
```

---

## 11. PR associée

```
PR-19 : Surveillance MOSFETs + INA3221 + basculement secours
  Commits :
    feat: driver ina3221 (I2C, 3 canaux, tension+courant)
    feat: batterie_lire_voltage() via INA3221 CH3
    feat: mosfet_surveillance (init, test démarrage, avant/après)
    feat: basculement automatique relais NC→NO sur panne MOSFET
    feat: gpio_ev_canon/poumon_set avec surveillance intégrée
    feat: gpio_all_ev_off coupe principal + secours (fail-safe)
    feat: JSON WebSocket mosfet_*_secours + batterie INA3221
    docs: gpio_config.h + bornier mis à jour
    test: test_mosfet_surveillance + test_ina3221
```

---

*MESURE_COURANT.md — Irrifrance ESP32 v3*
*INA3221 3 canaux I2C : CH1=EV_CANON / CH2=EV_POUMON / CH3=Batterie*
*Basculement via 2 modules relais 1 canal 12V — cavalier HIGH trigger*
*GPIO36 libéré — ADC1 entièrement libéré — problème ADC2/WiFi éliminé*
