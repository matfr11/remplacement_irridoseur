# Troubleshooting — Irrifrance ESP32

---

## Problèmes connus et solutions

### L'ESP32 ne démarre pas en OUVERTURE_CANON après `start`

**Symptôme** : bouton DEMARRER cliqué, rien ne se passe.

**Causes possibles** :
1. `cfg_valide = false` — programme non configuré (dose, largeur, buse ou pression = 0)
   → Aller dans Config → Programme → remplir tous les champs
2. `pression_ok = false` — pressostat absent au démarrage
   → Vérifier l'alimentation eau et le branchement pressostat (GPIO 27)
3. `fin_course = true` — canon en position rentrée
   → Dérouler le tuyau manuellement pour libérer le fin de course
4. `s_demarrage_autorise = false` — l'ESP32 attendait déjà un start qui n'est pas venu
   → Cliquer DEMARRER une nouvelle fois (comportement normal après urgence ou stop)

---

### La vitesse affichée est 0 en cours de session

**Symptôme** : session active, bobine qui tourne, mais `vitesse_m_h = 0`.

**Causes** :
1. Capteur vitesse non câblé ou diviseur de tension manquant
   → Vérifier GPIO 34, diviseur 10kΩ/3.3kΩ
2. `max_cycles_si` dépassé — trop de cycles poumon sans impulsion
   → Vérifier le capteur vitesse avec un multimètre (continuité quand pastille en face)
3. Câble capteur coupé → GPIO 34 flottant → impulsions parasites ou absence d'impulsions
   → Mode dégradé A automatique si `cycles_par_tour > 0`

---

### ARRET_URGENCE "Debordement bobine" en permanence

**Symptôme** : la machine entre en urgence dès le démarrage, secu_spires = ACTIF.

**Causes** :
1. Câble capteur spires coupé → GPIO 32 monte via pull-up → HIGH = ACTIF → SEC-2
   → Vérifier continuité câble capteur spires
2. Capteur défaillant (mécaniquement bloqué en position ouverte)
   → Activer le bypass : Config → Machine → Sécurité spires → Bypass ON
   → Un bandeau rouge apparaît pour rappeler que la sécurité est désactivée
3. Débordement bobine réel → réenrouler manuellement quelques spires

---

### ARRET_URGENCE "Fin de course en cours de cycle" inattendu

**Symptôme** : urgence en cours de session alors que la bobine n'est pas terminée.

**Causes** :
1. Câble capteur fin de course coupé → GPIO 35 monte via pull-up → HIGH = ACTIF
   → Vérifier continuité câble capteur fin de course
2. Longueur restante > `fin_course_seuil_m` (défaut 10m) → SEC-1 considère le signal comme anomalie
   → Si le capteur physique déclenche trop tôt : augmenter le seuil dans Config → Machine
3. Capteur collé mécaniquement en position active

**Note** : en fin de bobine normale (longueur restante ≤ seuil), fin_course → ARRET_FINAL (pas d'urgence).

---

### ARRET_URGENCE "Securite longueur - enroule > deroule"

**Symptôme** : urgence avec cette raison en cours de session.

**Causes** :
1. Capteur fin de course défaillant → session qui continue au-delà de la bobine
   → Vérifier le capteur fin de course (GPIO 35)
2. Longueur déployée mal mesurée (trop petite) → la session semble dépasser
   → Vérifier la mesure de déploiement (ETAT_DEROULE) ou la longueur forcée
3. Facteur de correction trop élevé → comptage longueur surestimé
   → Recalibrer via IRRITESTEUR → Étalonner

---

### Bandeau "Dose configuree trop basse" en cours de session

**Symptôme** : bandeau d'alerte orange avec vitesse max et dose corrigée.

**Signification** : le cycle pneumatique (T_remplissage + T_vidange) est trop long pour atteindre la vitesse cible issue de l'abaque. La machine tourne à vitesse maximale sans pause.

**Solutions** :
1. Augmenter la dose dans le programme → vitesse cible diminue → T_attente positif
2. Augmenter la pression eau → T_remplissage diminue → plus de marge
3. Réduire t_vidange_s dans Config → Machine si surestimé
4. Accepter la dose corrigée affichée (la machine fait de son mieux)

---

### T_attente très long → vitesse réelle trop faible

**Symptôme** : vitesse mesurée << vitesse cible, T_attente augmente.

**Causes** :
1. `t_vidange_s` non mesuré (= 0.0) alors que la vidange mécanique prend du temps
   → Mesurer t_vidange terrain, configurer dans Config → Machine
2. `dist_par_cycle_m` surestimé → T_cycle_cible trop long
   → Vérifier `cycles_par_tour` (valeur terrain, 40 sur ST1 Bis)
3. Pression insuffisante → T_attente négatif → alerte `alerte_pression_insuff`
   → Augmenter pression ou réduire dose dans le programme

---

### `alerte_pression_insuff = true` en cours de session

**Symptôme** : bandeau d'alerte "pression insuffisante" dans l'UI, T_attente = 0.

**Signification** : `dist_cycle / vitesse < t_remplissage + t_vidange`
→ La pression d'eau ne permet pas d'atteindre la dose demandée à cette vitesse.

**Solutions** :
1. Augmenter la pression eau réelle (si possible)
2. Réduire la dose dans le programme (vitesse augmente → T_cycle diminue)
3. Réduire la largeur arrosée si cela affecte la vitesse cible

---

### La longueur enroulée dérive de la réalité

**Symptôme** : longueur affichée dans l'UI diffère de la longueur réelle du tuyau enroulé.

**Cause** : facteur_correction ≠ valeur terrain.

**Solution** :
1. Faire une session complète ou partielle (> 50 impulsions)
2. Mesurer la longueur réellement enroulée (compteur mécanique ou ruban)
3. UI → IRRITESTEUR → champ Étalonner → saisir la longueur réelle → cliquer Étalonner
4. Le facteur est calculé et sauvé en NVS automatiquement

---

## Interprétation des logs Serial

Lancer `idf.py -p PORT monitor` pour voir les logs.

```
I (1234) main: === Irrifrance ESP32 — démarrage ===
I (1240) config_nvs: NVS initialisé
I (1245) batterie: ADC1 init OK, seuils: warn=11.5V crit=11.0V
I (1250) gpio_handler: GPIO init OK
I (1255) state_machine: Init — profil=ST1 Bis Ø82-330m abaque=SR 150C
I (1260) webserver: WiFi AP IRRIDOSEUR-A1B2, IP 192.168.4.1
I (1265) telemetry: Démarrage tâche telemetry (500ms)
```

```
W (5000) securites: SEC-2 spires active — IGNOREE (mode degrade spires ON)
```
→ Le capteur spires est en bypass (mode dégradé) — normal si activé volontairement.

```
W (6000) regulation: T_attente négatif (-2.3s) — pression insuffisante
```
→ Pression insuffisante pour la dose demandée.

```
W (7000) state_machine: Rearm physique ignore - debordement actif
```
→ 3-tap poumon tenté alors que le débordement est encore actif.

```
W (8000) state_machine: cmd_resume ignore - debordement toujours actif
```
→ Bouton REPRENDRE cliqué alors que secu_spires est encore HIGH (UI ne devrait pas permettre ça).

```
I (9000) state_machine: cmd_resume - reprise session (enroule=45.2m deroule=285.5m)
```
→ Reprise normale après urgence.

```
E (xxx) ota: Erreur écriture partition OTA : ESP_ERR_FLASH_OP_FAIL
```
→ Problème OTA (voir section OTA ci-dessous).

---

## Codes d'erreur ESP-IDF fréquents

| Code | Signification | Cause probable |
|---|---|---|
| `ESP_ERR_NVS_NOT_FOUND` | Clé NVS absente | Premier démarrage — valeurs par défaut appliquées |
| `ESP_ERR_NVS_INVALID_LENGTH` | Blob NVS taille incorrecte | Migration de structure (champ ajouté) — effacer NVS |
| `ESP_ERR_NO_MEM` | Heap ESP32 insuffisant | JSON_BUF_SIZE trop grand ou trop de clients WS |
| `ESP_ERR_TIMEOUT` | Timeout httpd | Client WebSocket déconnecté brutalement |
| `ESP_ERR_FLASH_OP_FAIL` | Erreur écriture flash | NVS ou OTA — vérifier partition table |

---

## Comment utiliser le mode IRRITESTEUR

L'IRRITESTEUR permet de piloter manuellement les EV et de lire les capteurs hors cycle.

**Conditions** : état VEILLE uniquement (sinon les commandes sont ignorées).

1. Aller dans l'UI → onglet Config → section IRRITESTEUR
2. Boutons EV_CANON ON/OFF et EV_POUMON ON/OFF
3. La section GPIO (onglet Accueil) affiche en temps réel :
   - EV CANON : OUVERTE/FERMEE
   - EV POUMON : OUVERTE/FERMEE
   - Pression : OK/ABSENTE
   - Fin de course : Normal/ACTIF
   - Sécurité spires : (dans IRRITESTEUR) Normal/ACTIF

**Utilisation terrain** :
- Tester le câblage des EV avant première session
- Vérifier que la pression monte bien quand EV_CANON=ON
- Vérifier que le contact poumon fonctionne (EV_POUMON=ON → observer poumon_plein)

---

## Procédure reset NVS complet

**Quand** : config corrompue, migration impossible, "repartir de zéro".

```bash
# Efface toute la flash (NVS + firmware)
idf.py -p PORT erase-flash

# Re-flasher le firmware
idf.py -p PORT flash
```

Après reset NVS, tous les paramètres reprennent les valeurs par défaut :
- `t_vidange_s = 0.0` ⚠️
- `facteur_correction = 1.0`
- `cycles_par_tour = 0.0` ⚠️
- Programmes vides (cfg_valide = false)

Reconfigurer depuis l'UI avant toute utilisation.

---

## Que faire si l'OTA échoue

### Symptôme : upload échoue dans l'UI

1. Vérifier que le fichier uploadé est bien `build/irrifrance-esp32.bin` (pas le bootloader)
2. Vérifier la connexion WiFi (l'upload peut prendre 30-60s)
3. Si erreur dans les logs : `ESP_ERR_OTA_PARTITION_CONFLICT` → flash corrompue → reset complet

### Symptôme : ESP32 redémarre en boucle après OTA

Le bootloader ESP32 tente la nouvelle partition. Si elle ne démarre pas, il revient à l'ancienne
(rollback automatique). Vérifier :
1. Que le binaire est bien pour ESP32 (pas ESP32-S2 ou autre)
2. Que le binaire est pour la même partition table (même `sdkconfig`)

### Procédure OTA manuelle (sans UI)

```bash
idf.py -p PORT flash
```

Ou via `esptool` directement :
```bash
python -m esptool --chip esp32 -p PORT -b 460800 \
  write_flash 0x10000 build/irrifrance-esp32.bin
```

---

## Récupération après reboot inattendu en cours de session

L'ESP32 sauvegarde en NVS :
- La raison d'urgence (namespace `irri_state`, clé `urgence`)
- La longueur enroulée (clé `longueur_m`)
- La longueur déployée (clé `deroule_m`)

Au redémarrage, `state_machine_init()` lit ces valeurs. Si une raison d'urgence est présente,
l'ESP32 démarre en `ETAT_ARRET_URGENCE` avec la raison affichée.

**Actions possibles depuis l'UI** :
- **RESET** → repart de zéro (longueurs effacées)
- **REPRENDRE SESSION** → conserve la position, reprend la session

Pour un reboot sans urgence (coupure de courant propre), les longueurs sont préservées et
l'état repart en VEILLE normalement.
