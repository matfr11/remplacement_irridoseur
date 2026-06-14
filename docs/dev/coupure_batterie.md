# Protection batterie — périmètre réduit

> Plan issu de l'analyse PROTECTION_BATTERIE.md (2026-06-14).
> Scope limité au Niveau 1 logiciel après identification des blocages hardware.

---

## Ce qui existe déjà (pas de code à écrire)

- Batterie lue toutes les **30 s** (`s_batt_tick >= 300`, 100ms×300 dans `tick_state_machine()`)
- `s_batt_warn_v` / `s_batt_crit_v` chargés depuis NVS, exposés dans l'UI ✅
- `s_batt.etat` expose déjà `BATT_ETAT_FAIBLE` / `BATT_ETAT_CRITIQUE` ✅
- Alerte UI batterie faible/critique déjà affichée ✅

---

## Problèmes identifiés dans le plan original

### 1. GPIO 13 pris par TPL5010

`PIN_TPL5010_DONE = 13` est déjà câblé. La `power_fail_task` avec ISR sur GPIO 13
est **inapplicable** — pas de GPIO libre identifié.

### 2. Faille architecture supercap

Le supercap est prévu sur le rail **5V** (ESP32). Mais les impulsions FERMER passent
par le LM2596 **12V→6V**, alimenté depuis la batterie 12V directement.

Lors d'une coupure brutale :
- ESP32 survit grâce au supercap ✅
- LM2596-EV perd son 12V immédiatement → bobines EV sans alimentation
- **Impulsions FERMER impossibles** ⚠️

Pour corriger : supercap sur le rail 12V (avant les deux LM2596), ce qui implique
un supercap 16V — plus encombrant, ~10€.

**Décision** : la coupure brutale ne peut pas être gérée logiciellement ni avec
ce schéma hardware. On se concentre sur la décharge progressive (cas le plus fréquent).

### 3. API incorrecte dans le plan original

| Plan | Code réel |
|------|-----------|
| `millis()` | `now_ms()` |
| `declencher_arret_urgence()` | `state_machine_declencher_urgence()` |
| `s_cfg_machine.seuil_batt_critique_v` | `s_batt_crit_v` (variable interne) |
| `nvs_sauvegarder_session(&s_session)` | déjà fait par `state_machine_declencher_urgence()` |
| `gpio_set_level(PIN_EV_CANON_FERMER, 1)` | `gpio_ev_canon_set(false)` |

### 4. Deep sleep — hors scope

Condition de réveil non définie, TPL5010 incompatible (plus d'impulsion DONE),
WiFi coupé (opérateur perd la visibilité). Supprimé du scope.

### 5. Urgence restreinte aux états actifs

Sur `ETAT_VEILLE`, `ETAT_ARRET_FINAL`, `ETAT_ARRET_URGENCE` : EVs déjà fermées,
urgence sans objet. La logique se limite aux états où les EVs peuvent être ouvertes :
`OUVERTURE_CANON`, `TEMPO_DEPART`, `REMPLISSAGE_POUMON`, `EN_COURS`,
`PAUSE_PRESSION`, `TEMPO_ARRIVEE`.

---

## Implémentation — Niveau 1 logiciel uniquement

### `main/state_machine.c`

**Nouvelles variables statiques :**
```c
static bool s_batt_alerte_active = false;
```

**Modifier le bloc poll batterie (autour de la ligne 704) :**
```c
// Poll 30s normal, 5s si alerte active (tension < seuil warn)
int batt_periode = s_batt_alerte_active ? 50 : 300;
if (++s_batt_tick >= batt_periode) {
    s_batt_tick = 0;
    s_batt = batterie_get_status();
    s_batt_alerte_active = (s_batt.etat == BATT_ETAT_FAIBLE ||
                             s_batt.etat == BATT_ETAT_CRITIQUE);
}
```

**Ajouter après `securites_watchdog()` (dans la zone mutex) :**
```c
// Batterie critique en session active → fermeture EVs + arrêt d'urgence
if (s_batt.etat == BATT_ETAT_CRITIQUE && s_batt.voltage_v > 0.0f) {
    static const etat_machine_t etats_actifs[] = {
        ETAT_OUVERTURE_CANON, ETAT_TEMPO_DEPART, ETAT_REMPLISSAGE_POUMON,
        ETAT_EN_COURS, ETAT_PAUSE_PRESSION, ETAT_TEMPO_ARRIVEE,
    };
    for (int i = 0; i < 6; i++) {
        if (s_etat == etats_actifs[i]) {
            char msg[48];
            snprintf(msg, sizeof(msg),
                     "Batterie critique (%.1fV)", s_batt.voltage_v);
            state_machine_declencher_urgence(msg);
            break;
        }
    }
}
```

### Tests — `test/host/scenarios/scenario_batterie.c` (nouveau fichier)

**Scénario 17 — Batterie faible :**
```
mock_ina3221_set_canal(3, 11.4f, 0)
Avancer 50 ticks (5s poll accéléré)
→ s_batt.etat == BATT_ETAT_FAIBLE
→ cycle continue (ETAT_EN_COURS)
→ s_batt_alerte_active == true (poll passe à 5s)
```

**Scénario 18 — Batterie critique :**
```
mock_ina3221_set_canal(3, 10.9f, 0)
Avancer 50 ticks (5s poll accéléré)
→ ETAT_ARRET_URGENCE
→ gpio_ev_canon_get() == false
→ gpio_ev_poumon_get() == false
→ raison_arret contient "Batterie critique"
```

---

## Niveau 2 hardware — documentation uniquement

Schéma à documenter dans `docs/dev/HARDWARE.md`, aucun code firmware.

**Condition pour que ça fonctionne :** supercap sur le rail **12V** (avant LM2596),
supercap 16V 1F (~8€), diode Schottky 1N5819 en série.

| Composant | Valeur | Prix |
|-----------|--------|------|
| Supercondensateur | 1F **16V** | ~8€ |
| Diode Schottky | 1N5819 | 0.05€ |

---

## Fichiers à modifier

| Fichier | Changement |
|---------|-----------|
| `main/state_machine.c` | Poll 5s/30s + urgence batterie critique |
| `test/host/scenarios/scenario_batterie.c` | Nouveau — scénarios 17 & 18 |
| `test/host/main_test.c` | Enregistrer `suite_scenario_batterie` |
| `test/host/CMakeLists.txt` | Ajouter `scenario_batterie.c` |
| `CHANGELOG.md` | Entrée `feat/protection-batterie` |

---

## Ce qui n'est PAS dans ce scope

- Deep sleep
- power_fail_task / ISR GPIO (pas de GPIO libre)
- Schéma supercap dans HARDWARE.md (à faire séparément)
- Modifications UI (alerte batterie déjà affichée)

---

*coupure_batterie.md — Irrifrance ESP32*
*Niveau 1 logiciel uniquement — décharge progressive via INA3221*
*Niveau 2 hardware bloqué par absence de GPIO libre et architecture supercap à revoir*
