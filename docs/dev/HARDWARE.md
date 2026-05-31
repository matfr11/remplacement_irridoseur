# Matériel et câblage — Irrifrance ESP32

---

## Carte : ESP32 Quad MOS Switch Module

Carte de développement AliExpress avec ESP32-D0WD-V3 (ESP32-32E) et 4 canaux MOSFET 12V intégrés.

```
┌─────────────────────────────────────────────────────┐
│            ESP32 Quad MOS Switch Module              │
│                                                     │
│  [USB-C]  ←── programmation + alimentation          │
│  [12V IN] ←── alimentation terrain (batterie 12V)  │
│                                                     │
│  OUT1  → EV_CANON  12V   ⚠️ GPIO provisoire (25)   │
│  OUT2  → EV_POUMON 12V   ⚠️ GPIO provisoire (26)   │
│  OUT3  → non utilisé                               │
│  OUT4  → non utilisé                               │
│                                                     │
│  GPIO 27 ←── Pressostat (NC, pull-up 10kΩ)         │
│  GPIO 32 ←── Sécurité spires (NC, pull-up 10kΩ)    │
│  GPIO 33 ←── Contact poumon plein (NC, pull-up 10kΩ)│
│  GPIO 34 ←── Capteur vitesse (diviseur 10k/3.3k)   │
│  GPIO 35 ←── Fin de course (NC, pull-up 10kΩ)      │
│  GPIO 36 ←── Tension batterie (ADC, diviseur R)    │
└─────────────────────────────────────────────────────┘
```

> **⚠️ Action requise avant mise en service** : identifier les pins OUT1/OUT2 sur le schéma
> technique de la carte Quad MOS et mettre à jour `PIN_EV_CANON` et `PIN_EV_POUMON` dans
> `main/gpio_config.h`. Des `#warning` à la compilation rappellent cette action.

---

## Bornier 12 voies — tableau complet

| # | Signal | Direction | Câble | Conditionnement |
|---|---|---|---|---|
| 1 | 12V+ batterie | IN | Rouge | Directement sur borne alim carte |
| 2 | GND | — | Noir | Masse commune |
| 3 | EV_CANON 12V (+) | OUT | — | Via MOSFET OUT1, 12V commuté |
| 4 | EV_POUMON 12V (+) | OUT | — | Via MOSFET OUT2, 12V commuté |
| 5 | Pressostat A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 27 |
| 6 | Pressostat B | IN | — | GND |
| 7 | Fin de course A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 35 |
| 8 | Fin de course B | IN | — | GND |
| 9 | Sécurité spires A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 32 |
| 10 | Sécurité spires B | IN | — | GND |
| 11 | Contact poumon A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 33 |
| 12 | Contact poumon B | IN | — | GND |

> Capteur vitesse (GPIO 34) et batterie (GPIO 36) : connexion directe sur bornes GPIO ESP32.

---

## Logique des signaux

Tous les contacts d'entrée sont **NC (Normalement Fermés)** avec **logique active HIGH** :

```
Contact NC :
  Câble intact, contact fermé → circuit fermé → GPIO tenu à GND (LOW) = NORMAL
  Câble coupé OU contact ouvert → GPIO monte via pull-up → HIGH = DANGER

Logique :
  GPIO LOW  = repos / normal / OK
  GPIO HIGH = danger / actif / incident
```

Cette convention garantit le fail-safe câblage : si un fil se coupe, le système détecte une
anomalie (HIGH) plutôt que d'ignorer le problème.

### Tableau de référence

| GPIO | Signal | LOW signifie | HIGH signifie |
|---|---|---|---|
| 27 | Pressostat | Pression présente ✅ | Pression absente ⚠️ |
| 32 | Sécurité spires | Normal ✅ | Débordement bobine 🔴 |
| 33 | Contact poumon | Remplissage en cours | Poumon plein ✅ |
| 34 | Capteur vitesse | — | Front montant = impulsion |
| 35 | Fin de course | Canon déroulé ✅ | Canon rentré / en position |

---

## Diviseur de tension — capteur vitesse (GPIO 34)

Le capteur de vitesse produit un signal 12V (sortie transistor NPN open-collector). Le diviseur
ramène ce signal à moins de 3.3V pour l'ESP32.

```
12V ──┤
       R1 = 10kΩ
      ├──────────────── GPIO 34 (input, pas de pull-up interne)
       R2 = 3.3kΩ
      ├──────────────── GND
```

**Calcul** :
```
V_gpio = V_in × R2 / (R1 + R2)
       = 12V × 3300 / (10000 + 3300)
       = 12V × 0.248
       = 2.98V  ← en dessous du seuil 3.3V ✅
```

GPIO 34 est **input-only** sur ESP32 (pas de pull-up interne, pas de sortie possible).

Anti-rebond ISR : 50ms (`DEBOUNCE_VITESSE_MS`). Les fronts plus rapprochés sont ignorés.

---

## Diviseur de tension — batterie (GPIO 36)

La batterie 12V nominale peut monter jusqu'à ~14V en charge. GPIO 36 supporte max 3.3V.

```
V_bat ──┤
         R1 = 100kΩ
        ├──────────────── GPIO 36 (ADC1 canal 0)
         R2 = 27kΩ
        ├──────────────── GND
```

**Calcul** :
```
V_gpio = V_bat × R2 / (R1 + R2)
       = V_bat × 27000 / (100000 + 27000)
       = V_bat × 0.2126

Plage mesure :
  11.0V → 2.34V
  12.6V → 2.68V
  14.0V → 2.98V  ← max attendu, sous 3.3V ✅

Formule inverse (dans batterie.c) :
  V_bat = V_gpio × (R1 + R2) / R2
        = V_gpio × 4.703...
```

Mesure : moyenne de 16 lectures ADC (`BATT_ADC_NB_SAMPLES`) pour filtrer le bruit.
GPIO 36 = ADC1 canal 0, pas de pull-up interne.

---

## Circuit RC fail-safe (si implémenté)

> [À IMPLÉMENTER si décidé] — Non présent dans le firmware actuel.

Un circuit RC matériel pourrait forcer EV=OFF si l'ESP32 plante (watchdog HW).
Concept : condensateur chargé régulièrement par une impulsion GPIO de l'ESP32 ; si l'impulsion
cesse (plantage), le condensateur se décharge et un transistor coupe les EV.

---

## Points de test et de mesure terrain

| Point | Comment mesurer | Valeur normale |
|---|---|---|
| Tension batterie | Multimètre sur bornes 1-2 | 11.5..14V |
| Signal GPIO 34 (vitesse) | Oscilloscope sur GPIO 34 | Fronts 0-3V, fréquence variable |
| EV_CANON ouverte | Multimètre sur sortie MOSFET | ~12V (Vbat - chute MOSFET) |
| Continuité capteur vitesse | Mode continuité bornes 9-10 | Contact fermé (LOW) au repos |
| t_vidange | Chronomètre : temps entre EV_POUMON=OFF et cliquet retracté | 0.5..3s selon machine |
| cycles_par_tour | Compter les cycles poumon pendant un tour de bobine | 40 sur ST1 Bis |

### Procédure mesure t_vidange

1. Démarrer une session (mode simulateur ou terrain)
2. Observer les logs série : `SOUS_REMPLISSAGE → SOUS_VIDANGE` avec timestamp
3. Mesurer physiquement le temps entre fin remplissage et retrait complet cliquet
4. Reporter dans Config → Machine → t_vidange_s

### Procédure mesure cycles_par_tour

1. Marquer une position de référence sur la bobine (ruban adhésif)
2. Démarrer l'enroulement
3. Compter les cycles poumon (bruit claquement) pendant un tour complet
4. Reporter dans Config → Machine → cycles_par_tour

Valeur mesurée physiquement sur ST1 Bis : **40 cycles/tour**.
