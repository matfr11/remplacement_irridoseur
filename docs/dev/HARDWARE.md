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
│  OUT1 (GPIO 16) → EV_CANON  12V  (MOSFET principal)│
│  OUT2 (GPIO 17) → EV_POUMON 12V  (MOSFET principal)│
│  OUT3 (GPIO 26) → EV_CANON  12V  (MOSFET secours)  │
│  OUT4 (GPIO 27) → EV_POUMON 12V  (MOSFET secours)  │
│                                                     │
│  GPIO  0 ←── Bouton physique carte                 │
│  GPIO  2  ──► Relais SPDT EV_CANON  (NC/NO switch) │
│  GPIO  4  ──► Relais SPDT EV_POUMON (NC/NO switch) │
│  GPIO 21  ──► I2C SDA (INA3221)                    │
│  GPIO 22  ──► I2C SCL (INA3221)                    │
│  GPIO 23  ──► LED carte (heartbeat RC 1Hz)         │
│  GPIO 25 ←── Pressostat (NC, pull-up 10kΩ)         │
│  GPIO 32 ←── Sécurité spires (NC, pull-up 10kΩ)    │
│  GPIO 33 ←── Contact poumon plein (NC, pull-up 10kΩ)│
│  GPIO 34 ←── Capteur vitesse (diviseur 10k/3.3k)   │
│  GPIO 35 ←── Fin de course (NC, pull-up 10kΩ)      │
└─────────────────────────────────────────────────────┘
```

---

## Bornier 12 voies — tableau complet

| # | Signal | Direction | Câble | Conditionnement |
|---|---|---|---|---|
| 1 | 12V+ batterie | IN | Rouge | Directement sur borne alim carte |
| 2 | GND | — | Noir | Masse commune |
| 3 | EV_CANON 12V (+) | OUT | — | Via relais SPDT → MOSFET OUT1 ou OUT3 selon état secours |
| 4 | EV_POUMON 12V (+) | OUT | — | Via relais SPDT → MOSFET OUT2 ou OUT4 selon état secours |
| 5 | Pressostat A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 25 |
| 6 | Pressostat B | IN | — | GND |
| 7 | Fin de course A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 35 |
| 8 | Fin de course B | IN | — | GND |
| 9 | Sécurité spires A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 32 |
| 10 | Sécurité spires B | IN | — | GND |
| 11 | Contact poumon A | IN | — | Pull-up 10kΩ vers 3.3V → GPIO 33 |
| 12 | Contact poumon B | IN | — | GND |

> Capteur vitesse (GPIO 34) : connexion directe sur borne GPIO ESP32.
> Tension batterie mesurée par INA3221 CH3 (I2C) — plus de diviseur résistif sur GPIO 36.

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
| 25 | Pressostat | Pression présente ✅ | Pression absente ⚠️ |
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

## Mesure tension batterie — INA3221 CH3 (I2C)

La tension batterie est mesurée par le canal 3 du module INA3221 via I2C — pas de diviseur
résistif ni d'ADC. GPIO 21 (SDA) et GPIO 22 (SCL) sont partagés entre les trois canaux.

```
Batterie 12V ──── INA3221 CH3 V_BUS (+) ────┐
                  INA3221 CH3 V_BUS (-) ──── GND
                  INA3221 SDA ────────────── GPIO 21
                  INA3221 SCL ────────────── GPIO 22
```

**INA3221 — adresse I2C** : 0x40 (broche A0 reliée à GND)

| Canal | Signal | Mesure |
|---|---|---|
| CH1 | EV_CANON | Tension (V) + courant (mA) — détection panne MOSFET |
| CH2 | EV_POUMON | Tension (V) + courant (mA) — détection panne MOSFET |
| CH3 | Batterie 12V | Tension (V) — niveau batterie |

Résolution bus voltage : 8 mV/LSB. Shunt R = 0,1 Ω → résolution courant : 0,4 mA/LSB.

> Avantage vs ADC GPIO 36 : pas de conflit ADC2/WiFi, mesure tension ET courant simultanée
> sur les canaux EV, libère GPIO 36 pour d'éventuels besoins futurs.

---

## Relais SPDT — basculement MOSFET principal / secours

Deux modules relais SPDT 12V bobine / signal 3,3V sont ajoutés pour basculer automatiquement
vers les MOSFETs de secours (OUT3/OUT4) en cas de panne du MOSFET principal (OUT1/OUT2).

**Configuration physique** : cavalier du module positionné sur déclenchement niveau HAUT.

```
                    ┌────── COM ─── EV (câble terrain)
GPIO  2 → Relais 1 ─┤ LOW  = repos = NC ─── MOSFET OUT1 (GPIO 16) — principal
(EV_CANON)          └ HIGH = actif = NO ─── MOSFET OUT3 (GPIO 26) — secours

                    ┌────── COM ─── EV (câble terrain)
GPIO  4 → Relais 2 ─┤ LOW  = repos = NC ─── MOSFET OUT2 (GPIO 17) — principal
(EV_POUMON)         └ HIGH = actif = NO ─── MOSFET OUT4 (GPIO 27) — secours
```

**Séquence de basculement** (automatique, sans interruption de l'arrosage) :
1. INA3221 détecte anomalie (CC, HS ouvert, EV débranchée)
2. OUT3 ou OUT4 est pré-synchronisé avec le niveau courant de OUT1/OUT2 (anti-glitch)
3. Relais bascule : COM passe de NC (principal) à NO (secours)
4. LED/badge UI indique `SECOURS ACTIF`
5. Si secours aussi défaillant → `ARRET_URGENCE`

---

## Circuit RC fail-safe (heartbeat GPIO 23)

La carte génère un signal **toggle 1Hz sur GPIO 23** (LED verte carte) si l'option
`heartbeat_rc_on` est activée dans Config → Machine. Ce signal peut alimenter un circuit RC
externe qui coupe les EV si l'ESP32 plante (watchdog HW).

Concept : condensateur chargé régulièrement par le toggle GPIO 23 ; si le signal cesse
(plantage ESP32), le condensateur se décharge et un transistor coupe les EV.

GPIO 23 = `PIN_HEARTBEAT` = `PIN_LED_CARTE` (même broche — le clignotement 1Hz est visible
sur la LED verte de la carte). Désactivé par défaut (`heartbeat_rc_on = false`).

---

## Points de test et de mesure terrain

| Point | Comment mesurer | Valeur normale |
|---|---|---|
| Tension batterie | Multimètre sur bornes 1-2 | 11.5..14V |
| Tension batterie INA3221 | Log UART `batterie_v` ou UI web | Cohérent avec multimètre ±0.1V |
| Signal GPIO 34 (vitesse) | Oscilloscope sur GPIO 34 | Fronts 0-3V, fréquence variable |
| EV_CANON ouverte | Multimètre sur sortie MOSFET actif | ~12V (Vbat - chute MOSFET) |
| EV_CANON courant | INA3221 CH1 log UART | 500..1200 mA selon EV |
| Relais SPDT au repos | Multimètre continuité COM–NC | Fermé (LOW sur GPIO 2/4) |
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
