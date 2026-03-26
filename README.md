# Projet de Pont Tournant Ferroviaire Nouvelle Génération

## Objectifs du projet

Ce projet a pour objectif de contrôler une maquette de **pont tournant ferroviaire** automatisé et motorisé.
Il permet de déplacer une locomotive entre une voie d’entrée et plusieurs voies de dépôt dans une maquette ferroviaire. 
Le contrôle s’effectue avec un **Arduino**, un moteur pas à pas, un capteur Hall, un écran LCD I2C et un clavier matriciel 4x4.

## 📖 Description

### Caractéristiques principales
- 🎯 **40 voies** configurables et calibrables
- 📐 **Diagnostic au démarrage**
- 🏠 **Fonction de homing** pour retrouver la position de référence
- 🔄 **Optimisation des trajets** (chemin le plus court)
- 💾 **Sauvegarde en EEPROM** des positions et de la voie courante en EEPROM avec contrôle d’intégrité
- 🎮 **Interface LCD 20x4** avec clavier 4x4 intégré
- 🔧 **Mode maintenance** pour le déplacement manuel du pont 
- 🗜️ **Mode calibration** pour ajuster et sauvegarder précisément les positions des voies

---

## 🛠️ Matériel Requis et connexions

| Composant | Spécifications | Broche Arduino |
|-----------|-----------------|-----------------|
| **Arduino** | Uno / Nano / Mega | - |
| **Pont tournant** | JOUEF, 40 voies max, 9° entre voies | - |
| **Moteur pas-à-pas** | NEMA 14, 200 pas/rotation, réduction 8:1 (400 pas/tour) | - |
| **Driver moteur** | A4988 | DIR: D11, STEP: D12 |
| **Afficheur LCD** | 20x4, I2C, adresse 0x27 | SDA: A4, SCL: A5 |
| **Clavier** | 4x4 matriciel | Lignes: D9,D8,D7,D6; Colonnes: D5,D4,D3,D2 |
| **Capteur Hall** | Pour homing | A0 (pull-up interne) |
| **Buzzer** | Confirmation audio | D13 |
| **EEPROM** | Sauvegarde configuration | Intégrée à l'Arduino |

## 📋 Exigences et Contraintes Système

### Contraintes physiques
- Le pont **n'est pas symétrique** (présence d'une cabine de pilotage)
- L'entrée de la locomotive se fait toujours **à l'opposé de la cabine**
- **Retournement possible** : pivot de 180° nécessaire sur certaines manœuvres (retournement)
- **Optimisation requise** : chemin le plus court (sens horaire ou anti-horaire)

### Contraintes électriques
- La broche **ENA du driver A4988 n’est pas connectée**, ce qui maintient le courant dans le moteur 
  pour une position stable mais peut entraîner une surchauffe. Il faut impérativement ajuster le courant maximal 
  délivré au moteur pas à pas *via* le potentiomètre présent sur le driver.

### Contraintes logiques
- La **voie d'entrée** (voie 0) est le point de référence (position zéro)
- **Fonction de homing** appelée systématiquement à l'initialisation
- **Sauvegarde en EEPROM** de la configuration des voies et de la voie courante
- **Magic byte** (0xA5) pour vérifier l'intégrité des données en EEPROM

### Configuration du système
- Maximum **40 voies** (actuellement limité par la structure du code)
- Chaque voie espacée de **10 pas** (400 pas / 40 voies)
- **Limites EEPROM** : ~100 000 écritures ; optimisation des writes en cas de non-changement

---

## 🚦 Limitation Actuelle et Extensions Futures

### Limitation : 40 voies maximum

Actuellement, le logiciel est configuré pour gérer **40 voies maximum**.

### Pour dépasser cette limite :

**1. Modifications logicielles :**
- Modifier la constante `NB_MAX_VOIE`
- Adapter le tableau `tabVoie[]`
- Mettre à jour les messages LCD (ex: "Voie (1-40)")
- Modifier la logique de saisie dans `saisirNumeroVoie()`
- Recalculer l'espacement entre voies

**2. Modifications matérielles :**
- Pour 80 voies : espacement de 5 pas (400 / 80) au lieu de 10
- Moteur plus précis ou réduction différente
- EEPROM supplémentaire si nécessaire

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

## 🚀 Homing (Recherche de la position zéro)

La fonction `homing()` :
- Utilise le **capteur Hall** pour trouver la voie d'entrée
- Effectue jusqu'à **3 révolutions** pour garantir la détection
- Réinitialise la position du moteur à **0 pas**
- Définit `voieCourante = 0`
- Sauvegarde en EEPROM

---

## ⚡ Algorithmes Clés

### 📍 Optimisation du trajet (Plus court chemin)

Fonction : `calculerPlusCourtChemin(posActuelle, posCible)`

```
Distance directe = posCible - posActuelle

SI |Distance| > (MAX_PAS / 2) pas (demi-tour) ALORS
   SI Distance > 0 ALORS
      Distance -= MAX_PAS  // Aller "à rebours"
   SINON
      Distance += MAX_PAS  // Aller "à l'endroit"
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

Ramène la position du moteur dans l'intervalle **[0..MAX_PAS]** en utilisant le modulo :

```cpp
position = position % MAX_PAS
SI (position < 0) ALORS position += MAX_PAS
```

Cela évite les débordements lors de manœuvres répétées.

---

### 💾 Gestion EEPROM

**Stratégie :**
- Vérification du magic byte au démarrage
- Validation des données (0 ≤ pos ≤ MAX_PAS)
- Réinitialisation si corrompues
- Write optimisé : écriture seulement si changement

---

## ⚙️ Mode Diagnostic (touche V au démarrage)

Affiche :
- Position courante du moteur
- Numéro de voie courant
- État du capteur Hall (ACTIF ou Libre)
- Version du logiciel

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

Ce projet est sous licence **GNU General Public License v3.0 (GPLv3)**.
Vous êtes libre de copier, modifier et distribuer ce logiciel, à condition que toute version modifiée soit également distribuée sous la même licence GPLv3. 
Pour plus de détails, consultez : https://www.gnu.org/licenses/gpl-3.0.html


---

## 🔗 Ressources Supplémentaires

- [Documentation Arduino](https://www.arduino.cc/reference/)
- [AccelStepper Library](http://www.airspayce.com/mikem/arduino/AccelStepper/)
- [LiquidCrystal I2C](https://github.com/frank-zhu/LiquidCrystal_I2C)
- [Keypad Library](https://github.com/Chris--A/Keypad)

---

## 📞 Support et Bugs

Pour signaler des bugs ou proposer des améliorations, ouvrir une issue sur le repository GitHub.
