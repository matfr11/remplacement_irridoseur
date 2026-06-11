# Contribuer — Irrifrance ESP32

---

## Convention de commits

Format : `type(scope): description courte en français`

| Type | Usage |
|---|---|
| `feat` | Nouvelle fonctionnalité |
| `fix` | Correction de bug |
| `test` | Ajout ou modification de tests |
| `docs` | Documentation uniquement |
| `refactor` | Refactoring sans changement de comportement |
| `chore` | Build, CI, dépendances |

Exemples :
```
feat: mesure tension batterie (GPIO 36 ADC1, diviseur R1=100k/R2=27k)
fix: T_attente negatif si t_vidange_s > t_remplissage
docs: PR-12 batterie — SPECS v3 et README mis a jour
refactor: extraire calcul_dist_pulse_m dans calculs_mecanique.c
```

**Règles** :
- Pas d'accents dans le message de commit (compatibilité Git cross-platform)
- Ligne 1 : max 72 chars
- Corps optionnel après une ligne vide si explication nécessaire
- Pas de "fixed", "added" en anglais — utiliser "corrige", "ajoute", etc.

---

## Workflow Git

```
main  ← branche principale, toujours buildable
  └── feat/PR-14-nom-court  ← branche de travail
```

1. Créer une branche depuis `main` :
   ```bash
   git checkout -b feat/PR-14-ma-feature
   ```

2. Développer avec des commits atomiques

3. Build + tests avant PR :
   ```bash
   idf.py build
   cd test/host/build && make && ./tests
   ```

4. Push et créer une Pull Request sur GitHub

5. Merge squash ou merge commit selon taille de la PR

---

## Template PR

Titre : `PR-XX : description courte`

Corps :
```markdown
## Contexte
<!-- Pourquoi ce changement ? Quel problème résout-il ? -->

## Changements
<!-- Liste des modifications par fichier -->
- `main/state_machine.c` : ...
- `main/webui/index.html` : ...

## Tests réalisés
- [ ] `idf.py build` — aucune erreur ni warning
- [ ] Tests PC : `cd test/host/build && make && ./tests` — tous verts
- [ ] Test simulateur : scénario [décrire] vérifié
- [ ] Test terrain : [si applicable]

## Taille firmware
- Avant : X KB (Y% libre)
- Après  : X KB (Y% libre)

## Points d'attention
<!-- Décisions non-évidentes, compromis, TODO restants -->
```

---

## Comment ajouter un profil machine (pas à pas)

Voir [docs/dev/ARCHITECTURE.md](ARCHITECTURE.md#comment-ajouter-un-nouveau-profil-machine).

Résumé :
1. Créer `main/machines/nom.c` avec `machine_profile_t`
2. Déclarer `extern` dans `machines/machines.h`
3. Ajouter au tableau `MACHINES_LISTE[]` dans `machines/machines.c`
4. Ajouter le `.c` dans `main/CMakeLists.txt`
5. Build + test preview vitesse dans l'UI

---

## Comment ajouter un abaque canon (pas à pas)

Voir [docs/dev/ARCHITECTURE.md](ARCHITECTURE.md#comment-ajouter-un-nouvel-abaque-canon).

Résumé :
1. Créer `main/abaques/nom.c` avec `canon_abaque_t` (max 20 lignes)
2. Déclarer `extern` dans `abaques/abaques.h`
3. Ajouter au tableau `ABAQUES_LISTE[]` dans `abaques/abaques.c`
4. Ajouter le `.c` dans `main/CMakeLists.txt`
5. Référencer l'index dans le profil machine (`abaque_idx`)

---

## Comment ajouter un scénario de test

Les tests unitaires sont dans `test/host/`. Chaque module a son fichier de test.

### Ajouter un test dans un fichier existant

```c
// Dans test/host/test_regulation.c par exemple

static void test_mon_nouveau_cas(void) {
    // Arrange
    float dist = 0.35f;
    float v_cible = 8.9f / 3600.0f;  // m/s
    float t_rempl = 4.2f;
    float t_vid = 1.0f;
    bool alerte = false;

    // Act
    float t_att = calcul_t_attente_s(dist, v_cible, t_rempl, t_vid, &alerte);

    // Assert
    TEST_ASSERT_FLOAT_WITHIN(0.01f, attendu, t_att);
    TEST_ASSERT_FALSE(alerte);
}

// Ajouter dans la fonction suite_*() du fichier :
RUN_TEST(test_mon_nouveau_cas);
```

### Ajouter un fichier de test

1. Créer `test/host/test_mon_module.c` (ou `scenarios/scenario_mon_cas.c`)
2. Définir le point d'entrée `void suite_mon_module(void)` qui appelle
   `unity_suite_setup(local_setUp, local_tearDown)` (ou `NULL, NULL`) puis les `RUN_TEST(...)`
3. Ajouter le fichier dans `test/host/CMakeLists.txt` (liste `add_executable`)
4. Déclarer et appeler `suite_mon_module()` dans `test/host/main_test.c`

### Règles issues de la revue 2026-06 (11 bugs passés sous les radars)

- **Pas de module orphelin** : tout `.c` de `main/` compilable sur host doit être
  dans `PROD_SRCS` et avoir sa suite — un mock vide cache les bugs du vrai code
- **Asserter les résultats, pas seulement les transitions** : un scénario qui
  vérifie `ETAT_ARRET_FINAL` sans vérifier volume/dose/NVS laisse passer un bilan à 0
- **Tout paramètre configurable doit prouver un effet** : deux valeurs différentes,
  deux comportements observables différents (cf. `scenario_parametres.c`)
- **Vérifier les sorties physiques** : `ASSERT_EVS(canon, poumon)` aux états clés
- **Mesurer la couverture** avant de déclarer un module testé (voir README, gcovr)

---

## Règles de nommage

### Variables

| Catégorie | Préfixe | Exemple |
|---|---|---|
| Statique module | `s_` | `s_etat`, `s_longueur_enroulee` |
| Statique locale (dans fonction) | `s_` | `s_rearm_count` |
| Paramètre de fonction | aucun | `vitesse_m_h`, `dist_par_cycle_m` |
| Variable locale | aucun | `t_attente`, `etage` |

### Fonctions

Format : `module_verbe_objet()`

```c
gpio_handler_lire_entrees()    // module=gpio_handler, verbe=lire, objet=entrees
state_machine_cmd_resume()     // module=state_machine, verbe=cmd, objet=resume
calcul_rayon_etage()           // module=calcul, verbe=rayon, objet=etage
regulation_update_dist_par_cycle()
batterie_get_status()
config_nvs_sauver_machine()
```

### GPIO et constantes

- GPIO : `PIN_` + nom signal MAJUSCULE : `PIN_FIN_COURSE`, `PIN_SECU_SPIRES`
- Constantes de calcul : MAJUSCULE : `NB_PASTILLES`, `DEBOUNCE_VITESSE_MS`, `CALIB_WINDOW`
- États machine : `ETAT_` préfixe : `ETAT_EN_COURS`, `ETAT_ARRET_URGENCE`
- Sous-états : `SOUS_` préfixe : `SOUS_REMPLISSAGE`
- Clés NVS : minuscule sans accent, max 15 chars : `"t_vidange"`, `"facteur_cor"`

### Fichiers

- Modules C : `snake_case.c` / `snake_case.h`
- Profils machine : `constructeur_modele.c` (ex: `st1bis_82_330.c`)
- Abaques : `reference_constructeur.c` (ex: `sr150c.c`)

---

## Règles de commentaires

- **Langue** : français dans les commentaires (sauf termes techniques anglais inévitables)
- **Format** : une ligne max par commentaire inline ; bloc `/* */` uniquement si vraiment nécessaire
- **Contenu** : expliquer le POURQUOI, pas le QUOI (le code s'explique lui-même)
- **Pas de** : docstrings multi-paragraphes, commentaires de section redondants avec le code
- **Tag ESP_LOG** : `static const char *TAG = "nom_module";` en tête de chaque .c

```c
// ✅ Bon commentaire — explique pourquoi
if (t_attente < 0.0f) {
    // Vitesse trop élevée pour la pression disponible — lever l'alerte
    return 0.0f;
}

// ❌ Mauvais — redondant avec le code
// Retourner 0 si t_attente est négatif
if (t_attente < 0.0f) {
    return 0.0f;
}
```

- **Strings C** : ASCII pur, sans accents (pour compatibilité logs Serial et JSON) :
  `"Debordement bobine"` et non `"Débordement bobine"`

---

## Checklist avant merge

- [ ] `idf.py build` sans warning (les `#warning` GPIO provisoires sont acceptés)
- [ ] Tests PC : `cd test/host/build && make && ./tests` — 0 FAIL
- [ ] Taille firmware vérifiée (objectif : garder > 40% flash libre)
- [ ] CHANGELOG.md mis à jour avec l'entrée PR-XX
- [ ] SPECS_FINAL_v3.md mis à jour si comportement documenté change
- [ ] README.md mis à jour si fonctionnalité visible opérateur change
- [ ] Pas d'accent dans les strings C compilées
- [ ] Pas de `printf` ou `malloc` direct (utiliser `ESP_LOGI` et les allocations stack)
- [ ] `gpio_all_ev_off()` appelé avant tout `state_machine_declencher_urgence()`
- [ ] `regulation_reset_calibration()` appelé à chaque transition vers VEILLE
