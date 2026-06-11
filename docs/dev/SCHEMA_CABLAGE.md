# Schéma de câblage — Irrifrance ESP32

Document de référence pour le **montage complet** du boîtier : synoptique général,
bornier, conditionnement des entrées, chaîne de puissance EV, I2C, et checklist de
mise sous tension. Pour le détail composant par composant (INA3221, TPL5010,
relais, points de test), voir [HARDWARE.md](HARDWARE.md).

> ⚠️ **Règle unifiée (décision 2026-06-11)** : le firmware n'active **aucune pull-up
> interne**. Toutes les entrées contacts exigent leur résistance **10 kΩ externe**.
> Sans elle, la broche flotte : un fil coupé ou un capteur absent peut être lu
> « tout va bien » — y compris sur banc de test.

---

## Synoptique général

```
                                    BOÎTIER IP65
 ┌────────────────────────────────────────────────────────────────────────────┐
 │                                                                            │
 │  ┌────────────────────────────┐         ┌─────────────────┐                │
 │  │  ESP32 Quad MOS Switch     │   I2C   │  INA3221 0x40   │                │
 │  │                            ├─SDA 21──┤  CH1 EV_CANON   │                │
 │  │  12V IN ◄── borne 1/2      ├─SCL 22──┤  CH2 EV_POUMON  │                │
 │  │                            │         │  CH3 Batterie   │                │
 │  │  OUT1 GPIO16 ─► Relais1 NC │         └─────────────────┘                │
 │  │  OUT3 GPIO26 ─► Relais1 NO ├─ COM ─► INA CH1 ─► borne 3-4 ─► EV_CANON   │
 │  │  OUT2 GPIO17 ─► Relais2 NC │                                            │
 │  │  OUT4 GPIO27 ─► Relais2 NO ├─ COM ─► INA CH2 ─► borne 5-6 ─► EV_POUMON  │
 │  │  GPIO2  ─► bobine Relais 1 │                                            │
 │  │  GPIO4  ─► bobine Relais 2 │         ┌─────────────────┐                │
 │  │  GPIO13 ─► TPL5010 DONE    ├─────────┤ TPL5010 watchdog│                │
 │  │  EN     ◄─ TPL5010 RESET   │         │ Rext 3,3MΩ→GND  │                │
 │  │  GPIO23 ─► LED heartbeat   │         └─────────────────┘                │
 │  │                            │                                            │
 │  │  GPIO34 ◄─[diviseur]─ borne 8   ◄── Capteur vitesse (signal)            │
 │  │  GPIO35 ◄─[pull-up]── borne 9   ◄── Fin de course (NC)                  │
 │  │  GPIO32 ◄─[pull-up]── borne 10  ◄── Sécurité spires (NC)                │
 │  │  GPIO33 ◄─[pull-up]── borne 11  ◄── Contact poumon plein (NC)           │
 │  │  GPIO25 ◄─[pull-up]── borne 12  ◄── Pressostat (NC)                     │
 │  └────────────────────────────┘                                            │
 │                                                                            │
 │   Bornier DIN 12 voies : [1][2][3][4][5][6] │ [7][8][9][10][11][12]        │
 │                            PUISSANCE        │       SIGNAUX                │
 └────────────────────────────────────────────────────────────────────────────┘
        │    │                                       │
   Batterie 12V                              Câbles capteurs machine
   (panneau solaire)                         (+ retour GND commun → borne 2)
```

---

## Bornier 12 voies — affectation

Principe : **puissance à gauche (1-6), signaux à droite (7-12)** — les transitoires
de commutation des EV (≈ 1 A) ne longent pas les entrées 3,3 V haute impédance.

| Borne | Signal | Câblage côté boîtier | Câblage côté terrain |
|---|---|---|---|
| 1 | 12V+ batterie | → VIN carte QMOS, repiquage → borne 7 | Câble rouge batterie |
| 2 | GND batterie | → GND carte, retour contacts | Câble noir batterie + fil retour commun contacts |
| 3 | EV_CANON + | ← COM relais 1 (via INA3221 CH1) | Paire torsadée vers EV canon |
| 4 | EV_CANON − | → GND | idem (2ᵉ fil de la paire) |
| 5 | EV_POUMON + | ← COM relais 2 (via INA3221 CH2) | Paire torsadée vers EV poumon |
| 6 | EV_POUMON − | → GND | idem |
| 7 | Capteur vitesse alim | ← repiquage 12V borne 1 | Fil alim capteur (souvent brun) |
| 8 | Capteur vitesse signal | → diviseur 10k/3,3k → GPIO 34 | Fil signal (souvent noir) |
| 9 | Fin de course | → pull-up 10k → GPIO 35 | 1 fil du contact NC |
| 10 | Sécurité spires | → pull-up 10k → GPIO 32 | 1 fil du contact NC |
| 11 | Contact poumon plein | → pull-up 10k → GPIO 33 | 1 fil du contact NC |
| 12 | Pressostat | → pull-up 10k → GPIO 25 | 1 fil du contact NC |

> **Retour des contacts** : le 2ᵉ fil de chacun des 4 contacts NC (bornes 9-12) est
> **chaîné en un seul fil de retour** côté machine, qui rentre sur la **borne 2**
> (GND). Les courants sont de l'ordre de 0,3 mA par contact (3,3 V / 10 kΩ) — aucun
> problème de chute de tension sur un retour commun.
> Le GND du capteur vitesse rentre aussi sur la borne 2.

---

## Conditionnement des entrées contacts (bornes 9-12)

Chaque entrée contact NC a le même montage : résistance **10 kΩ soudée sur fil**
entre le 3,3V de la carte et l'entrée GPIO, protégée par gaine thermorétractable.

**Montage retenu** : le 3,3 V de la carte alimente un **bornier de répartition
1→4** ; chaque sortie va à sa résistance 10 kΩ (une par entrée — jamais une
résistance partagée : les contacts NC fermés tireraient le nœud commun à 0 et
rendraient les 4 entrées aveugles). Sur la **patte de sortie** de chaque
résistance, **deux fils soudés** : un vers le GPIO, un vers la borne du bornier
12 voies. Gaine thermo sur l'ensemble résistance + jonction.

```
 3,3V (carte) ──► répartiteur 1→4 ─┬─ fil ─[10 kΩ]═╦═ fil ──► GPIO 35   borne 9
                                   │    (soudure Y) ╚═ fil ──► borne 9
                                   ├─ idem ──► GPIO 32 / borne 10
                                   ├─ idem ──► GPIO 33 / borne 11
                                   └─ idem ──► GPIO 25 / borne 12

 borne 9..12
   │
   ~ ~ ~ câble terrain ~ ~ ~
   │
 contact NC (fermé au repos)
   │
 fil retour commun (chaîné sur les 4 contacts) ──► borne 2 (GND)
```

Cheminement : faire courir les fils résistance→GPIO du côté signaux du boîtier,
à l'écart des fils EV 12 V (c'est le tronçon haute impédance le plus sensible).

**Logique résultante (fail-safe)** :

| Situation | Circuit | GPIO | Interprétation firmware |
|---|---|---|---|
| Normal (contact NC fermé) | tiré à GND | **LOW** | repos / OK |
| Sécurité déclenchée (contact ouvert) | pull-up seul | **HIGH** | danger / actif |
| **Fil coupé / capteur débranché** | pull-up seul | **HIGH** | danger / actif ✅ |
| Pull-up absente | broche flottante | aléatoire | ❌ indéterminé — INTERDIT |

C'est la 4ᵉ ligne qui fait du « fil coupé » l'équivalent exact d'une sécurité
déclenchée — et la 5ᵉ qui rend la résistance obligatoire.

---

## Capteur vitesse (bornes 7-8 → GPIO 34)

Capteur actif 3 fils (NPN open-collector, signal 12 V). GPIO 34 est **input-only**
(pas de sortie, pas de pull interne possible) — le diviseur fait office de
conditionnement complet.

```
 borne 7 (12V) ──► alim capteur
 borne 8 (signal 12V) ──┐
                       [R1 = 10 kΩ]
                        ├──────────► GPIO 34
                       [R2 = 3,3 kΩ]
                        │
                       GND

 V_gpio = 12 × 3300 / 13300 ≈ 2,98 V  (< 3,3 V ✅)
```

Résistances soudées sur fil + gaine thermo, au plus près de la carte.
Anti-rebond : 50 ms dans l'ISR (`DEBOUNCE_VITESSE_MS`) — fronts montants comptés.

---

## Chaîne de puissance EV — MOSFET principal / secours / relais / INA3221

Chaque EV a un chemin **principal** (OUT1/OUT2) et un chemin **secours**
(OUT3/OUT4), sélectionnés par un relais SPDT. L'INA3221 est sur le **fil commun
après le COM du relais** : il mesure toujours le chemin actif.

```
            ┌─ NC ◄── QMOS OUT1 (GPIO 16, principal)
 COM ───────┤
  │         └─ NO ◄── QMOS OUT3 (GPIO 26, secours)
  │
  ▼                       Relais 1 — bobine pilotée par GPIO 2
 INA3221 CH1              (LOW = principal, HIGH = secours)
 (shunt 0,1 Ω)
  │
  ▼
 borne 3 ── EV_CANON + ── EV_CANON − ── borne 4 ── GND
```

Identique pour EV_POUMON : OUT2 (GPIO 17) / OUT4 (GPIO 27), relais 2 (GPIO 4),
INA3221 CH2, bornes 5-6.

**Réglages des modules relais** (à faire avant montage) :
- cavalier sur **HIGH level trigger** ;
- VCC signal = **3,3 V**, JD-VCC bobine = **12 V** (cavalier JD-VCC retiré,
  alimentations séparées).

Au repos (GPIO 2/4 LOW), COM–NC fermé → chemin principal. Le basculement secours
est automatique sur détection de panne MOSFET (voir HARDWARE.md, PR-19).

---

## I2C — INA3221 (mesures EV + batterie)

```
 ESP32 GPIO 21 (SDA) ◄──► INA3221 SDA
 ESP32 GPIO 22 (SCL) ───► INA3221 SCL
 3,3V ──────────────────► INA3221 VCC      A0 → GND (adresse 0x40)
 GND  ──────────────────► INA3221 GND

 CH1 : en série fil EV_CANON  (COM relais 1 → borne 3)   — tension + courant
 CH2 : en série fil EV_POUMON (COM relais 2 → borne 5)   — tension + courant
 CH3 : V_BUS+ sur borne 1 (12V), V_BUS− sur borne 2      — tension batterie
```

Câbles I2C courts (< 20 cm) et éloignés des fils EV. Le module MCU-3221 embarque
déjà ses pull-ups I2C.

> Si l'INA3221 est absent/débranché, l'UI affiche batterie « Inconnue » (gris,
> 0 V) — aucune tension n'est inventée. La surveillance MOSFET est inactive.

---

## Watchdog TPL5010 + heartbeat

Câblage compact (détails, formule de timeout et procédures de test :
[HARDWARE.md](HARDWARE.md#watchdog-matériel-tpl5010ddcr-gpio-13)) :

```
 GPIO 13 ──────► DONE     TPL5010 (SOT-23-6 sur adaptateur)
 3,3V    ──────► VDD
 GND     ──────► GND
 GND ─[3,3 MΩ]─► REXT     (timeout ≈ 5,3 s)
 EN ESP32 ◄───── RESET
```

GPIO 23 (LED verte carte) sert de heartbeat 1 Hz optionnel pour un futur circuit
RC fail-safe (désactivé par défaut, Config → Machine).

---

## Ordre de montage conseillé

1. **Rail DIN** fixé dans le boîtier ; bornier 12 voies + carte QMOS + INA3221 + 2 relais.
2. **Réglages avant câblage** : cavaliers relais (HIGH trigger, JD-VCC séparé), A0
   de l'INA à GND.
3. **Puissance** : bornes 1-2 → carte QMOS ; chaînes EV (OUT → relais → INA → bornes
   3-6) ; vérifier le serrage, ce sont les seuls fils qui portent ≈ 1 A.
4. **Conditionnement signaux** : répartiteur 1→4 sur le 3,3 V, souder les 4
   pull-ups 10 kΩ (deux fils en Y sur la patte de sortie : GPIO + borne) et le
   diviseur 10k/3,3k, gaine thermo, raccorder bornes 7-12.
5. **I2C + watchdog** : INA3221 (21/22), TPL5010 (13/EN).
6. **Contrôles hors tension** (checklist ci-dessous) puis mise sous tension USB
   seule d'abord (12 V débranché), puis 12 V.

---

## Checklist avant mise sous tension

| # | Contrôle | Méthode | Attendu |
|---|---|---|---|
| 1 | Pas de court 12V/GND | Ohmmètre bornes 1-2 | > 1 kΩ (carte QMOS en entrée) |
| 2 | Pull-ups présentes | Ohmmètre GPIO 25/32/33/35 ↔ 3,3V | ≈ 10 kΩ chacune |
| 3 | Diviseur vitesse | Ohmmètre GPIO 34 ↔ GND | ≈ 3,3 kΩ |
| 4 | Relais au repos | Continuité COM ↔ NC | fermé (les 2 relais) |
| 5 | Cavaliers relais | Visuel | HIGH trigger, JD-VCC séparé |
| 6 | Adresse INA | Visuel A0 | A0 → GND (0x40) |
| 7 | Contacts NC au repos | Continuité bornes 9-12 ↔ borne 2, capteurs branchés | fermé (LOW au boot) |
| 8 | Test fil coupé | Débrancher un contact, lire l'UI | sécurité/alarme correspondante déclenchée |

Le contrôle 8 est le test fonctionnel de la règle fail-safe : **chaque entrée
débranchée doit déclencher le même comportement que la sécurité réelle**
(SEC-1 fin de course, SEC-2 débordement, pression absente → PAUSE_PRESSION).
