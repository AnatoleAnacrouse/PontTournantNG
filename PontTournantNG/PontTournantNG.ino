// --------------------------------------------------------------------
//
// TITRE       : Pont tournant NG
// AUTEUR      : M. EPARDEAU et F. FRANKE
// DATE        : 10/03/2026
//
#define VERSION "  VERSION 0.1"
//
//DESCRIPTION :
//
// --------------------------------------------------------------------

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <AccelStepper.h>

// Une seconde = 1000 millisecondes
#define SECOND 1000

// Pin du buzzer
const int buzzerPin = 13; // adapter selon votre montage

// Configuration LCD I2C 20x4 (adresse 0x27, à adapter si besoin)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Configuration keypad 5x4
const byte ROWS = 5;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},   // A = flèche haut
  {'4','5','6','B'},   // B = flèche bas
  {'7','8','9','C'},   // C = Échap
  {'*','0','#','D'},   // D = ENT
  {'E','F','G','H'}    // touches inutilisées ou autres
};

byte rowPins[ROWS] = {9, 8, 7, 6, 10}; // connecter aux lignes du keypad
byte colPins[COLS] = {A3, A2, A1, A0}; // connecter aux colonnes du keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

/*
// --------------------------------------------------------------------
// Configuration du moteur pas à pas
//
//  - Moteur à pas NEMA 14 200 pas/rotation avec reduction 2:1 ()
//       via A4988 sur les broches D11 (DIR) et D12 (STEP)
// Broche Arduino pour le signal STEP (impulsions)
#define PIN_MOT_STEP 12
// Broche Arduino pour le signal DIR (direction)
#define PIN_MOT_DIR 11
// Broche Arduino pour le signal ENA (enable)
#define PIN_MOT_ENA 10

// Nombre de pas pour une rotation complète (200 pas * réduction 2:1)
const int stepsPerRevolution = 400;
//
// Instanciation du pont tournant avec la librairie AccelStepper
// Cette librairie  elle permet de définir une vitesse maximale (setMaxSpeed) 
// et une accélération (setAcceleration), ce qui assure des démarrages 
// et des arrêts progressifs pour le pont tournant
AccelStepper pontTournant(1, PIN_MOT_STEP, PIN_MOT_DIR);

// Configuration du pont tournant
//
// Nombre maximum de voies disponibles
#define NB_MAX_VOIE 40

// Tableau des positions en pas pour chaque voie (0 à 40)
// Pour un moteur à 200 pas/rev. et une reduction de 1/2, il y a au total 400 pas/rev.
// Avec 400 pas/révolution et 40 voies : chaque voie = 10 pas
const int tabVoie[NB_MAX_VOIE + 1] = {
  0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
  110, 120, 130, 140, 150, 160, 170, 180, 190, 200,
  210, 220, 230, 240, 250, 260, 270, 280, 290, 300,
  310, 320, 330, 340, 350, 360, 370, 380, 390, 400 };
*/

// Definition des voies principales
const byte voieEntree = 0;
const byte voieSortie = voieEntree;

// Au démarrage, la voie courante est la voie d'entree
int voieCourante = voieEntree;

// Liste des types de manœuvres
const char* manoeuvres[] = {
  "Entree",
  "Sortie"
};
const int nbManoeuvres = sizeof(manoeuvres) / sizeof(manoeuvres[0]);

// Options Oui/Non pour retournement
const char* ouiNon[] = {"Non", "Oui"};
const int nbOuiNon = 2;


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
//  -1 si annulation (Échap)
-----------------------------------------------------------------------*/
int saisirTypeManoeuvre() {

  int selectedIndex = 0;
  //afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selectedIndex);

  while (true) {

    char key = keypad.getKey();

    if (key) {
      switch (key) {
        case 'A': // Flèche haut
          if (selectedIndex > 0) selectedIndex--;
          //afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selectedIndex);
          break;

        case 'B': // Flèche bas
          if (selectedIndex < nbManoeuvres - 1) selectedIndex++;
          //afficherMenu("Type de manoeuvre:", manoeuvres, nbManoeuvres, selectedIndex);
          break;

        case 'C': // Échap
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Saisie annulee");
          delay(1500);
          return -1; // annulation

        case 'D': // ENT
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Valide: ");
          lcd.print(manoeuvres[selectedIndex]);
          delay(1500);
          return selectedIndex; // retour de la sélection
      } // case
    } // if
  } // while
}

/* --------------------------------------------------------------------
// Fonction indépendante pour saisir un numéro de voie (1-40) via touches numériques
// Retourne le numéro validé ou -1 si annulation
-----------------------------------------------------------------------*/
int saisirNumeroVoie() {

  String input = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("N° Voie (1-40):");
  lcd.setCursor(0,1);
  lcd.print("> ");

  while (true) {

    char key = keypad.getKey();

    if (key) {

      if (key >= '0' && key <= '9') {
        if (input.length() < 2) { // max 2 chiffres (40 max)
          input += key;
          lcd.setCursor(2,1);
          lcd.print("  ");
          lcd.setCursor(2,1);
          lcd.print(input);
        }
      } else if (key == '#') {
        if (input.length() > 0) {
          input.remove(input.length() - 1);
          lcd.setCursor(2,1);
          lcd.print("  ");
          lcd.setCursor(2,1);
          lcd.print(input);
        }
      } else if (key == 'C') { // Échap
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print("Saisie annulee");
        delay(1500);
        return -1;
      } else if (key == 'D') { // ENT
        if (input.length() == 0) {
          beep(); // Pas de saisie, bip d'erreur
          continue;
        }
        int numero = input.toInt();
        if (numero < 1 || numero > 40) {
          lcd.setCursor(0,2);
          lcd.print("Erreur: 1 a 40");
          beep(); // Bip erreur
          delay(1500);
          lcd.setCursor(0,2);
          lcd.print("               ");
          lcd.setCursor(2,1);
          lcd.print(input);
        } else {
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Valide: ");
          lcd.print(numero);
          delay(1500);
          return numero;
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
  //afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);

  while (true) {

    char key = keypad.getKey();

    if (key) {

      switch (key) {
        case 'A': // Flèche haut
          if (selection > 0) selection--;
          //afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);
          break;

        case 'B': // Flèche bas
          if (selection < nbOuiNon - 1) selection++;
          //afficherMenu("Retournement ?", ouiNon, nbOuiNon, selection);
          break;

        case 'C': // Échap
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Annule");
          delay(1500);
          return -1; // annulation
        case 'D': // ENT

          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print("Valide: ");
          lcd.print(ouiNon[selection]);
          delay(1500);
          return selection; // 0 = Non, 1 = Oui
      }
    }
  }
}

/* --------------------------------------------------------------------
  Attendre la confirmation de l'operateur
-----------------------------------------------------------------------*/
int attendreDeplacementEngin() {
/*
  char touche = '\0';     // donne automatiquement la valeur ASCII
  bool entreeValide = false;

  // Saisir le type de manoeuvre Entree=A ou Sortie=B
  afficherLCD("Loco deplacee? oui=*", 3, false);

  do {
    touche = kp.getKey();
    entreeValide = (touche == '*' );
    //if (!entreeValide) {}; // tone(PIN_BUZZER, 220, 1/2*SECOND);
  } while (!entreeValide);

  effacerLCD(3);
  return OK;

  */
}

/* --------------------------------------------------------------------
  Optimiser le trajet du pont roulant
-----------------------------------------------------------------------*/
int calculerPlusCourtChemin(int currentPos, int targetPos) {

  // Calculer la distance entre la position actuelle et la position cible
  int distance = targetPos - currentPos;

  // Si la distance absolue est supérieure à la moitié d'un tour complet
  /*if (abs(distance) > stepsPerRevolution / 2) {
    // Ajuster la distance pour prendre le chemin le plus court
    if (distance > 0) {
      distance -= stepsPerRevolution;
    } else {
      distance += stepsPerRevolution;
    }
  }*/

  // return currentPos + distance;
  return distance;
}

/* --------------------------------------------------------------------
   Deplacer le PT de la voie actuelle à la voie cible
-----------------------------------------------------------------------*/
int deplacerPT(const int voieCible, const int retournement) {

  int voie = voieCible;
/*
  afficherLCD("En rotation", 3, false);

  // Si c'est un retournement alors choisir la voie opposée (pivot de 180 deg.)
  if (retournement == RETOURNEMENT) {
    voie = (NB_MAX_VOIE / 2 + voie) % NB_MAX_VOIE;
  }

  // Si on est deja sur la voie  alors ne rien faire
  if (voie == voieCourante) {
    effacerLCD(3);
    return OK;
  }

  // Normaliser si necessaire
  if (abs(pontTournant.currentPosition()) > stepsPerRevolution) {
    int position = pontTournant.currentPosition() % stepsPerRevolution;
    pontTournant.setCurrentPosition(position);
  }

  // Calculer la maneuvre optimale
  int distance = calculerPlusCourtChemin(tabVoie[voieCourante], tabVoie[voie]);

  // Réaliser la manoeuvre
  pontTournant.moveTo(pontTournant.currentPosition() + distance);
  pontTournant.runToPosition();

  effacerLCD(3);
  voieCourante = voie;
  return OK;   
  */
}

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

  // Configuration des broches moteur PAP
  /* pinMode(PIN_MOT_DIR,OUTPUT);
  pinMode(PIN_MOT_STEP,OUTPUT);
  pinMode(PIN_MOT_ENA, OUTPUT);
  // Active le driver (LOW = enabled sur A4988)
  digitalWrite(PIN_MOT_ENA, LOW); 
  */

  // afficher la VERSION
  lcd.print("Demarrage...");
  delay(1500);
  lcd.clear();
}

/* --------------------------------------------------------------------
-----------------------------------------------------------------------*/
void loop() {

  // Saisie du type de manœuvre
  int typeManoeuvre = saisirTypeManoeuvre();
  if (typeManoeuvre == -1) {
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Saisie annulee");
    delay(2000);
    while(1);
  }

  // Saisie du numéro de voie
  int numeroVoie = saisirNumeroVoie();
  if (numeroVoie == -1) {
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Saisie annulee");
    delay(2000);
    while(1);
  }

  // Saisie de la nécessité d'un retournement
  int retournement = demanderRetournement();
  if (retournement == -1) {
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Saisie annulee");
    delay(2000);
    while(1);
  }

  // Affichage final
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Manoeuvre: ");
  lcd.print(typeManoeuvre == 0 ? "Entree" : "Sortie");

  lcd.setCursor(0,1);
  lcd.print("Voie: ");
  lcd.print(numeroVoie);

  lcd.setCursor(0,2);
  lcd.print("Retournement: ");
  lcd.print(retournement == 1 ? "Oui" : "Non");

  delay(4000);


/*

  // Boucle principale

  int voieSelectionnee = 0;
  int retournementChoisi = SANSRETOURNEMENT;

  LCD.clear();

  // Saisir le type de manoeuvre Entree ou Sortie
  int typeManoeuvre = saisirTypeManoeuvre();

  switch (typeManoeuvre) { 

    case ABANDON: 
      return;
    break;

    case ENTREE:
      afficherLCD("Entree", 0, true);
      if (voieCourante != voieEntree) {
        deplacerPT(voieEntree, false);
        attendreDeplacementEngin();
      }

      voieSelectionnee = saisirVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }
      else if (voieSelectionnee == ERREUR) {
        afficherLCD("Saisie erronnee", 3, false);
        delay (1*SECOND);
        return;
      }

      retournementChoisi = saisirRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }

      deplacerPT(voieSelectionnee, retournementChoisi);
    break;

    case SORTIE:
      afficherLCD("Sortie", 0, true);
      voieSelectionnee = saisirVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }
      else if (voieSelectionnee == ERREUR) {
        afficherLCD("Saisie erronnee", 3, false);
        delay (1*SECOND);
        return;
      }

      deplacerPT(voieSelectionnee, false);
      attendreDeplacementEngin();

      retournementChoisi = saisirRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }     
      deplacerPT(voieEntree, retournementChoisi);
    break;

    default:
      // Cas non prévu 
      afficherLCD("ERREUR DE TRAITEMENT", 3, false);
      delay (1*SECOND);
      break;
  }

  // Liberer le pont avant la prochaine manoeuvre
    attendreDeplacementEngin();
*/
}
