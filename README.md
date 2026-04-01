# Projet de Pont Tournant Ferroviaire Nouvelle Génération

## Objectifs du projet

Ce projet a pour objectif de contrôler une maquette de **pont tournant ferroviaire** automatisé et motorisé.
Il permet de déplacer une locomotive entre une voie d’entrée et plusieurs voies de dépôt dans une maquette ferroviaire.
Le contrôle s’effectue avec un **Arduino**, un moteur pas à pas, un capteur Hall, un écran LCD I2C et un clavier matriciel 4x4.

## ✨ Principales fonctionnalités

* 🚂 **Manœuvres** d'entrée et de sortie au dépôt et de transfert entre les voies
* 🔧 **Mode Maintenance** pour le déplacement manuel du pont par pas de 1 ou de 10
* 📐️ **Mode Diagnostic** pour vérifier la position moteur, la voie courante et l'état du capteur Hall
* 🗜 **Mode Calibration** pour l'ajustement précis de la position de chaque voie

## 📖 Caractéristiques principales

* 🎯 **40 voies** configurables et étalonnables
* 🏠 **Fonction de homing** pour retrouver la position de référence
* 🔄 **Optimisation des trajets** recherche du plus court chemin
* 💾 **Sauvegarde en EEPROM** des positions et de la voie courante en EEPROM avec contrôle d’intégrité
* 🎮 **Interface LCD 20x4** avec clavier 4x4 intégré

\---

## 🛠️ Matériel Requis et connexions

|Composant|Spécifications|Broche Arduino|
|-|-|-|
|**Arduino**|Uno / Nano / Mega|-|
|**Pont tournant**|JOUEF, 40 voies max, 9° entre voies|-|
|**Moteur pas-à-pas**|NEMA 14, 200 pas/rotation, réduction 8:1 (400 pas/tour)|-|
|**Driver moteur**|A4988|DIR: D11, STEP: D12|
|**Afficheur LCD**|20x4, I2C, adresse 0x27|SDA: A4, SCL: A5|
|**Clavier**|4x4 matriciel|Lignes: D9,D8,D7,D6; Colonnes: D5,D4,D3,D2|
|**Capteur Hall**|Pour homing|A0 (pull-up interne)|
|**Buzzer**|Confirmation audio|D13|
|**EEPROM**|Sauvegarde configuration|Intégrée à l'Arduino|

## 📋 Exigences et Contraintes Système

### Contraintes physiques

* Le pont **n'est pas symétrique** (présence d'une cabine de pilotage)
* L'entrée de la locomotive se fait toujours **à l'opposé de la cabine**
* **Retournement possible** : pivot de 180° nécessaire sur certaines manœuvres
* **Optimisation requise** : chemin le plus court (sens horaire ou anti-horaire)

### Contraintes électriques

* La broche **ENA du driver A4988 n’est pas connectée**, ce qui maintient le courant dans le moteur
pour une position stable mais peut entraîner une surchauffe. Il faut impérativement ajuster le courant maximal
délivré au moteur pas à pas *via* le potentiomètre présent sur le driver.

### Contraintes logiques

* La **voie d'entrée** (voie 0) est le point de référence (position zéro)
* **Fonction de homing** appelée systématiquement à l'initialisation
* **Sauvegarde en EEPROM** de la configuration des voies et de la voie courante
* **Magic byte** (0xA5) pour vérifier la présence des données en EEPROM

### Configuration du système

* Maximum **40 voies** (constante définie dans le code)
* Chaque voie espacée de **10 pas** (400 pas / 40 voies)
* **Limites EEPROM** : \~100 000 écritures ; optimisation du nombre des écritures

\---

## 🚦 Limitation Actuelle et Extensions Futures

### Pour dépasser la limite de 40 voies maximum :

**1. Modifications logicielles :**

* Modifier la constante `NB\_MAX\_VOIE`
* Adapter le tableau `tabVoie\[]`
* Mettre à jour les messages LCD (ex: "Voie (1-40)")
* Modifier la logique de saisie dans `saisirNumeroVoie()`
* Recalculer l'espacement entre voies

**2. Modifications matérielles :**

* Pour 80 voies : espacement de 5 pas (400 / 80) au lieu de 10
* Moteur plus précis ou réduction différente
* EEPROM supplémentaire si nécessaire

\---

## 🎮 Modes de Fonctionnement

### Manœuvre d'entrée

Amener une locomotive de la voie principale vers une voie du dépôt.

**Séquence :**

1. Positionnement initial → Voie d'entrée (voie 0)
2. Saisie de la destination (voie 1-40)
3. Choix du retournement (Oui/Non)
4. Déplacement optimisé vers la voie cible
5. Confirmation finale

```
Voie d'entrée (0) ──→ Voie cible (1-40)
```

\---

### Manœuvre de sortie

Sortir une locomotive d'une voie du dépôt vers la voie principale.

**Séquence :**

1. Saisie de la voie source (voie 1-40)
2. Déplacement vers la voie source
3. Choix du retournement (Oui/Non)
4. Retour à la voie d'entrée (voie 0)

```
Voie source (1-40) ──→ Voie d'entrée (0)
```

\---

### Manœuvre de transfert

Déplacement d'une locomotive d'une voie du dépôt vers une autre voie.

**Séquence :**

1. Saisie de la voie origine (voie 1-40)
2. Choix du retournement (Oui/Non)
3. Déplacement vers la voie origine
4. Saisie de la voie destination (voie 1-40)
5. Déplacement vers la voie destination

```
Voie origine (1-40) ──→ Voie destination (1-40)
```

\---

### Mode Maintenance

Déplacement **manuel pas-à-pas** du pont tournant pour tests et diagnostics.

**Commandes :**

* `U` : +10 pas
* `D` : -10 pas
* `R` : +1 pas
* `L` : -1 pas
* `E` : Quitter

\---

### Mode Calibration

Ajustement **précis** de la position de chaque voie et sauvegarde en EEPROM.

**Commandes :**

* `U` : +1 offset
* `D` : -1 offset
* `R` : Voie suivante
* `L` : Voie précédente
* `V` : Valider et sauvegarder
* `E` : Quitter

\---

## 🎹 Interface Utilisateur - Clavier 4x4

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

|Touche|Action|
|-|-|
|**U/D**|Naviguer dans le menu|
|**V**|Valider sélection|
|**E**|Annuler / Quitter|

### Saisie numérique (Voies 1-40)

|Touche|Action|
|-|-|
|**0-9**|Saisir le numéro|
|**L**|Effacer le dernier chiffre|
|**V**|Valider la saisie|
|**E**|Annuler|

### Sélection Oui/Non (Retournement)

|Touche|Action|
|-|-|
|**U/D**|Basculer Non ↔ Oui|
|**V**|Valider le choix|
|**E**|Annuler|

\---

## 🚀 Homing (Recherche de la position zéro)

La fonction `homing()` :

* Utilise le **capteur Hall** pour trouver la voie d'entrée
* Effectue jusqu'à **3 révolutions** pour garantir la détection
* Réinitialise la position du moteur à **0 pas**
* Définit `voieCourante = 0`
* Sauvegarde en EEPROM

\---

## 📝 Notes Importantes

### ⚠️ Risques EEPROM

* **100 000 écritures maximum** par adresse
* Le code optimise en écrivant **uniquement si changement**
* Éviter les écritures inutiles en mode calibration

### ⚠️ Dissipation thermique

* Driver A4988 et moteur peuvent **chauffer**
* **Régler le potentiomètre de courant** du driver A4988
* Assurer une bonne **ventilation**

### ⚠️ Capteur Hall

* Pull-up **interne** activé (pas de résistance externe nécessaire)
* Aimant doit passer **régulièrement** près du capteur
* Homing **obligatoire** au démarrage

\---

## 🤝 Auteurs

* **M. EPARDEAU** - Conception
* **F. FRANKE** - Développement

\---

## 📄 Licence

Ce projet est sous licence **GNU General Public License v3.0 (GPLv3)**.
Vous êtes libre de copier, modifier et distribuer ce logiciel, à condition que toute version modifiée soit également distribuée sous la même licence GPLv3.
Pour plus de détails, consultez : https://www.gnu.org/licenses/gpl-3.0.html



\---

## 🔗 Ressources Supplémentaires

* [Documentation Arduino](https://www.arduino.cc/reference/)
* [AccelStepper Library](http://www.airspayce.com/mikem/arduino/AccelStepper/)
* [LiquidCrystal I2C](https://github.com/frank-zhu/LiquidCrystal_I2C)
* [Keypad Library](https://github.com/Chris--A/Keypad)

\---

## 📞 Support et Bugs

Pour signaler des bugs ou proposer des améliorations, ouvrir une issue sur le repository GitHub.

