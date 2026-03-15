# Projet de Pont Tournant Ferroviaire Nouvelle Génération

## Description

Ce projet permet de gérer un **pont tournant ferroviaire** avec un **Arduino**.

Le pont tournant permet de faire pivoter une voie pour aligner une locomotive avec une voie (entrée et/ou sortie) et une voie de garage.

## Matériel

Le matériel est constitué de :

* un pont tournant JOUEF, 40 voies max, angle de 9° entre chaque voie ;
* un Arduino Uno / Nano ou Mega ;
* un afficheur LCD 4 x 20 I2C avec SCL sur les broches A5 et SDA sur A4 ;
* PAD 4 x 4 touches sur les broches D2 a D9 ;
* un capteur Hall et un aimant pour le *homing* ;
* un moteur à pas NEMA 14 à 200 pas/rotation avec réduction 8:1 via A4988 sur les broches D11 (DIR) et D12 (STEP) et mise en œuvre avec la librairie `AccelStepper()`

## Exigences et contraintes

L'entrée de la locomotive sur le pont et la sortie peut se faire sur la même voie ou sur deux voies différentes.

Le pont n’est pas symétrique du fait de la présence de la cabine de pilotage sur le pont.
L’entrée de la locomotive sur le pont se fait toujours à l’opposé de la cabine.
De ce fait, il doit être possible de retourner la locomotive lors d'une manœuvre.

Pour atteindre la voie choisie, le trajet du pont doit être optimisé en choisissant le chemin le plus court dans le sens des aiguilles d'une montre ou le sens inverse.

Une locomotive à vapeur est toujours garée, dans une voie couverte, cheminée vers le pont tournant (garée *tender en arrière* sur sa voie de garage couverte).

La voie de référence pour le point zéro du moteur PAP est la voie d’entrée (voie 1). Une fonction de *homing* doit permet de recaler le pont sur la voie d'entrée.
Cette fonction est appelée systématiquement à l'initialisation du pont.

## Limitation

Actuellement, le logiciel est configuré pour un 40 voies.

Pour dépasser cette limite, plusieurs modifications logicielles et matérielles seraient nécessaires :

1. Le code définit la constante `NB_MAX_VOIE` à 40.
2. La fonction de saisie des voies indique explicitement qu'il « ne peut pas y avoir plus de 40 voies » et rejette toute saisie supérieure à ce nombre avec un message d'erreur.
3. Le système utilise un moteur pas à pas avec une réduction aboutissant à 400 pas par révolution. Avec 40 voies, chaque voie est espacée exactement de 10 pas (400 / 40 = 10). Pour 80 voies, l'écart ne serait plus que de 5 pas entre chaque voie. Et le moteur utilisé ne pourrait positionner plus de 400 voies. Pour gérer un nombre différents de 40 voies, il faudrait modifier : a) le tableau nommé `tabVoie[]` ; b) les messages affichés sur l'écran LCD (comme "Voie (1-40)") ; c) la logique de saisie du clavier dans la fonction `saisirVoie()`.

## Logique de fonctionnement du pont tournant

### Mode Entrée (Manœuvre vers le garage)

Ce mode est utilisé pour amener une locomotive de la voie principale vers l'une des 40 voies de garage.

La séquence est la suivante :
* Positionnement initial : si le pont n'y est pas déjà, il se déplace automatiquement vers la voie d'entrée (définie comme la voie 0).
* Embarquement : le système attend que l'utilisateur confirme que la locomotive est montée sur le pont (touche \*).
* Destination : l'utilisateur saisit le numéro de la voie de garage (1 à 40) où il souhaite envoyer la locomotive.
* Mouvement final : le pont se déplace vers la voie sélectionnée, avec ou sans retournement de 180° selon le choix de l'utilisateur.

### Mode Sortie (Manœuvre vers la voie principale)

Ce mode est utilisé pour sortir une locomotive d'une voie de garage afin de la ramener sur le réseau.

La séquence est inversée :
* Positionnement initial : l'utilisateur doit d'abord saisir le numéro de la voie où se trouve la locomotive. Le pont se déplace alors vers cette voie.
* Embarquement : le système attend que la locomotive monte sur le pont (confirmé par la touche \*).
* Destination : le système sait que la destination est la voie d'entrée (voie 0).
* Mouvement final : le pont retourne à la voie d'entrée, en effectuant ou non un retournement selon la demande.

### Résumé des différences :

|Caractéristique|Mode Entrée (A)|Mode Sortie (B)|
|-|-|-|
|Point de départ|Voie d'entrée (0)|Voie sélectionnée (1-40)|
|Point d'arrivée|Voie sélectionnée (1-40)|Voie d'entrée (0)|
|Saisie de voie|Après l'embarquement|Avant l'embarquement|

Dans les deux cas, le logiciel propose à l'utilisateur de choisir si un retournement (pivotement de 180°) est nécessaire avant d'atteindre la destination finale.

## Protocoles de Manœuvre et Interface Utilisateur

L'interface utilisateur permet de piloter le pont via des commandes spécifiques sur le pavé numérique.

Le système distingue deux types de manœuvre :
1. Touche 'A' pour une entrée : le pont se positionne d'abord sur la voie d'entrée (voie 0) pour recevoir une locomotive, puis se déplace vers la voie de destination choisie ;
2. Touche 'B' pour une sortie : le pont se déplace vers une voie sélectionnée pour récupérer une locomotive, puis revient à la voie d'entrée/sortie.

Pour choisir la voie, l'utilisateur doit saisir un numéro entre 1 et 40.

Si un retournement de la locomotive est nécessaire, l'utilisateur doit presser :
1. Touche 'C' : retournement ;
2. Touche 'D' : sans retournement.

Annulation : La touche # permet d'abandonner la manœuvre en cours à tout moment.

## Optimisation du déplacement du pont tournant

Le code utilise une fonction nommée `calculerPlusCourtChemin()` pour évaluer de manière optimale le déplacement du pont tournant.

Voici comment fonctionne cette optimisation :

```
  calculer la distance (différence de pas) entre la position actuelle et la position cible

  SI la distance dépasse un demi-tour ALORS
     SI la distance est positive ALORS
        soustraire un tour complet
     SINON (la distance est négative) ALORS
        ajouter un tour complet
     FIN SI
 FIN SI
 
 Retourner la distance optimisée (positive ou négative)
```

**Exemple concret**

Si le pont est à la position 10 pas et doit aller à la position 350 pas :

* La distance directe est de +340 pas.
* Comme 340 est supérieur à 200, le code calcule : 340 - 400 = **-60 pas**.
* Le pont tournera donc de seulement 60 pas dans le sens inverse au lieu de faire presque un tour complet de 340 pas.

## Procédure de normalisation du nombre de pas

En cas de dépassement du nombre de pas pour une révolution complète (compte tenu de la réduction, dans notre cas 400 pas, l'opérateur modulo (%) est appliquée dans la fonction de déplacement `deplacerPT()`.

Cette nouvelle valeur "normalisée" est alors définie comme la position courante du moteur via la commande `setCurrentPosition(position)`.

Cette opération permet au système de toujours travailler avec des coordonnées comprises entre 0 et 399 pas, évitant ainsi des erreurs de calcul potentielles ou des dépassements de capacité des variables lors de manœuvres répétées dans le même sens.


