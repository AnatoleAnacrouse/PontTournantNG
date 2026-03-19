# Projet de Pont Tournant Ferroviaire Nouvelle Génération

## 📖 Description

Ce projet permet de gérer un **pont tournant ferroviaire** avec un **Arduino**. Il s'agit d'une solution de contrôle complet et modulaire pour automatiser le positionnement d'un pont tournant destiné à une maquette ferroviaire.

Le pont tournant permet de faire pivoter une voie pour aligner une locomotive avec une voie d'entrée/sortie et les différentes voies de garage. Le système gère automatiquement l'optimisation des trajets, la sauvegarde de l'état en EEPROM, et propose plusieurs modes d'opération.

### Caractéristiques principales
- 🎯 **40 voies** configurables et calibrables
- 🏠 **Fonction de homing** pour retrouver la position de référence
- 🔄 **Optimisation des trajets** (chemin le plus court)
- 💾 **Sauvegarde en EEPROM** de la configuration et de l'état
- 🎮 **Interface LCD 20x4** avec clavier 4x4 intégré
- 🔧 **Mode maintenance** et **mode calibration**
- ⚙️ **Diagnostic au démarrage**

---

## 🛠️ Matériel Requis

| Composant | Spécifications | Broche Arduino |
|-----------|-----------------|-----------------|
| **Arduino** | Uno / Nano / Mega | - |
| **Pont tournant** | JOUEF, 40 voies max, 9° entre voies | - |
| **Moteur pas-à-pas** | NEMA 14, 200 pas/rotation, réduction 8:1 (400 pas/tour) | DIR: D11, STEP: D12 |
| **Driver moteur** | A4988 | DIR: D11, STEP: D12 |
| **Afficheur LCD** | 20x4, I2C, adresse 0x27 | SDA: A4, SCL: A5 |
| **Clavier** | 4x4 matriciel | Lignes: D9,D8,D7,D6; Colonnes: D5,D4,D3,D2 |
| **Capteur Hall** | Pour homing | A0 (pull-up interne) |
| **Buzzer** | Confirmation audio | D13 |
| **EEPROM** | Sauvegarde configuration | Intégrée à l'Arduino |

### Schéma de connexion
```
Arduino Pin Layout:
D2-D5: Colonnes du clavier (C0-C3)
D6-D9: Lignes du clavier (R0-R3)
D11:   DIR du moteur (A4988)
D12:   STEP du moteur (A4988)
D13:   Buzzer
A0:    Capteur Hall (avec pull-up interne)
A4:    SDA (LCD I2C)
A5:    SCL (LCD I2C)
```

---

## 📋 Exigences et Contraintes Système

### Contraintes physiques
- Le pont **n'est pas symétrique** (présence d'une cabine de pilotage)
- L'entrée de la locomotive se fait toujours **à l'opposé de la cabine**
- **Retournement possible** : pivot de 180° nécessaire sur certaines manœuvres
- **Optimisation requise** : chemin le plus court (sens horaire ou anti-horaire)

### Contraintes logiques
- La **voie d'entrée** (voie 0) est le point de référence (position zéro)
- **Fonction de homing** appelée systématiquement à l'initialisation
- **Sauvegarde en EEPROM** de la configuration et de l'état courant
- **Magic byte** (0xA5) pour vérifier l'intégrité des données en EEPROM

### Configuration du système
- Maximum **40 voies** (actuellement limité par la structure du code)
- Chaque voie espacée de **10 pas** (400 pas / 40 voies)
- **Limites EEPROM** : ~100 000 écritures ; optimisation des writes en cas de non-changement

---

## ⚙️ Limitation Actuelle et Extensions Futures

### Limitation : 40 voies maximum

Actuellement, le logiciel est configuré pour gérer **40 voies maximum**.

### Pour dépasser cette limite :

**1. Modifications logicielles :**
- Modifier la constante `NB_MAX_VOIE` (ligne 116)
- Adapter le tableau `tabVoie[]` (ligne 118)
- Mettre à jour les messages LCD (ex: "Voie (1-40)")
- Modifier la logique de saisie dans `saisirNumeroVoie()`
- Recalculer l'espacement entre voies

**2. Modifications matérielles :**
- Pour 80 voies : espacement de 5 pas (400 / 80) au lieu de 10
- Moteur plus précis ou réduction différente
- EEPROM supplémentaire si nécessaire

**3. Exemple pour 80 voies :**
```cpp
#define NB_MAX_VOIE 80
// Chaque voie = 5 pas (400 / 80)
```

---

## 🎮 Modes de Fonctionnement

### 1️⃣ Mode Entrée (Manœuvre vers le garage)

Amener une locomotive de la voie principale vers une voie de garage.

**Séquence :**
1. Positionnement initial → Voie d'entrée (voie 0) 
2. Saisie de la destination (voie 1-40)
3. Choix du retournement (Oui/Non)
4. Déplacement optimisé vers la voie cible
5. Confirmation finale

```
Voie d'entrée (0) ──→ Voie cible (1-40)
```

---

### 2️⃣ Mode Sortie (Manœuvre vers la voie principale)

Sortir une locomotive d'une voie de garage vers la voie principale.

**Séquence :**
1. Saisie de la voie source (voie 1-40)
2. Déplacement vers la voie source
3. Choix du retournement (Oui/Non)
4. Retour à la voie d'entrée (voie 0)
5. Confirmation finale

```
Voie source (1-40) ──→ Voie d'entrée (0)
```

---

### 3️⃣ Mode Maintenance

Déplacement **manuel pas-à-pas** du pont tournant pour tests et diagnostics.

**Commandes :**
- `U` : +10 pas
- `D` : -10 pas
- `R` : +1 pas
- `L` : -1 pas
- `E` : Quitter

---

### 4️⃣ Mode Calibration

Ajustement **précis** de la position de chaque voie et sauvegarde en EEPROM.

**Commandes :**
- `U` : +1 offset
- `D` : -1 offset
- `R` : Voie suivante
- `L` : Voie précédente
- `V` : Valider et sauvegarder
- `E` : Quitter

---

## 🔑 Interface Utilisateur - Clavier 4x4

### Disposition du clavier
```
┌─────┬─────┬─────┬─────┐
│  1  │  2  │  3  │  U  │  U=Up (avancer)
├─────┼─────┼─────┼─────┤
│  4  │  5  │  6  │  D  │  D=Down (reculer)
├─────┼─────┼─────┼─────┤
│  7  │  8  │  9  │  E  │  E=Echap (annuler)
├─────┼─────┼─────┼─────┤
│  L  │  0  │  R  │  V  │  L=Left, R=Right, V=Valider
└─────┴─────┴─────┴─────┘
```

### Navigation principale
| Touche | Action |
|--------|--------|
| **U/D** | Naviguer dans le menu |
| **V** | Valider sélection |
| **E** | Annuler / Quitter |

### Saisie numérique (Voies 1-40)
| Touche | Action |
|--------|--------|
| **0-9** | Saisir le numéro |
| **L** | Effacer le dernier chiffre |
| **V** | Valider la saisie |
| **E** | Annuler |

### Sélection Oui/Non (Retournement)
| Touche | Action |
|--------|--------|
| **U/D** | Basculer Non ↔ Oui |
| **V** | Valider le choix |
| **E** | Annuler |

---

## 🚀 Procédures de Démarrage

### 1. Initialisation au Power-on

```
1. Affichage: "Pont Tournant V0.1"
2. Chargement EEPROM (avec validation)
3. Diagnostic : position, voie courante, capteur Hall
4. Proposition de homing
5. Menu principal
```

### 2. Homing (Recherche de la position zéro)

La fonction `homing()` :
- Utilise le **capteur Hall** pour trouver la voie d'entrée
- Effectue jusqu'à **3 révolutions** pour garantir la détection
- Réinitialise la position du moteur à **0 pas**
- Définit `voieCourante = 0`
- Sauvegarde en EEPROM

**Vitesses réduites pendant le homing :**
- Speed: 50 pas/s (vs 700 normal)
- Acceleration: 100 (vs 200 normal)

---

## ⚡ Algorithmes Clés

### 📍 Optimisation du trajet (Plus court chemin)

Fonction : `calculerPlusCourtChemin(posActuelle, posCible)`

```
Distance directe = posCible - posActuelle

SI |Distance| > 200 pas (demi-tour) ALORS
   SI Distance > 0 ALORS
      Distance -= 400  // Aller "à rebours"
   SINON
      Distance += 400  // Aller "à l'endroit"
   FIN SI
FIN SI

Retourner Distance optimisée
```

**Exemple :**
- Position actuelle: 10 pas
- Position cible: 350 pas
- Distance directe: +340 pas
- Optimisée: -60 pas (chemin plus court!)

---

### 🔄 Normalisation de position

Fonction : `normaliserPosition()`

Ramène la position du moteur dans l'intervalle **[0..399]** en utilisant le modulo :

```cpp
position = position % 400
if (position < 0) position += 400
```

Cela évite les débordements lors de manœuvres répétées.

---

### 💾 Gestion EEPROM

**Adresses EEPROM :**
```cpp
0x00 - 0xA3    : tabVoie[] (41 × 4 bytes = 164 bytes)
0xA4 - 0xA7    : voieCourante (4 bytes)
0xA8           : Magic byte (0xA5)
```

**Stratégie :**
- Vérification du magic byte au démarrage
- Validation des données (0 ≤ pos ≤ 400)
- Réinitialisation si corrompues
- Write optimisé : écriture seulement si changement

---

## 🔧 Configuration Logicielle

### Constantes de vitesse et accélération

```cpp
#define SPEED_NORMAL     700    // pas/s en mode normal
#define ACCEL_NORMAL     200    // accélération normal
#define SPEED_HOMING      50    // pas/s en mode homing
#define ACCEL_HOMING     100    // accélération homing
```

### Timeouts d'affichage

```cpp
#define TIMEOUT_MSG        1500  // Durée messages courts (ms)
#define TIMEOUT_ERREUR      800  // Durée affichage erreurs (ms)
#define TIMEOUT_SAUVEGARDE  800  // Durée confirmation sauvegarde (ms)
```

---

## 📦 Structure du Code

```
PontTournantNG.ino
├── Configuration générale
│   ├── Librairies
│   ├── Constantes moteur, affichage
│   └── Pins Arduino
├── Initialisation
│   ├── EEPROM (charger/sauvegarder)
│   ├── LCD et clavier
│   └── Moteur pas-à-pas
├── Fonctions utilitaires
│   ├── beep()
│   ├── lcdClearLine()
│   ├── afficherProgression()
│   └── normaliserPosition()
├── Logique de déplacement
│   ├── calculerPlusCourtChemin()
│   ├── pontTournantAllerA()
│   └── deplacerPT()
├── Interface utilisateur
│   ├── saisirTypeManoeuvre()
│   ├── saisirNumeroVoie()
│   ├── demanderRetournement()
│   └── Diagnostic()
├── Modes d'opération
│   ├── modeMaintenance()
│   ├── modeCalibration()
│   ├── homing()
│   └── proposerHoming()
└── Boucle principale
    └── loop()
```

---

## 🐛 Codes d'Erreur et Diagnostics

### Affichages possibles

| Affichage | Signification | Action |
|-----------|--------------|--------|
| `Init EEPROM...` | Première initialisation | Valeurs par défaut chargées |
| `EEPROM invalide !` | EEPROM corrompue | Valeurs par défaut restaurées |
| `Homing capteur Hall` | Recherche position zéro | En cours... |
| `Origine OK` | Homing réussi | Pont à la position 0 |
| `ECHEC : origine NOK` | Capteur Hall non détecté | Vérifier le capteur |
| `Erreur: 1 a 40` | Numéro de voie invalide | Saisie rejetée |
| `Rotation...` | Déplacement en cours | Barre de progression affichée |
| `Manoeuvre OK` | Opération réussie | Retour au menu principal |
| `Sauvegarde OK` | Calibration sauvegardée | Confirmation audio |

### Mode Diagnostic (touche V au démarrage)

Affiche :
- Position courante du moteur
- Numéro de voie courant
- État du capteur Hall (ACTIF ou Libre)
- Version du logiciel

---

## 🚦 Procédures Typiques

### Manœuvre d'entrée (Garer une loco)

```
1. Menu principal → Sélectionner "Entree"
2. Pont se déplace vers voie d'entrée (0)
3. Locomotive monte sur le pont
4. Saisir la voie destination (1-40)
5. Choisir retournement : Oui ou Non
6. Pont se déplace vers destination
7. Confirmation "Manoeuvre OK"
```

### Manœuvre de sortie (Sortir une loco)

```
1. Menu principal → Sélectionner "Sortie"
2. Saisir la voie d'où extraire (1-40)
3. Pont se déplace vers cette voie
4. Locomotive monte sur le pont
5. Choisir retournement : Oui ou Non
6. Pont retourne à voie d'entrée (0)
7. Confirmation "Manoeuvre OK"
```

### Calibration d'une voie

```
1. Menu principal → Sélectionner "Calibration"
2. Affichage de la voie courante et son offset
3. Ajuster l'offset :
   - U : +1 pas
   - D : -1 pas
4. Naviguer vers autres voies :
   - R : Voie suivante
   - L : Voie précédente
5. V : Sauvegarder en EEPROM
6. E : Quitter mode calibration
```

---

## 📊 Performances et Spécifications

| Paramètre | Valeur |
|-----------|--------|
| **Résolution moteur** | 400 pas/tour (10 pas/voie) |
| **Nombre de voies** | 40 (extensible) |
| **Vitesse normale** | 700 pas/s (1,75 tour/s) |
| **Vitesse homing** | 50 pas/s (0,125 tour/s) |
| **Temps de trajet max** | ~1,2 secondes (400 pas) |
| **Écritures EEPROM** | Limitées à ~100 000 |
| **Refresh LCD** | ~60 ms par frame |
| **Timeout inactivité LCD** | 1500 ms pour messages |

---

## 🔌 Installation et Compilation

### Prérequis
- Arduino IDE 1.8.0+
- Bibliothèques Arduino :
  - `Wire.h` (I2C)
  - `LiquidCrystal_I2C.h` (LCD)
  - `Keypad.h` (clavier matriciel)
  - `AccelStepper.h` (moteur pas-à-pas)
  - `EEPROM.h` (mémoire)

### Installation des librairies
```
Arduino IDE → Sketch → Include Library → Manage Libraries
Rechercher et installer :
- LiquidCrystal_I2C (by Frank de Brabander)
- Keypad (by Mark Stanley)
- AccelStepper (by Mike McCauley)
```

### Compilation et upload
```
1. Ouvrir PontTournantNG.ino dans Arduino IDE
2. Sélectionner la carte : Tools → Board
3. Sélectionner le port : Tools → Port
4. Télécharger : Sketch → Upload
```

---

## 📝 Notes Importantes

### ⚠️ Risques EEPROM
- **100 000 écritures maximum** par adresse
- Le code optimise en écrivant **uniquement si changement**
- Éviter les écritures inutiles en mode calibration

### ⚠️ Dissipation thermique
- Driver A4988 et moteur peuvent **chauffer**
- **Régler le potentiomètre de courant** du driver A4988
- Assurer une bonne **ventilation**

### ⚠️ Capteur Hall
- Pull-up **interne** activé (pas de résistance externe nécessaire)
- Aimant doit passer **régulièrement** près du capteur
- Homing **obligatoire** au démarrage

---

## 🤝 Auteurs

- **M. EPARDEAU** - Conception
- **F. FRANKE** - Développement

---

## 📄 Licence

À définir par les auteurs

---

## 🔗 Ressources Supplémentaires

- [Documentation Arduino](https://www.arduino.cc/reference/)
- [AccelStepper Library](http://www.airspayce.com/mikem/arduino/AccelStepper/)
- [LiquidCrystal I2C](https://github.com/frank-zhu/LiquidCrystal_I2C)
- [Keypad Library](https://github.com/Chris--A/Keypad)

---

## 📞 Support et Bugs

Pour signaler des bugs ou proposer des améliorations, ouvrir une issue sur le repository GitHub.