# Schéma de câblage — Irrifrance ESP32

Document de référence pour le **montage complet** du boîtier : synoptique général,
bornier, conditionnement des entrées, chaîne de puissance EV, I2C, et checklist de
mise sous tension. Pour le détail composant par composant (INA3221, TPL5010,
points de test), voir [HARDWARE.md](HARDWARE.md).

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
 │  │  eletechsup ES30G29        │   I2C   │  INA3221 0x40   │                │
 │  │                            ├─SDA 21──┤  CH3 Batterie   │                │
 │  │  12V IN ◄─[F1]─ borne 1/2 ├─SCL 22──┤                 │                │
 │  │                            │         └─────────────────┘                │
 │  │  GPIO 18 ──────────────────┐                                            │
 │  │  GPIO 19 ──────────────────┤──► Module MOSFET 4CH ──► bornes 3-6       │
 │  │  GPIO 14 ──────────────────┤    DC+ ◄─[F2]─ LM2596 6V                  │
 │  │  GPIO  4 ──────────────────┘    (bobines EV OUVRIR/FERMER)              │
 │  │  GPIO23 ─► TPL5010 DONE    ├─────────┐                                 │
 │  │  EN     ◄─ TPL5010 RESET   │  ┌──────┴──────────┐                      │
 │  │  GPIO 2 ─► LED heartbeat   │  │ TPL5010 watchdog│                      │
 │  │                            │  │ Rext 3,3MΩ→GND  │                      │
 │  │  GPIO34 ◄─[diviseur]─ borne 8   ◄── Capteur vitesse (signal 2V/8V)     │
 │  │  GPIO35 ◄─[pull-up]── borne 9   ◄── Fin de course (NC)                 │
 │  │  GPIO32 ◄─[pull-up]── borne 10  ◄── Sécurité spires (NC)               │
 │  │  GPIO33 ◄─[pull-up]── borne 11  ◄── Contact poumon plein (NC)          │
 │  │  GPIO25 ◄─[pull-up]── borne 12  ◄── Pressostat (NC)                    │
 │  └────────────────────────────┘                                            │
 │                                                                            │
 │   Bornier DIN 12 voies : [1][2][3][4][5][6] │ [7][8][9][10][11][12]       │
 │                            PUISSANCE        │       SIGNAUX                │
 └────────────────────────────────────────────────────────────────────────────┘
        │    │                                       │
   Batterie 12V                              Câbles capteurs machine
   (panneau solaire)                         (+ retour GND commun → borne 2)
```

---

## Bornier 12 voies — affectation

Principe : **puissance à gauche (1-6), signaux à droite (7-12)** — les transitoires
de commutation des EV (≈ 800 mA sur 100 ms) ne longent pas les entrées 3,3 V haute
impédance.

| Borne | Signal | Câblage côté boîtier | Câblage côté terrain |
|---|---|---|---|
| 1 | 12V+ batterie | → VIN ES30G29, → VIN LM2596 (après F1), repiquage → borne 7 | Câble rouge batterie · **F1 3A ATO** sur ce fil |
| 2 | GND batterie | → GND carte, GND LM2596, retour EV et contacts | Câble noir batterie + fil retour commun |
| 3 | EV_CANON OUVRIR | ← 6V via Module MOSFET OUT1 (GPIO 18) | Fil bobine ouverture EV canon · diode D1 sur borne |
| 4 | EV_CANON FERMER | ← 6V via Module MOSFET OUT3 (GPIO 14) | Fil bobine fermeture EV canon · diode D3 sur borne |
| 5 | EV_POUMON OUVRIR | ← 6V via Module MOSFET OUT2 (GPIO 19) | Fil bobine ouverture EV poumon · diode D2 sur borne |
| 6 | EV_POUMON FERMER | ← 6V via Module MOSFET OUT4 (GPIO 4) | Fil bobine fermeture EV poumon · diode D4 sur borne |
| 7 | Capteur vitesse alim | ← repiquage 12V borne 1 | Fil alimentation capteur |
| 8 | Capteur vitesse signal | → diviseur 10k/5,6k → GPIO 34 | Fil signal capteur (2V/8V) |
| 9 | Fin de course | → pull-up 10k → GPIO 35 | 1 fil du contact NC |
| 10 | Sécurité spires | → pull-up 10k → GPIO 32 | 1 fil du contact NC |
| 11 | Contact poumon plein | → pull-up 10k → GPIO 33 | 1 fil du contact NC |
| 12 | Pressostat | → pull-up 10k → GPIO 25 | 1 fil du contact **NO** |

> **Retour des EV** : le fil commun (GND) des deux EVs (bornes 3-6, côté bobine −)
> rentre sur la **borne 2** (GND commun). Chaque EV a donc 2 fils actifs (OUVRIR et
> FERMER) + 1 fil retour commun.
>
> **Retour des contacts** : le 2ᵉ fil de chacun des contacts NC (bornes 9-11) et du
> pressostat NO (borne 12) est **chaîné en un seul fil de retour** côté machine, qui
> rentre sur la **borne 2**.
> Les courants sont de l'ordre de 0,3 mA par contact (3,3 V / 10 kΩ) — aucun
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

Contacts NC — bornes 9, 10, 11 (fin de course, spires, poumon) :

| Situation | Circuit | GPIO | Interprétation firmware |
|---|---|---|---|
| Normal (contact NC fermé) | tiré à GND | **LOW** | repos / OK |
| Sécurité déclenchée (contact ouvert) | pull-up seul | **HIGH** | danger / actif |
| **Fil coupé / capteur débranché** | pull-up seul | **HIGH** | danger / actif ✅ |
| Pull-up absente | broche flottante | aléatoire | ❌ indéterminé — INTERDIT |

Pressostat NO — borne 12 (GPIO 25) :

| Situation | Circuit | GPIO | Interprétation firmware |
|---|---|---|---|
| Normal (pression présente, contact NO fermé) | tiré à GND | **LOW** | pression OK |
| Pression absente (contact ouvert) | pull-up seul | **HIGH** | pas de pression |
| **Fil coupé / capteur débranché** | pull-up seul | **HIGH** | pas de pression ✅ |
| Pull-up absente | broche flottante | aléatoire | ❌ indéterminé — INTERDIT |

Dans les deux cas, un fil coupé équivaut à la sécurité déclenchée — la pull-up 10 kΩ
est obligatoire. Le câblage interne (répartiteur → résistance → GPIO + borne) est
identique pour les 4 entrées ; seul le comportement physique du contact diffère.

---

## Capteur vitesse (bornes 7-8 → GPIO 34)

Capteur **2 fils** (alimentation + signal) : sans pastille → 2 V au bornier, avec
pastille → 8 V. GPIO 34 est **input-only** (pas de sortie, pas de pull interne
possible) — le diviseur fait office de conditionnement complet.

```
 borne 7 (12V) ──► alim capteur
 borne 8 (signal 2V/8V) ──┐
                          [R1 = 10 kΩ]
                           ├──────────► GPIO 34
                          [R2 = 5,6 kΩ]
                           │
                          GND

 V_gpio (HIGH, 8V) = 8 × 5600 / 15600 ≈ 2,87 V  (> seuil HIGH 2,31 V ✅)
 V_gpio (LOW,  2V) = 2 × 5600 / 15600 ≈ 0,72 V  (< seuil LOW  0,80 V ✅)
```

> ⚠️ **Modification matérielle obligatoire** : remplacer l'ancienne résistance
> R2 = 3,3 kΩ par **5,6 kΩ**. Avec 3,3 kΩ, la tension HIGH (8V) ne dépasse pas
> 2,00 V — sous le seuil de détection de l'ESP32 (2,31 V).

Résistances soudées sur fil + gaine thermo, au plus près de la carte.
Anti-rebond : 50 ms dans l'ISR (`DEBOUNCE_VITESSE_MS`) — fronts montants comptés.

---

## Chaîne de puissance EV — bistables à impulsion 6 V / 5 W

Les EVs sont **bistables** : une brève impulsion électrique (100 ms) suffit pour
changer d'état (ouvrir ou fermer). La vanne mémorise sa position mécaniquement,
sans courant permanent. Deux bobines distinctes par EV : une pour OUVRIR, une pour
FERMER.

**Abaissement tension** : un module **LM2596** abaisse le 12 V batterie à **6 V**
pour alimenter les 4 sorties MOSFET (courant de crête ≈ 830 mA × 100 ms par
impulsion).

```
 Batterie 12V ──[F1 3A]──► borne 1 ──► LM2596 ──[F2 2A]──► DC+ Module MOSFET (6V)

 MOSFET OUT1 (GPIO 18) ──► borne 3 ──► bobine OUVRIR  EV_CANON
 MOSFET OUT3 (GPIO 14) ──► borne 4 ──► bobine FERMER  EV_CANON
 MOSFET OUT2 (GPIO 19) ──► borne 5 ──► bobine OUVRIR  EV_POUMON
 MOSFET OUT4 (GPIO  4) ──► borne 6 ──► bobine FERMER  EV_POUMON

 Retour commun bobines ──► borne 2 (GND)
 Diodes D1..D4 (1N4007) : cathode sur bornes 3-6, anode sur borne 2
```

**Fonctionnement firmware** :
- Au boot : impulsion FERMER simultanée sur les 2 EVs → état connu dès le départ.
- Ouverture : impulsion 100 ms sur la bobine OUVRIR uniquement ; l'état est mémorisé
  en RAM (`s_ev_canon_ouverte`, `s_ev_poumon_ouverte`).
- Fermeture d'urgence (`gpio_all_ev_off`) : les deux bobines FERMER pulsent
  simultanément (100 ms total).
- Commande redondante ignorée : si l'EV est déjà dans l'état voulu, aucune impulsion
  n'est envoyée.

**Comportement au reset watchdog** : après un reset TPL5010, les EVs restent dans
leur état mécanique pendant ~7 s (5,3 s timeout watchdog + ~2 s boot ESP32). Le
boot envoie ensuite les impulsions FERMER. Ce délai est accepté comme comportement
de sécurité dégradé.

> L'INA3221 n'est **pas** connecté aux EVs bistables : sans courant permanent,
> il n'y a rien à mesurer. La surveillance EV est supprimée.

---

## I2C — INA3221 (batterie uniquement)

```
 ESP32 GPIO 21 (SDA) ◄──► INA3221 SDA
 ESP32 GPIO 22 (SCL) ───► INA3221 SCL
 3,3V ──────────────────► INA3221 VCC      A0 → GND (adresse 0x40)
 GND  ──────────────────► INA3221 GND

 CH3 : V_BUS+ sur borne 1 (12V), V_BUS− sur borne 2 (GND) — tension batterie
```

Câbles I2C courts (< 20 cm) et éloignés des fils EV. Le module MCU-3221 embarque
déjà ses pull-ups I2C.

> Si l'INA3221 est absent/débranché, l'UI affiche batterie « Inconnue » (gris,
> 0 V) — aucune tension n'est inventée.

---

## Watchdog TPL5010 + heartbeat

Câblage compact (détails, formule de timeout et procédures de test :
[HARDWARE.md](HARDWARE.md#watchdog-matériel-tpl5010ddcr-gpio-23)) :

```
 GPIO 23 ──────► DONE     TPL5010 (SOT-23-6 sur adaptateur)
 3,3V    ──────► VDD
 GND     ──────► GND
 GND ─[3,3 MΩ]─► REXT     (timeout ≈ 5,3 s)
 EN ESP32 ◄───── RESET
```

GPIO 2 (LED bleue carte) sert de heartbeat 1 Hz optionnel pour un futur circuit
RC fail-safe (désactivé par défaut, Config → Machine).

---

## Ordre de montage conseillé

1. **Rail DIN** fixé dans le boîtier ; bornier 12 voies + carte ES30G29 + Module MOSFET +
   INA3221 + module LM2596.
2. **Réglages avant câblage** : ajuster le LM2596 à 6 V (multimètre, potentiomètre) ;
   vérifier A0 de l'INA à GND.
3. **Fusibles** : installer les porte-fusibles ATO inline — F1 3A sur le câble 12V+
   (entre batterie et borne 1), F2 2A sur le fil 6V (entre LM2596 OUT+ et MOSFET DC+).
4. **Puissance** : bornes 1-2 → ES30G29 et LM2596 ; fil 6V LM2596 → MOSFET DC+ (via F2) ;
   chaînes EV (MOSFET OUT1-4 → bornes 3-6) ; souder diodes D1..D4 sur bornier.
   Vérifier le serrage, ce sont les seuls fils qui portent ≈ 830 mA.
5. **Conditionnement signaux** : répartiteur 1→4 sur le 3,3 V, souder les 4
   pull-ups 10 kΩ (deux fils en Y sur la patte de sortie : GPIO + borne) et le
   diviseur 10k/5,6k, gaine thermo, raccorder bornes 7-12.
6. **I2C + watchdog** : INA3221 (21/22), TPL5010 (23/EN).
7. **Contrôles hors tension** (checklist ci-dessous) puis mise sous tension USB
   seule d'abord (12 V débranché), puis 12 V.

---

## Checklist avant mise sous tension

| # | Contrôle | Méthode | Attendu |
|---|---|---|---|
| 1 | Pas de court 12V/GND | Ohmmètre bornes 1-2 | > 1 kΩ (carte QMOS en entrée) |
| 2 | Pull-ups présentes | Ohmmètre GPIO 25/32/33/35 ↔ 3,3V | ≈ 10 kΩ chacune |
| 3 | Diviseur vitesse | Ohmmètre GPIO 34 ↔ GND | ≈ 5,6 kΩ |
| 4 | LM2596 réglé à 6V | Multimètre V_out LM2596 (12V branché) | 5,8 V – 6,2 V |
| 5 | Adresse INA | Visuel A0 | A0 → GND (0x40) |
| 6a | Contacts NC au repos | Continuité bornes 9-11 ↔ borne 2, capteurs branchés | fermé (LOW au boot) |
| 6b | Pressostat NO (borne 12) | Continuité borne 12 ↔ borne 2, pression présente | fermé (LOW) ; ouvert (HIGH) sans pression |
| 7 | Test fil coupé | Débrancher un contact, lire l'UI | sécurité/alarme correspondante déclenchée |
| 8 | Fusibles F1 et F2 en place | Visuel — porte-fusibles ATO sur câble 12V+ (F1) et fil 6V (F2) | Fusible présent (jamais de cavalier) |
| 9 | Test impulsion EV | Lancer un arrosage court, écouter les EVs | claquement mécanique à l'ouverture et à la fermeture |

Le contrôle 7 est le test fonctionnel de la règle fail-safe : **chaque entrée
débranchée doit déclencher le même comportement que la sécurité réelle**
(SEC-1 fin de course, SEC-2 débordement, pression absente → PAUSE_PRESSION).

Le contrôle 8 confirme que les impulsions 6 V / 100 ms activent mécaniquement les
EVs bistables — un claquement distinct doit être audible à l'ouverture et à la
fermeture de chaque vanne.
