#pragma once

// =============================================================================
// gpio_config.h — Affectation GPIO Irrifrance ESP32
//
// Carte : ESP-32D DevKit + shield breakout Heemol 38 pins + module 4 MOSFET externe
//         Alimentation : LM2596 #1 12V→5V (ESP32 via pin 5V) + LM2596 #2 12V→6V (EV)
//
// Choix des GPIO optimisés :
//   - EV sur GPIO 18/19/26/27 → UART2 (16/17) libéré pour LoRa/GSM V3
//   - TPL5010 DONE sur GPIO 23 → JTAG complet libéré (12/13/14/15)
//   - ADC1 uniquement pour les capteurs analogiques (compatible WiFi)
//   - UART2 (16/17) réservé télémétrie V3
//   - ADC1 CH0 (36) et CH3 (39) réservés capteur pression V3
// =============================================================================

// -----------------------------------------------------------------------------
// SORTIES — Électrovannes bistables Bürkert S78722
// Alimentées en 6V via LM2596 externe
// Impulsion 100-200ms pour changer d'état (configurable NVS)
// -----------------------------------------------------------------------------

#define PIN_EV_CANON_OUVRIR     18      // MOSFET module externe — impulsion OUVRIR canon
#define PIN_EV_CANON_FERMER     26      // MOSFET module externe — impulsion FERMER canon (ex-14, libère JTAG TMS)
#define PIN_EV_POUMON_OUVRIR    19      // MOSFET module externe — impulsion OUVRIR poumon
#define PIN_EV_POUMON_FERMER    27      // MOSFET module externe — impulsion FERMER poumon (ex-4, libère strapping)

#define DUREE_IMPULSION_EV_MS      100
#define DUREE_IMPULSION_EV_MS_MIN   20
#define DUREE_IMPULSION_EV_MS_MAX  500

// -----------------------------------------------------------------------------
// ENTRÉES — Capteurs et contacts
// Pull-up externe 10 kΩ vers 3,3V (bornier Voltage Out carte)
// Logique contacts NC : LOW = normal, HIGH = actif/danger
// -----------------------------------------------------------------------------

// Capteur vitesse bobine — diviseur 10kΩ/5,6kΩ (signal 8V → 2,87V)
// Hors pastille : 1,9V → 0,68V LOW | Sur pastille : 8,0V → 2,87V HIGH
// ADC1 CH6 — compatible WiFi ✅ — input only
#define PIN_CAPTEUR_VITESSE     34

// Fin de course canon — pull-up externe 10kΩ, contact NC
// LOW = canon déroulé (normal) | HIGH = canon rentré (SEC-1)
// ADC1 CH7 — input only
#define PIN_FIN_COURSE          35

// Sécurité spires (débordement bobine) — pull-up externe 10kΩ, contact NC
// LOW = normal | HIGH = débordement (SEC-2, priorité absolue)
// ADC1 CH4
#define PIN_SECU_SPIRES         32

// Contact poumon plein — pull-up externe 10kΩ, contact NC
// LOW = poumon vide (normal) | HIGH = poumon plein (fin remplissage)
// ADC1 CH5
#define PIN_POUMON_PLEIN        33

// Pressostat — pull-up externe 10kΩ, contact NC
// LOW = pression présente (normal) | HIGH = pression absente (pause)
// Type NC/NO à confirmer terrain via feature inversion contacts
// ADC2 CH8 — utilisé en digital uniquement (pas en ADC) ✅
#define PIN_PRESSOSTAT          25

// -----------------------------------------------------------------------------
// Capteur vitesse — paramètres ISR
// -----------------------------------------------------------------------------

#define NB_PASTILLES            10
#define DEBOUNCE_VITESSE_MS     50

// -----------------------------------------------------------------------------
// I2C — INA3221 (surveillance batterie CH3)
// Adresse : 0x40 (pin A0 sur GND)
// Bornier dédié ES30G29 : 21 SDA / 22 SCL / VCC / GND
// -----------------------------------------------------------------------------
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22
#define INA3221_I2C_ADDR        0x40
#define INA3221_CH_BATTERIE     3

// Seuils surveillance batterie (configurables NVS)
#define SEUIL_BATT_ALERTE_V     11.5f
#define SEUIL_BATT_CRITIQUE_V   11.0f

// -----------------------------------------------------------------------------
// Heartbeat — LED bleue intégrée DevKit ESP32
// Toggle 1Hz (activé via Config → Machine → heartbeat_rc_on)
// -----------------------------------------------------------------------------
#define PIN_HEARTBEAT            2

// -----------------------------------------------------------------------------
// TPL5010DDCR — Watchdog matériel externe
// DONE → impulsion périodique depuis state_machine_task()
// RESET → broche EN ESP32 : reboot si timeout (Rext=3,3MΩ → ~35s)
// GPIO 23 choisi : évite JTAG TCK (GPIO 13), libère UART2 (16/17)
// -----------------------------------------------------------------------------
#define PIN_TPL5010_DONE        23

// -----------------------------------------------------------------------------
// GPIO RÉSERVÉS — NE PAS UTILISER
// -----------------------------------------------------------------------------
// GPIO 6–11   → Flash SPI interne ❌
// GPIO 16, 17 → UART2 réservé télémétrie LoRa/GSM V3
// GPIO 36, 39 → ADC1 réservé capteur pression analogique V3

// -----------------------------------------------------------------------------
// GPIO LIBRES — disponibles pour évolutions futures
// -----------------------------------------------------------------------------
// GPIO 36 (ADC1 CH0)  → capteur pression analogique V3 ✅
// GPIO 39 (ADC1 CH3)  → libre ✅
// GPIO 16 (UART2 RX)  → LoRa/GSM RX V3 ✅
// GPIO 17 (UART2 TX)  → LoRa/GSM TX V3 ✅
// GPIO 12             → JTAG TDI ✅ (libéré depuis migration Heemol)
// GPIO 13             → JTAG TCK ✅ (libéré depuis migration Heemol)
// GPIO 14             → JTAG TMS ✅ (libéré depuis migration Heemol, ex-EV_CF)
// GPIO 15             → JTAG TDO ✅ (libéré depuis migration Heemol)
// GPIO 5  ⚠️ strapping pin (SDIO timing au boot) — vérifier compatibilité avant usage
