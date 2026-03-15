// --------------------------------------------------------------------
//
// TITRE       : Pont Tournant Nouvelle Generation
// AUTEUR      : M. EPARDEAU et F. FRANKE
// DATE        : 10/03/2026
//
#define VERSION "V0.R1"
//
//DESCRIPTION :
//
// --------------------------------------------------------------------

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <AccelStepper.h>

// Une seconde = 1000 millisecondes
#define JESUS    false
#define SECOND   1000
#define ERREUR  -1
#define ABANDON -1
#define OK       0 
#define ENTREE   0
#define SORTIE   1
#define OUI      1

// Pin du buzzer
const int buzzerPin = 13; // adapter selon votre montage

// Configuration LCD I2C 20x4 (adresse 0x27, à adapter si besoin)
LiquidCrystal_I2C lcd(0x27, 20, 4);

#define ROWS 4
#define COLS 4
const char kpKeys[ROWS][COLS] = {
  {'1', '2', '3', 'U'},
  {'4', '5', '6', 'D'},
  {'7', '8', '9', 'E'},
  {'L', '0', 'R', 'V'}
};
byte rowKpPin [ROWS] = {9, 8, 7, 6};
byte colKpPin [COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(kpKeys), rowKpPin, colKpPin, ROWS, COLS);

// Liste des types de manœuvres
const int nbManoeuvres = 2;
const char* manoeuvres[nbManoeuvres] = {
  "Entree",
  "Sortie"
};

// Options Oui/Non pour retournement
const int nbOuiNon = 2;
const char* ouiNon[nbOuiNon] = {"Non", "Oui"};

// Configuration du moteur pas à pas
//  - Moteur à pas NEMA 14 200 pas/rotation avec reduction 2:1 ()
//    via A4988 sur les broches D11 (DIR) et D12 (STEP)
const int enaStepperPin = 10;
const int dirStepperPin = 11;
const int stepStepperPin = 12;

// Nombre de pas pour une rotation complète (200 pas * réduction 2:1)
const int stepsPerRevolution = 400;

// Instanciation du pont tournant avec la librairie AccelStepper
// Cette librairie  elle permet de définir une vitesse maximale (setMaxSpeed) 
// et une accélération (setAcceleration), ce qui assure des démarrages
// et des arrêts progressifs pour le pont tournant
AccelStepper pontTournant(AccelStepper::DRIVER, stepStepperPin, dirStepperPin);

// Configuration du pont tournant
//
// Nombre maximum de voies disponibles
/* const int NB_MAX_VOIE = 40;*/
#define NB_MAX_VOIE 40
// Tableau des positions en pas pour chaque voie (0 à 40)
// Pour un moteur à 200 pas/rev. et une reduction de 1/2, il y a au total 400 pas/rev.
// Avec 400 pas/révolution et 40 voies : chaque voie = 10 pas
const int tabVoie[NB_MAX_VOIE + 1] = {
  0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
  110, 120, 130, 140, 150, 160, 170, 180, 190, 200,
  210, 220, 230, 240, 250, 260, 270, 280, 290, 300,
  310, 320, 330, 340, 350, 360, 370, 380, 390, 400 };

// Definition des voies principales
const byte voieEntree = 0;
const byte voieSortie = voieEntree;

// Au démarrage, la voie courante est la voie d'entree
int voieCourante = voieEntree;

/* --------------------------------------------------------------------
-----------------------------------------------------------------------*/
void setup() {

  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);

  // LCD.begin(0x27, 20, 4);
  lcd.init();          // initialisation de l'écran I2C
  lcd.backlight();     // allumer le rétroéclairage
  lcd.clear();
  lcd.setCursor(0,0);

  // afficher la VERSION
  lcd.print("Pont Tournant ");
  lcd.print(VERSION); 
  delay(1500);
  lcd.clear();

  // Configuration des broches moteur PAP
  pinMode(stepStepperPin,OUTPUT);
  pinMode(dirStepperPin,OUTPUT);
  pinMode(enaStepperPin, OUTPUT);
  // Active le driver (LOW = enabled sur A4988)
  digitalWrite(enaStepperPin, LOW); 

  // Position initiale du moteur PAP
  pontTournant.setCurrentPosition(0);

  // Configuration moteur PAP
  pontTournant.setMaxSpeed(1000);
  pontTournant.setAcceleration(100);

  // AJOUTER UNE FONCTION DE HOMING

}

/* --------------------------------------------------------------------
 Fonction pour émettre un bip court sur le buzzer
-----------------------------------------------------------------------*/
void beep() {
  tone(buzzerPin, 1000, 150); // 1000 Hz pendant 150 ms
  delay(200);
  noTone(buzzerPin);
}

/* --------------------------------------------------------------------
 Fonction pour afficher un menu simple avec curseur
-----------------------------------------------------------------------*/
void afficherMenu(const char* titre, const char* options[], int nbOptions, int selection) {

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(titre);

  for (int i = 0; i < nbOptions; i++) {
    lcd.setCursor(0, i+1);

    if (i == selection) {
      lcd.print("> ");
    } else {
      lcd.print("  ");
    }

    lcd.print(options[i]);
  }
}

/* --------------------------------------------------------------------
// Fonction indépendante pour saisir le type de manœuvre
// Retourne :
//   0 pour "Entree"
//   1 pour "Sortie"
//  -1 si annulation (ESC)
-----------------------------------------------------------------------*/
int saisirTypeManoeuvre() {

  int selection = 0;

  afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selection);

  while (!JESUS) {

    char touche = keypad.getKey();

    if (touche) {

      switch (touche) {

        case 'U': // Flèche haut
          if (selection > 0) selection--;
          afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selection);
          break;

        case 'D': // Flèche bas
          if (selection < (nbManoeuvres - 1)) selection++;
          afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selection);
          break;

        case 'E': // ESC
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Saisie annulee");
          delay(1*SECOND);
          return ABANDON; // annulation

        case 'V': // VAL
          lcd.clear();
          lcd.setCursor(0,3);
          lcd.print("Valide: ");
          lcd.print(manoeuvres[selection]);
          delay(1*SECOND);
          return selection;  

      } // case
    } // if
  } // while
}

/* --------------------------------------------------------------------
// Fonction indépendante pour saisir un numéro de voie (1-40) via touches numériques
// Retourne le numéro validé ou -1 si annulation
-----------------------------------------------------------------------*/
int saisirNumeroVoie() {

  String saisie = "";

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("N0 Voie (1-40):");
  lcd.setCursor(0,1);
  lcd.print("> ");

  while (!JESUS) {

    char touche = keypad.getKey();   

    if (touche) {

      if (((touche >= '0') && (touche <= '9')) && (saisie.length() < 2)) {
        if (saisie.length() < 2) { // max 2 chiffres (40 max)
          saisie += touche;
          lcd.setCursor(2,1);
          lcd.print(saisie);            //lcd.print("  ");
          lcd.setCursor(2,1);
          lcd.print(saisie);
        }
        
      } else if (touche == 'L') {
        if (saisie.length() > 0) {
          saisie.remove(saisie.length() - 1);
          lcd.setCursor(2,1);
          lcd.print("  ");
          lcd.setCursor(2,1);
          lcd.print(saisie);
        }

      } else if (touche == 'E') { // ESC
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print("Saisie annulee");
        delay(1*SECOND);
        return ABANDON;

      } else if (touche == 'V') { // VAL
        if (saisie.length() == 0) {
          beep(); // Pas de saisie, bip d'erreur
          continue;
        }

        int numeroVoie = saisie.toInt();

        if ((numeroVoie < 1) || (numeroVoie > NB_MAX_VOIE)) {
          lcd.setCursor(0,2);
          lcd.print("Erreur: 1 a ");
          lcd.print(NB_MAX_VOIE);
          beep(); // Bip erreur
          delay(1*SECOND);
          lcd.setCursor(0,2);
          lcd.print("               ");
          lcd.setCursor(2,1);
          lcd.print(saisie);

        } else {
          lcd.clear();
          lcd.setCursor(0,3);
          lcd.print("Voie: ");
          lcd.print(numeroVoie);
          delay(1*SECOND);
          return numeroVoie;
        }

      } else {
        // Touche non valide pour ce contexte
        beep();
      }
    }
  }
}

/* --------------------------------------------------------------------
// Fonction indépendante pour demander si la manœuvre nécessite un retournement
// Retourne 0 pour Non, 1 pour Oui, -1 si annulation
-----------------------------------------------------------------------*/
int demanderRetournement() {

  int selection = 0;

  afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);

  while (!JESUS) {

    char touche = keypad.getKey();

    if (touche) {

      switch (touche) {
        case 'U': // Flèche haut
          if (selection > 0) selection--;
          afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);
          break;

        case 'D': // Flèche bas
          if (selection < nbOuiNon - 1) selection++;
          afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);
          break;

        case 'E': // ESC
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Annule");
          delay(1500);
          return ABANDON; // annulation

        case 'V': // VAL
          lcd.clear();
          lcd.setCursor(0, 3);
          lcd.println((selection == OUI) ? "Retournement" : "Sans retournement");
          delay(1*SECOND);
          return selection; // 0 = Non, 1 = Oui
      }
    }
  }
}

/* ==========================================================
   Saisir sur le PAD entree/sortie loco sur PT par touche '*'
   
   Afficher "Loco deplacee?"
   Attendre que l’utilisateur appuie sur 'V'
   
   Retourner OK
   ========================================================== */
int attendreDeplacementEngin() {

  char touche = '\0';     // donne automatiquement la valeur ASCII
  bool entreeValide = false;

  // Saisir le type de manoeuvre Entree=A ou Sortie=B
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Loco deplacee? ");

  do {
    touche = keypad.getKey();
    entreeValide = (touche == 'V' );
    if (!entreeValide) {beep();}
  } while (!entreeValide);

  return OK;
}

/* ==================================
  Optimiser le trajet du pont roulant

  Le pont roulant peut tourner dans les deux sens SAM et SIAM.
  Pour minimiser le déplacement du pont, on calcule la distance 
  entre la position actuelle et la cible. Si cette distance est plus grande
  que la moitié d’un tour complet, on tourne dans l’autre sens. 
  Pour cela, au lieu d'ajouter la distance qui sépare la position actuelle
  vers la position cible, on soustrait cette distance.
   ================================== */
int calculerPlusCourtChemin(int currentPos, int targetPos) {

  // Calculer la distance entre la position actuelle et la position cible
  int distance = targetPos - currentPos;

  // Si la distance absolue est supérieure à la moitié d'un tour complet
  if (abs(distance) > stepsPerRevolution / 2) {
    // Ajuster la distance pour prendre le chemin le plus court
    if (distance > 0) {
      distance -= stepsPerRevolution;
    } else {
      distance += stepsPerRevolution;
    }
  }

  // return currentPos + distance;
  return distance;
}

/* ==================================================
   Deplacer le PT de la voie actuelle à la voie cible
   
   Si retournement demandé alors calculer la voie opposée : voie = (20 + voie) % 40
   Si déjà sur la voie alors terminer en retournant OK

   Afficher "En rotation"
   Normaliser la position actuelle si > stepsPerRevolution pas
   
   Calculer le sens d erotation optimal (SAM ou SIAM)
   Déplacer le moteur à la position cible
   Mettre à jour la voie courante
   
   Effacer le message "En rotation"
   Retourner OK
   ================================================== */
int deplacerPT(const int voieCible, const int retournement) {

  int voie = voieCible;

  // Si c'est un retournement alors choisir la voie opposée (pivot de 180 deg.)
  if (retournement == OUI) {
    // Pour retourner le pont il faut le positionner sur la voie d'en face
    // (puisque le nombre de voies est pair)
    // le calcul du modulo (operateur %) permet de rester dans l'intervalle [1..NB_MAX_VOIE]
    // Exemple : sur la voie 30, la voie en face est la voie 10
    //           à la voie 30 j'ajoute 20 (40 voies / 2) cela donne la voie 50
    //           et 50 modulo 40 donne 10 (reste de la division de 50 / 40)
    voie = (NB_MAX_VOIE / 2 + voie) % NB_MAX_VOIE;
  }

  // Si on est deja sur la voie  alors ne rien faire
  if (voie == voieCourante) {
    lcd.clear(); //effacerLCD(3);
    return OK;
  }

  lcd.setCursor(0,3);
  lcd.print("En rotation        ");

  // Normaliser si necessaire
  // La librairie AccelStepper calcul un nombre de pas absolue
  // Si par exemple je fais deux tours d'un moteur 40 pas
  // la position courante du moteur sera égale à 80
  // Le calcul du modulo parmet de ramener la position
  // dans l'intervalle [10..stepsPerRevolution]
  if (abs(pontTournant.currentPosition()) > stepsPerRevolution) {
    int position = pontTournant.currentPosition() % stepsPerRevolution;
    pontTournant.setCurrentPosition(position);
  }

  // Calculer la maneuvre optimale (ie tournant SAM ou SIAM 
  // afin de minimiser le nombre de pas)
  int distance = calculerPlusCourtChemin(tabVoie[voieCourante], tabVoie[voie]);

  // Réaliser la manoeuvre
  pontTournant.moveTo(pontTournant.currentPosition() + distance);
  pontTournant.runToPosition();

  lcd.clear();
  voieCourante = voie;
  return OK;
}

/* --------------------------------------------------------------------
-----------------------------------------------------------------------*/
void loop() {

  int retournementChoisi;

  lcd.clear();

  // Saisir le type de manoeuvre Entree ou Sortie
  int typeManoeuvre = saisirTypeManoeuvre();  
  int voieSelectionnee;                       

  switch (typeManoeuvre) {  

   case ENTREE:
      if (voieCourante != voieEntree) {
        deplacerPT(voieEntree, false);
        attendreDeplacementEngin();
      }

      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }

      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }

      deplacerPT(voieSelectionnee, retournementChoisi);
    break;

    case SORTIE:
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }

      deplacerPT(voieSelectionnee, false);
      attendreDeplacementEngin();

      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }     
      deplacerPT(voieEntree, retournementChoisi);
    break;

    case ABANDON: 
      return;
    break;

    default:
      // Cas non prévu 
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("ERREUR INNATENDUE");
      delay (3*SECOND);
      return;
      break;
  }

  // Liberer le pont avant la prochaine manoeuvre
  attendreDeplacementEngin();
  
  lcd.setCursor(0,3);
  lcd.print("Fin de manoeuvre");
  delay(2*SECOND);

}
