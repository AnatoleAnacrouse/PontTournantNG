// ====================================================================================
//                         PONT TOURNANT NOUVELLE GÉNÉRATION
// ====================================================================================
// Auteur  : M. EPARDEAU et F. FRANKE
// Date    : 20 avril 2026
// Version : Cf. define ci dessous
// Projet  : Contrôle d’un pont tournant motorisé pour maquette ferroviaire :
//           - Permet de déplacer une locomotive entre une voie d’entrée et une voie
//             du dépôt, avec ou sans retournement, via un pont tournant motorisé.
//           - Une fonction de "homing" permet de rechercher la position de la voie
//             de garage.
//           - Un diagnostic est lancé au démarrage afin de présenter la position
//             du moteur, la voie courante et l'état capteur Hall
//           - Le mode MAINTENANCE permet de déplacer manuellement le pont par
//             pas de 1 ou 10.
//           - Un mode CALIBRATION permet d'ajuster précisément la position de
//             chaque voie et de sauvegarder en EEPROM la configuration.
//           - Les positions des voies ainsi que la position courante du pont sont
//             sauvegardées en EEPROM. Un magic byte (0xA5) permet de vérifier
//             l'intégrité des données et de les réinitialiser si elles sont corrompues.
// ------------------------------------------------------------------------------------
// CONFIGURATION
// - Arduino ou compatible
// - Moteur pas à pas : 400 pas/tour, piloté via un driver moteur A4988,
//                      pins STEP (12), DIR (11), ENA (10) avec clavier non I2C
//                      pins STEP (10), DIR (11), ENA (9) avec clavier I2C
// - Capteur Origine  : broche 3 pour détection position zéro (pull-up interne activé ou pas selon capteur))
// - LCD I2C 20 x 4   : adresse 0x27, 20x4, pins SDA (A4), SCL (A5)
// - Clavier 4x4      : non I2C ou I2C
//                      - cas 1, Non I2C : lignes (9,8,7,6), colonnes (5,4,3,2)
//                      - cas 2, I2C : utilisation d'un module PCF8574.
//                          lignes sur broches (0,1,2,3) du PCF8574
//                          colonnes sur broches (4,5,6,7) du PCF8574
//                      - cas 3, I2C : utilisation d'un module MCP23017.
//                          lignes sur broches PA0 à PA3 du DFROBOT MCP23017
//                          colonnes sur broches PB0 à PB4 du DFROBOT MCP23017
// - Buzzer           : broche 6
// - EEPROM           : pour la sauvegarde des positions des voies
//                      et de la voie courante
// ====================================================================================

// ------------------------------------------------------------------------------------
// CONFIGURATION MATERIEL
// A commenter si keypad non I2C
// ------------------------------------------------------------------------------------
#define KEYPAD_I2C

// ------------------------------------------------------------------------------------
// LIBRAIRIES
// ------------------------------------------------------------------------------------
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <EEPROM.h>
#include <Adafruit_MCP23X17.h>

/*
  #ifdef KEYPAD_I2C
  #include <Keypad_I2C.h>
  #else
  #include <Keypad.h>
  #endif
*/

Adafruit_MCP23X17 mcp1;

// ------------------------------------------------------------------------------------
// CONFIGURATION GÉNÉRALE
// ------------------------------------------------------------------------------------
#define VERSION        "V0.11"
#define JESUS_CHRIST   false
#define OK             0
#define ABANDON       -1
#define I2CADDR       0x20           // Adresse module PCF8574
#define I2CADDRmcp1   0x23           // Adresse module MCP23017 N°1

// ------------------------------------------------------------------------------------
// CONSTANTES MOTEUR
// ------------------------------------------------------------------------------------
#define SPEED_NORMAL     1200
#define ACCEL_NORMAL     400
#define SPEED_HOMING     200
#define ACCEL_HOMING     100

// ------------------------------------------------------------------------------------
// CONSTANTES D'AFFICHAGE
// ------------------------------------------------------------------------------------
#define TIMEOUT_MSG         1500   // durée affichage messages courts (ms)
#define TIMEOUT_ERREUR      3000   // durée affichage erreurs (ms)
#define TIMEOUT_SAUVEGARDE  1000   // durée confirmation sauvegarde (ms)

// ------------------------------------------------------------------------------------
// CONSTANTES DE DUREE MAX DU HOMING
// ------------------------------------------------------------------------------------
#define DELAY_WATCHDOG  40000 // 10 000 ms = 10 secondes

// ------------------------------------------------------------------------------------
// CAPTEUR ORIGINE & BUZZER
// ------------------------------------------------------------------------------------

//const byte capteurOrigineBroche = 3;
//const byte ledHoming = 4;
//const byte BPInitPosition = 5;                // en prevision RAZ à la demande
//const byte buzzerPin = 6;
const unsigned long tpsClignot = 700;   // Durée du clignotement
const unsigned long tpsOnOff = 700;     // Clignotement LED prise origine

// ------------------------------------------------------------------------------------
// LCD I2C 20x4
// ------------------------------------------------------------------------------------
const byte NB_LIGNE = 4;
const byte NB_CHAR = 20;
LiquidCrystal_I2C lcd1(0x27, NB_CHAR, NB_LIGNE);

/*
  // ------------------------------------------------------------------------------------
  // CLAVIER 4x4, 16 touches, 8 broches
  // U : Up / avancer               D : Down / reculer
  // L : Left / Déplacer à gauche   R : Right / Déplacer à droite
  // V : Valider                    E : Echap / Annuler
  // ------------------------------------------------------------------------------------
  const byte ROWS_4x4 = 4;
  const byte COLS_4x4 = 4;

  const char touches_4x4[ROWS_4x4][COLS_4x4] = {
  {'1', '2', '3', 'U'},
  {'4', '5', '6', 'D'},
  {'7', '8', '9', 'E'},
  {'L', '0', 'R', 'V'}
  };
*/


// ------------------------------------------------------------------------------------
// CLAVIER 5x4, 20 touches, 9 broches
// H : Haut / avancer               B : Bas / reculer
// G : Gauche / Déplacer à gauche   D : Droite / Déplacer à droite
// V : Valider                      E : Echap / Annuler
// ------------------------------------------------------------------------------------
const byte ROWS_5x4 = 4;
const byte COLS_5x4 = 5;

// Définition des touches (à adapter selon besoins)
char touches_5x4[ROWS_5x4][COLS_5x4] = {
  {'*', 'H', 'B', 'E', 'V'},
  {'#', '3', '6', '9', 'D'},
  {'Y', '2', '5', '8', '0'},
  {'X', '1', '4', '7', 'G'}
};

/*
  #ifdef KEYPAD_I2C
  const byte rowKpPins [ROWS_4x4] = {0, 1, 2, 3};   // broches du PCF8574
  const byte colKpPins [COLS_4x4] = {4, 5, 6, 7};   // broches du PCF8574

  TwoWire *jwire = &Wire;                 // test balayage pointeur bibliothèque clavier
  Keypad_I2C clavier4x4 = Keypad_I2C(makeKeymap(touches_4x4), rowKpPins, colKpPins, ROWS_4x4, COLS_4x4, I2CADDR, PCF8574, jwire);
  #else
  const byte rowKpPins[ROWS_4x4] = {9, 8, 7, 6};
  const byte colKpPins[COLS_4x4] = {5, 4, 3, 2};
  Keypad clavier4x4 = Keypad(makeKeymap(touches_4x4), rowKpPins, colKpPins, ROWS_4x4, COLS_4x4);
  #endif
*/

// ------------------------------------------------------------------------------------
// MOTEUR PAS À PAS PILOTE VIA LE DRIVER A4988
// La broche ENA du driver n'est pas connectée à l'Arduino et restera donc à l'état bas.
// Un courant circulera dans les bobines du moteur même à l'arrêt ce qui permettra au
// pont de rester parfaitement aligné face aux voies. Par contre, le driver A4988 et
// le moteur vont chauffer. Il est donc impératif de régler correctement le courant
// via le potentiomètre du driver A4988 à la valeur minimale.
// ------------------------------------------------------------------------------------
/*
  #ifdef KEYPAD_I2C
  const byte enableStepperPin = 9;
  const byte stepStepperPin = 10;
  const byte dirStepperPin  = 11;
  const byte capteurOrigineBroche = 3;
  const byte ledHoming = 4;
  const byte BPInitPosition = 5;                // en prevision RAZ à la demande
  const byte buzzerPin = 6;
  #else
  const byte enableStepperPin = 10;
  const byte dirStepperPin    = 11;
  const byte stepStepperPin   = 12;
  const byte capteurOrigineBroche = A0;
  const byte buzzerPin = 13;
  #endif
*/

const byte enableStepperPin = 9;
const byte stepStepperPin = 10;
const byte dirStepperPin  = 11;
const byte capteurOrigineBroche = 3;
const byte ledHoming = 4;
const byte BPInitPosition = 5;                // en prevision RAZ à la demande
const byte buzzerPin = 6;

AccelStepper pontTournant(AccelStepper::DRIVER, stepStepperPin, dirStepperPin);

// ------------------------------------------------------------------------------------
// POSITIONS DES VOIES (1..40) — valeurs par défaut
// Chargées / écrasées depuis EEPROM au démarrage
// ------------------------------------------------------------------------------------
#define NB_MAX_VOIE 40

/*
  // postion voies en pas, transmission 1/3 et driver A4988 en full pas (1/1)
  // Modifier stepsPerRevolution en consequence et ajuster vitesse et accélération/décéleration
  const int tabVoie[NB_MAX_VOIE + 1] = {
  0,
  30, 60, 90, 120, 150, 180, 210, 240, 270, 300,
  330, 360, 390, 420, 450, 480, 510, 540, 570, 600,
  630, 660, 690, 720, 750, 780, 810, 840, 870, 900,
  930, 960, 990, 1020, 1050, 1080, 1110, 1140, 1170, 1200
  };
*/

/*
  // Moteur PAP 400 pas, reduction 1/3, A4988 en 1/1 pas, FONCTIONNE avec PT Jouef
  // mettre const int stepsPerRevolution = 4800;
  // mettre const int vitesseRotation = 800;
  const int tabVoie[NB_MAX_VOIE + 1] = {
  0,
  120, 240, 360, 480, 600, 720, 840, 960, 1080, 1200,
  1320, 1440, 1560, 1680, 1800, 1920, 2040, 2160, 2280, 2400,
  2520, 2640, 2760, 2880, 3000, 3120, 3240, 3360, 3480, 3600,
  3720, 3840, 3960, 4080, 4200, 4320, 4440, 4560, 4680, 4800
  };
*/

/*
  #ifdef KEYPAD_I2C
  // FONCTIONNE avec PT PECO
  // Moteur PAP 400 pas, reduction 1/3, A4988 en 1/1 pas
  // mettre const int stepsPerRevolution = 9600;
  // mettre const int vitesseRotation = 200;
  const int tabVoie[NB_MAX_VOIE + 1] = {
  0,  240,  480,  720,  960, 1200, 1440, 1680, 1920, 2160,
  2400, 2640, 2880, 3120, 3360, 3600, 3840, 4080, 4320, 4560,
  4800, 5040, 5280, 5520, 5760, 6000, 6240, 6480, 6720, 6960,
  7200, 7440, 7680, 7920, 8160, 8400, 8640, 8880, 9120, 9360, 9600
  };
  const int stepsPerRevolution = 9600;

  #else
  const int tabVoie[NB_MAX_VOIE + 1] = {
  0,
  10,  20,  30,  40,  50,  60,  70,  80,  90, 100,
  110, 120, 130, 140, 150, 160, 170, 180, 190, 200,
  210, 220, 230, 240, 250, 260, 270, 280, 290, 300,
  310, 320, 330, 340, 350, 360, 370, 380, 390, 400
  };
  const int stepsPerRevolution = 400;
  #endif
*/

// FONCTIONNE avec PT PECO
// Moteur PAP 400 pas, reduction 1/3, A4988 en 1/1 pas
// mettre const int stepsPerRevolution = 9600;
// mettre const int vitesseRotation = 200;
const int tabVoie[NB_MAX_VOIE + 1] = {
  0,  240,  480,  720,  960, 1200, 1440, 1680, 1920, 2160,
  2400, 2640, 2880, 3120, 3360, 3600, 3840, 4080, 4320, 4560,
  4800, 5040, 5280, 5520, 5760, 6000, 6240, 6480, 6720, 6960,
  7200, 7440, 7680, 7920, 8160, 8400, 8640, 8880, 9120, 9360, 9600
};
const int stepsPerRevolution = 9600;
const int voieEntree = 0;

// ------------------------------------------------------------------------------------
// ADRESSES EEPROM
// tabVoie occupe (NB_MAX_VOIE+1) * sizeof(int) octets
// voieCourante est stockée juste après
// ------------------------------------------------------------------------------------
#define EEPROM_ADDR_VOIES         0
#define EEPROM_ADDR_VOIE_COURANTE ((NB_MAX_VOIE + 1) * sizeof(int))
#define EEPROM_MAGIC_ADDR         (EEPROM_ADDR_VOIE_COURANTE + sizeof(int))
#define EEPROM_MAGIC_VALUE        0xA5

// ------------------------------------------------------------------------------------
// CONFIGURATION DU PONT
// ------------------------------------------------------------------------------------
struct ConfigurationPontTournant
{
  byte magic;
  int tabVoie[NB_MAX_VOIE + 1];
  int voieCourante;
};

ConfigurationPontTournant configPT;

// ------------------------------------------------------------------------------------
// ENUM DES DIFFERENTS ITEMS DU MENU PRINCIPAL
// ------------------------------------------------------------------------------------
enum TypeManoeuvre
{
  MANOEUVRE_ENTREE      = 0,
  MANOEUVRE_SORTIE      = 1,
  MANOEUVRE_TRANSFERT   = 2,
  MANOEUVRE_MAINTENANCE = 3,
  MANOEUVRE_CALIBRATION = 4,
  ARRET                 = 5
};

// ------------------------------------------------------------------------------------
// ALERTE SONORE
// ------------------------------------------------------------------------------------
void beep(bool error = false)
{
  if (error)
  {
    //tone(buzzerPin, 500, 400);
    delay(450);
  }
  else
  {
    //tone(buzzerPin, 1000, 120);
    delay(150);
  }
  //noTone(buzzerPin);
}


// ------------------------------------------------------------------------------------
// FONCTION DE GESTION DU CLAVIER 5x4 SUR MCP23017
// ------------------------------------------------------------------------------------
char obtentionTouche()
{
  for (int r = 0; r < ROWS_5x4; r++)
  {
    mcp1.digitalWrite(r, LOW);              // On met la ligne actuelle à LOW

    for (int c = 0; c < COLS_5x4; c++)
    {
      if (mcp1.digitalRead(c + 8) == LOW)   // Si la colonne est à LOW, la touche est pressée
      {
        mcp1.digitalWrite(r, HIGH);         // On remet la ligne à HIGH avant de sortir
        return touches_5x4[r][c];
      }
    }
    mcp1.digitalWrite(r, HIGH);             // On remet la ligne à HIGH
  }
  return '\0';                              // Aucune touche pressée
}     // FIN char obtentionToucheClavier()

// ------------------------------------------------------------------------------------
// FONCTION AFFICHAGE TOUCHE CLAVIER 5x4 SUR MCP23017
// ------------------------------------------------------------------------------------
void affichageToucheClavier()
{
  char touche_5x4 = obtentionTouche();

  if (touche_5x4 != '\0')
  {
    Serial.print(F("Touche pressee : "));
    Serial.println(touche_5x4);
    delay(100); // Anti-rebond simple, 200 à l'origine
  }
}

// ------------------------------------------------------------------------------------
// FONCTIONS DE GESTION DE L'AFFICHEUR LCD
// ------------------------------------------------------------------------------------
void effacerLigne(const byte line)
{
  lcd1.setCursor(0, line);
  lcd1.print(F("                    "));
}

// ------------------------------------------------------------------------------------
void afficherTitre(String titre)
{
  lcd1.clear();

  // Calculer le nombre d'espaces à gauche pour centrer
  int espacesGauche = (NB_CHAR - titre.length()) / 2;
  if (espacesGauche < 0) espacesGauche = 0; // si titre trop long

  // Construire la ligne avec espaces à gauche + titre + espaces à droite
  String ligne = "";
  for (int i = 0; i < espacesGauche; i++)
  {
    ligne += " ";
  }
  ligne += titre;
  // Compléter avec espaces à droite pour remplir la ligne
  while (ligne.length() < (NB_CHAR)) ligne += " ";

  // Afficher sur la première ligne (ligne 0)
  lcd1.setCursor(0, 0);
  lcd1.print(ligne);
}

// ------------------------------------------------------------------------------------
void afficherLigne(String msg, byte ligne = 1)
{
  effacerLigne(ligne);
  lcd1.setCursor(0, ligne);
  lcd1.print(msg);
}

// ------------------------------------------------------------------------------------
void afficherMessage(String msg, byte ligne = 1, bool erreur = false, int duree = TIMEOUT_MSG)
{
  effacerLigne(ligne);
  lcd1.setCursor(0, ligne);
  lcd1.print(msg);
  delay(duree);
  beep(erreur);
}

// ------------------------------------------------------------------------------------
// GESTION DES EXCEPTIONS
// ------------------------------------------------------------------------------------
void exception(String message = "")
{
  // Avertir l'opérateur
  lcd1.clear();
  afficherTitre(F("ERREUR IRRECOUVRABLE"));
  afficherMessage(message, 3, true, TIMEOUT_ERREUR);
  beep(true);

  pontTournant.stop();                      // Arrêter et libérer moteur PAP
  digitalWrite(enableStepperPin, HIGH);

  // Boucler en attendant une intervention
  while (true)
  {
    delay(1000);
  }
}

// ------------------------------------------------------------------------------------
// SAUVEGARDE DE LA VOIE COURANTE EN EEPROM
// ATTENTION : l'EEPROM est limitée à 100 000 écritures. On ne sauvegarde la voie
//             courante que si la valeur a changée.
// ------------------------------------------------------------------------------------
void sauvegarderVoieCourante()
{
  // Lire la voie courante en EEPROM
  int voieEE;
  EEPROM.get(EEPROM_ADDR_VOIE_COURANTE, voieEE);

  if (configPT.voieCourante != voieEE)        // Si voie courante est différente voie en EEPROM
  {
    // On ne sauvegarde que s'il y a une différence
    EEPROM.put(EEPROM_ADDR_VOIE_COURANTE, configPT.voieCourante);
  }
}

// ------------------------------------------------------------------------------------
// SAUVEGARDE DE LA CONFIGURATION COMPLETE DU PONT EN EEPROM
// ------------------------------------------------------------------------------------
void sauverConfigurationPontTournant()
{
  configPT.magic = EEPROM_MAGIC_VALUE;
  EEPROM.put(EEPROM_ADDR_VOIES, configPT);
}

// ------------------------------------------------------------------------------------
// Gere en EEPROM la configuration du pont (tabVoie et voieCourante)
// et vérifie que ces données sont presentes et valides.
// Un magic byte permet de determiner le type d'un fichier (ici la présence)
// ------------------------------------------------------------------------------------
void chargerEEPROM()
{
  EEPROM.get(EEPROM_ADDR_VOIES, configPT);         // Lire la configuration en EEPROM

  if (configPT.magic != EEPROM_MAGIC_VALUE)        // Si configuration n'est pas présente ou incorrecte
  {
    afficherTitre(F("Init EEPROM..."));
    // Initialiser la configuration dans l'EEPROM
    configPT.voieCourante = voieEntree;
    memcpy(configPT.tabVoie, tabVoie, sizeof(tabVoie));
    sauverConfigurationPontTournant();
    // Prévenir l'opéreateur
    afficherMessage(F("EEPROM initialisee  "), 3, false, TIMEOUT_MSG);   ///++++++++++
    return;
  }

  // Vérifier la configuration en EEPROM
  afficherTitre(F("Verif EEPROM..."));

  // Vérifier que les données des voies sont dans la plage autorisée
  bool valide = true;
  for (int i = 0; i <= NB_MAX_VOIE; i++)
  {
    if ((configPT.tabVoie[i] < 0) || (configPT.tabVoie[i] > stepsPerRevolution))
    {
      valide = false;
      break;
    }
  }

  if (valide)                   // Vérifier que voie courante est valide
  {
    if ((configPT.voieCourante  < 0) || (configPT.voieCourante > NB_MAX_VOIE))
    {
      valide = false;
    }
  }

  if (!valide)               // Si l'EEPROM est corrompue
  {
    afficherMessage(F("EEPROM invalide !"), 3, true, TIMEOUT_ERREUR);   // Prévenir opérateur
    configPT.voieCourante = voieEntree;                                 // Réinitialiser configuration dans EEPROM
    memcpy(configPT.tabVoie, tabVoie, sizeof(tabVoie));                 // recharger les défauts
    sauverConfigurationPontTournant();
    afficherMessage(F("EEPROM reinitialisee"), 3, true, TIMEOUT_MSG);   // Prévenir l'opérateur
  }
}

// ------------------------------------------------------------------------------------
// GESTION DE LA BARRE DE PROGRESSION SUR L'AFFICHEUR LCD
// ------------------------------------------------------------------------------------
inline void afficherProgression(int progress) __attribute__((always_inline));

inline void afficherProgression(int progress)
{
  // Ecréter entre 0.0 et 1.0
  if (progress < 0) progress = 0;
  if (progress > 100) progress = 100;

  // Calculer le nombre de blocks à afficher
  int blocs = int((progress / 100.0) * NB_CHAR);

  // Afficher les blocks et effacer le reste de la ligne
  lcd1.setCursor(0, 2);
  for (int i = 0; i < NB_CHAR; i++)
  {
    lcd1.print(i < blocs ? char(255) : ' ');
  }

  // Afficher le % de progression
  lcd1.setCursor(0, 3);
  lcd1.print(progress);
  lcd1.print("%   ");
}

// ------------------------------------------------------------------------------------
// DIAGNOSTIC
// ------------------------------------------------------------------------------------
void diagnostic()
{
  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  bool entreeValide = false;

  afficherTitre(F("== DIAGNOSTIC == "));

  // Afficher les infos de diagnostic
  String message = "Hall:" + String((digitalRead(capteurOrigineBroche) == LOW ? "ACTIF" : "Libre"))
                   + " Ver:" + String(VERSION);
  afficherLigne(message, 1);
  message = "Pos:" + String(pontTournant.currentPosition())
            + " Voie:"
            + (configPT.voieCourante == 0 ? "entree" : String(configPT.voieCourante));
  afficherLigne(message, 2);
  afficherLigne(F("V ou E: Quitter"), 3);

  // Attendre la réponse de l'opérateur
  do {
    //touche_4x4 = clavier4x4.getKey();
    touche_5x4 = obtentionTouche();
    //entreeValide = ((touche_4x4 == 'V') || (touche_4x4 == 'E'));
    entreeValide = ((touche_5x4 == 'V') || (touche_5x4 == 'E'));
  } while (!entreeValide);
}


// --------------------------------------------------------------------
// Fonction CLIGNOTEMENT LED
// --------------------------------------------------------------------
void clignotementLED()
{
  static unsigned long startms = 0;
  static unsigned long blinkms = 0;

  //si le bouton est appuyé,
  if (digitalRead(BPInitPosition) == LOW || digitalRead(capteurOrigineBroche) == LOW/*HIGH*/)
  {
    // init time
    startms = millis();
  }

  if (startms && millis() - startms <= tpsClignot)
  {
    if (millis() - blinkms >= tpsOnOff)
    {
      // inverse état led
      digitalWrite(ledHoming, !digitalRead(ledHoming));
      blinkms = millis();
    }
  }
  else
  {
    startms = 0;
    digitalWrite(ledHoming, LOW);
  }
} // FIN void clignotementLED()


// ------------------------------------------------------------------------------------
// RECRCHE DE LA VOIE DE SORTIE / HOMING
// ------------------------------------------------------------------------------------
void homing()
{
  afficherLigne(F("Recherche origine..."), 2);
  effacerLigne(3);

  // Réduire vitesse et accélération
  pontTournant.setMaxSpeed(SPEED_HOMING);
  pontTournant.setAcceleration(ACCEL_HOMING);

  // Lancer 3 révolutions du moteur pour garantir que le capteur est bien trouvé
  pontTournant.moveTo(pontTournant.currentPosition() + 3 * stepsPerRevolution);

  unsigned long start = millis();         // Armer watchdog pour éviter boucle infinie

  // Arreter rotation PT si capteur origine détecté ou si nbre révolutions atteint
  while ((digitalRead(capteurOrigineBroche) == LOW /*HIGH*/) && (pontTournant.distanceToGo() > 0))
  {
    if ((millis() - start) > DELAY_WATCHDOG) break;   // Sortir de la boucle si le watchdog est tombé
    pontTournant.run();
    clignotementLED();
  }
  pontTournant.stop();
  digitalWrite(ledHoming, LOW);

  if (digitalRead(capteurOrigineBroche) == HIGH /*LOW*/)      // Si la voie est detectée
  {
    pontTournant.setCurrentPosition(0);                       // Définir position voie entrée
    configPT.voieCourante = voieEntree;
    sauvegarderVoieCourante();                                // Sauvegarder la configuration en EEPROM
    afficherMessage(F("Origine OK"), 3, false, TIMEOUT_MSG);  // Informer l'opérateur du succès
  }
  else
  {
    afficherMessage(F("ECHEC: origine NOK  "), 3, true, TIMEOUT_ERREUR);  // Informer opérateur de l'echec
  }

  // Restituer vitesse et accélération
  pontTournant.setMaxSpeed(SPEED_NORMAL);
  pontTournant.setAcceleration(ACCEL_NORMAL);
}

// ------------------------------------------------------------------------------------
// HOMING OPTIONNEL AU DÉMARRAGE
// ------------------------------------------------------------------------------------
void proposerHoming()
{
  // Afficher le menu de homing
  afficherTitre(F("== HOMING == "));
  afficherLigne(F("V: Oui E: Non"), 3);

  // Attendre la réponse de l'opérateur
  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  //while (touche_4x4 != 'V' && touche_4x4 != 'E')
  while (touche_5x4 != 'V' && touche_5x4 != 'E')
  {
    //touche_4x4 = clavier4x4.getKey();
    touche_5x4 = obtentionTouche();
  }

  //if (touche_4x4 == 'V')      // Lancer le homing
  if (touche_5x4 == 'V')      // Lancer le homing
  {
    homing();
  }
}

// ------------------------------------------------------------------------------------
// VOIE OPPOSÉE (retournement)
// Retourne la voie opposée à la voie en paramètre.
// Cette fonction ne fonctionne pas si le pont n'est pas symétrique.
// ------------------------------------------------------------------------------------
int voieOpposee(int voie)
{
  return ((NB_MAX_VOIE / 2 + voie - 1) % NB_MAX_VOIE) + 1;
}

// ------------------------------------------------------------------------------------
// NORMALISATION POSITION MOTEUR
// La librairie AccelStepper calcul un nombre de pas absolu positif ou négatif
// selon le sens de rotation. Par exemple, la position courante du moteur sera égale
// à 800 // après deux révolutions d'un moteur à 400 pas par révolution.
// ------------------------------------------------------------------------------------
long normaliserPosition()
{
  // Calculer la position dans l'intervalle [0..stepsPerRevolution]
  long position = pontTournant.currentPosition() % stepsPerRevolution;

  if (position < 0)          // Si position est négative calculer position positive équivalente
  {
    position += stepsPerRevolution;
  }
  pontTournant.setCurrentPosition(position);      // Reconfigurer le moteur
  return position;
}

// ------------------------------------------------------------------------------------
// CALCUL DU PLUS COURT CHEMIN (optimiser le trajet du pont roulant)
// Le pont roulant peut tourner dans les deux sens SAM et SIAM.
// Pour minimiser le déplacement du pont, on calcule la distance
// entre la position actuelle et la cible. Si cette distance est plus grande
// que la moitié d’un tour complet, on tourne dans l’autre sens.
// Pour cela, au lieu d'ajouter la distance qui sépare la position actuelle
// vers la position cible, on soustrait cette distance.
// ------------------------------------------------------------------------------------
long calculerPlusCourtChemin(long posActuelle, long posCible)
{
  long distance = posCible - posActuelle;         // Calculer distance entre position actuelle et position cible

  if (abs(distance) > (stepsPerRevolution / 2))   // Si distance absolue est supérieure à moitié tour complet
  {
    // Ajuster la distance pour prendre le chemin le plus court :
    //   - si la distance est positive on soustrait un tour complet
    //   - si la distance est négative on ajoute un tour complet
    distance += (distance > 0) ? -stepsPerRevolution : stepsPerRevolution;
  }
  return distance;
}

// ------------------------------------------------------------------------------------
// DÉPLACEMENT AVEC BARRE DE PROGRESSION
// ------------------------------------------------------------------------------------
void deplacerPontTournant(long cible)
{
  // Définir la position de départ et le nombre de pas
  long depart = pontTournant.currentPosition();
  float distance = abs(cible - depart);
  int distanceParcourue = 0;

  if (distance == 0.0)                // Ne rien faire si PT déjà en position
  {
    afficherProgression(100);
    return;
  }

  pontTournant.moveTo(cible);          // Déplacer pont en affichant progression
  while (pontTournant.distanceToGo() != 0)
  {
    pontTournant.run();
    // Calculer le reste à faire et n'afficher qu'une fois sur dix
    distanceParcourue = abs(pontTournant.currentPosition() - depart);
    if  ((distanceParcourue % 10) == 0)
    {
      afficherProgression(int((distanceParcourue / distance) * 100));
    }
  }
  afficherProgression(100);
}

// ------------------------------------------------------------------------------------
// REALISER UNE MANOEUVRE D'ENTREE OU DE SORTIE DU PONT TOURNANT
// ------------------------------------------------------------------------------------
int manoeuvrerPontTournant(const int versVoie, const byte retournement)
{
  int voieCible = versVoie;

  if (retournement)        // Si retournement alors choisir voie d'en face
  {
    voieCible = voieOpposee(voieCible);
  }

  if (voieCible == configPT.voieCourante)    // Si on est deja sur la voie  alors ne rien faire
  {
    return OK;
  }

  afficherLigne(F("Rotation..."), 1);       // Mettre à jour affichage

  int posActuelle = normaliserPosition();    // Normaliser si necessaire

  // Optimiser le trajet du pont roulant
  int distance = calculerPlusCourtChemin(posActuelle, configPT.tabVoie[voieCible]);
  deplacerPontTournant(posActuelle + distance);    // Deplacer le pont

  configPT.voieCourante = voieCible;       // Remettre a jour la configuration
  //sauvegarderVoieCourante();   /// +++++++++

  return OK;
}

// ------------------------------------------------------------------------------------
// GESTION DU MENU PRINCIPAL
// ------------------------------------------------------------------------------------
int saisirTypeManoeuvre()
{
  const int nbManoeuvres = 6;
  const char* manoeuvres[] = {
    "Entree", "Sortie", "Transfert", "Maintenance", "Calibration", "Arret"
  };

  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  bool entreeValide = false;

  int startIndex = 0;             // Index du premier item affiché à l'écran
  int selection = 0;              // Index de l'élément selectionné
  int selectionGlobale = 0;       // Index global de la sélection dans le tableau
  int selectionPrecedente = -1;   // Forcer le premier affichage
  int startIndexPrecedent = -1;

  lcd1.clear();

  while (!JESUS_CHRIST)
  {
    // Affichage seulement si la sélection ou la fenêtre a changé
    if (selection != selectionPrecedente || startIndex != startIndexPrecedent)
    {
      for (int i = 0; i < NB_LIGNE; i++)
      {
        int itemIndex = startIndex + i;
        char ligne[NB_CHAR + 1];              // Construire ligne fixe de 20 caractères
        memset(ligne, ' ', NB_CHAR);
        ligne[NB_CHAR] = '\0';

        if (itemIndex < nbManoeuvres)         // Si ligne est selectionnée alors afficher le curseur
        {
          ligne[0] = (i == selection) ? '>' : ' ';
          ligne[1] = ' ';
          // Ecrire le Label de l'item depuis la colonne 2, max 18 chars
          const char* label = manoeuvres[itemIndex];
          int len = strlen(label);
          if (len > 18) len = 18;
          memcpy(&ligne[2], label, len);
        }
        else
        {
          memset(ligne, ' ', NB_CHAR);      // Ligne vide si pas d'item
          ligne[NB_CHAR] = '\0';
        }
        // Afficher la ligne sur l'écran LCD
        lcd1.setCursor(0, i);
        lcd1.print(ligne);
      }
      // Etablir la "fenetre" du menu
      selectionPrecedente = selection;
      startIndexPrecedent = startIndex;
    }

    // Attente d'une touche valide
    do {
      //touche_4x4 = clavier4x4.getKey();
      touche_5x4 = obtentionTouche();
      /*
        entreeValide = ((touche_4x4 == 'U') ||
                      (touche_4x4 == 'D') ||
                      (touche_4x4 == 'V') ||
                      (touche_4x4 == 'E'));
      */
      entreeValide = ((touche_5x4 == 'H') ||
                      (touche_5x4 == 'B') ||
                      (touche_5x4 == 'V') ||
                      (touche_5x4 == 'E'));
    } while (!entreeValide);

    // Gestion des touches
    affichageToucheClavier();
    //if (touche_4x4 == 'U')
    if (touche_5x4 == 'H')
    {
      if (selection == 0)
      {
        if (startIndex > 0)    // Si on est en haut de la fenêtre et qu'on peut remonter dans la liste
        {
          startIndex--;
        }
      }
      else
      {
        selection--;
      }
    }
    //else if (touche_4x4 == 'D')
    else if (touche_5x4 == 'B')
    {
      if (selection == 3)
      {
        if ((startIndex + 4) < nbManoeuvres)     // Si on est en bas de la fenêtre et qu'on peut descendre dans la liste
        {
          startIndex++;
        }
      }
      else
      {
        if ((startIndex + selection + 1) < nbManoeuvres)      // Vérifier qu'on ne dépasse pas le nombre total d'items
        {
          selection++;
        }
      }
    }
    //else if (touche_4x4 == 'V')
    else if (touche_5x4 == 'V')
    {
      selectionGlobale = startIndex + selection;    // Calcul de l'index global sélectionné
      return selectionGlobale;
    }
    //else if (touche_4x4 == 'E')
    else if (touche_5x4 == 'E')
    {
      return ABANDON;
    }
  }
  //return ABANDON; // Robustesse
}

// ------------------------------------------------------------------------------------
// SAISIE NUMÉRO DE VOIE
// ------------------------------------------------------------------------------------
int saisirNumeroVoie()
{
  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  String saisie = "";

  // Afficher le message de saisie de la voie
  effacerLigne(2);
  effacerLigne(3);
  afficherLigne("Voie (1-" + String (NB_MAX_VOIE) + "):", 1);
  lcd1.setCursor(0, 2);
  lcd1.print(F("> "));

  while (!JESUS_CHRIST)
  {
    affichageToucheClavier();
    Serial.print(saisie);
    //touche_4x4 = clavier4x4.getKey();        // Attendre la saisie de l'opérateur
    touche_5x4 = obtentionTouche();        // Attendre la saisie de l'opérateur
    //if (!touche_4x4) continue;
    if (!touche_5x4) continue;

    //if ((touche_4x4 >= '0' && touche_4x4 <= '9') && (saisie.length() < 2))    // Si chiffre de 0 à 9
    if ((touche_5x4 >= '0' && touche_5x4 <= '9') && (saisie.length() < 2))    // Si chiffre de 0 à 9
    {
      //saisie += touche_4x4;
      saisie += touche_5x4;
      lcd1.setCursor(2, 2);
      lcd1.print("   ");
      lcd1.setCursor(2, 2);
      lcd1.print(saisie);
    }
    //else if (touche_4x4 == 'L' && saisie.length() > 0)    // Sinon si c'est la touche backspace
    else if (touche_5x4 == 'G' && saisie.length() > 0)    // Sinon si c'est la touche backspace
    {
      saisie.remove(saisie.length() - 1);
      lcd1.setCursor(2, 2);
      lcd1.print(F("   "));
      lcd1.setCursor(2, 2);
      lcd1.print(saisie);
      Serial.print(saisie);
    }
    //else if (touche_4x4 == 'V')               // Sinon si c'est une validation
    else if (touche_5x4 == 'V')               // Sinon si c'est une validation
    {
      if (saisie.length() == 0)           // Boucler si rien n'est saisie
      {
        beep();
        continue;
      }

      // Si le numéro de voie n'est pas conforme alors boucler
      // ===> TRAITER LA VOIE DE GARAGE = 0 avec if (voie < 0 ?????
      int voie = saisie.toInt();
      Serial.println(touche_5x4);
      Serial.println(voie);
      if (voie < 1 || voie > NB_MAX_VOIE)
      {
        String message = "Voie invalide (1-" + String(NB_MAX_VOIE) + ")";
        //String message = "ERREUR" + String(NB_MAX_VOIE);
        afficherMessage(message, 2, true, TIMEOUT_ERREUR);
        effacerLigne(2);
        effacerLigne(3);
        saisie = "";
        lcd1.setCursor(0, 2);
        lcd1.print(F("> "));

        continue;
      }
      // Sinon retourner la voie
      return voie;
    }
    //else if (touche_4x4 == 'E') return ABANDON;         // Sinon abandonner
    else if (touche_5x4 == 'E') return ABANDON;         // Sinon abandonner
  }
  //return ABANDON; // Robustesse
}

// ------------------------------------------------------------------------------------
// SAISIE RETOURNEMENT
// ------------------------------------------------------------------------------------
int demanderRetournement()
{
  const char* ouiNon[] = {"Non", "Oui"};

  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  bool entreeValide = false;
  int selection = 0;

  // Afficher le message de saisie du retournement
  effacerLigne(2);
  effacerLigne(3);
  afficherLigne(F("Retournement:       "), 1);

  while (!JESUS_CHRIST)
  {
    for (int i = 0; i < 2; i++)           // Afficher le choix Non ou Oui
    {
      lcd1.setCursor(0, i + 2);
      lcd1.print(i == selection ? "> " : "  ");
      lcd1.print(ouiNon[i]);
    }

    // Attendre la réponse de l'opérateur
    do {
      /*
        touche_4x4 = clavier4x4.getKey();
        entreeValide = ((touche_4x4 == 'U') || (touche_4x4 == 'D') ||
                      (touche_4x4 == 'V') || (touche_4x4 == 'E'));
      */

      touche_5x4 = obtentionTouche();
      entreeValide = ((touche_5x4 == 'H') || (touche_5x4 == 'B') ||
                      (touche_5x4 == 'V') || (touche_5x4 == 'E'));
    } while (!entreeValide);

    // Traiter le choix de l'opérateur
    /*
      if (touche_4x4 == 'U' && selection > 0) selection--;
      if (touche_4x4 == 'D' && selection < 1) selection++;
      if (touche_4x4 == 'V') return selection;
    */
    if (touche_5x4 == 'H' && selection > 0) selection--;
    if (touche_5x4 == 'B' && selection < 1) selection++;
    if (touche_5x4 == 'V') return selection;
    if (touche_5x4 == 'E') return ABANDON;
  }
  //return ABANDON; // Robustesse
}

// ------------------------------------------------------------------------------------
// MODE MAINTENANCE
// ------------------------------------------------------------------------------------
void modeMaintenance()
{
  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  bool entreeValide = false;
  String message = "";

  while (!JESUS_CHRIST)
  {
    // Afficher le menu
    message = "Offset:" + String(pontTournant.currentPosition())
              + ((digitalRead(capteurOrigineBroche) == LOW) ? " Hall ACTIF" : " Hall libre");
    afficherLigne(message, 1);
    afficherLigne(F("U/D: +/-10 R/L:+/-1") , 2);
    afficherLigne(F("E: Quitter"), 3);

    // Attendre la réponse de l'opérateur
    do {
      /*
        touche_4x4 = clavier4x4.getKey();
        entreeValide = ((touche_4x4 == 'U') ||
                      (touche_4x4 == 'D') ||
                      (touche_4x4 == 'R') ||
                      (touche_4x4 == 'L') ||
                      (touche_4x4 == 'E'));
      */

      touche_5x4 = obtentionTouche();
      entreeValide = ((touche_5x4 == 'H') ||
                      (touche_5x4 == 'B') ||
                      (touche_5x4 == 'D') ||
                      (touche_5x4 == 'G') ||
                      (touche_5x4 == 'E'));
    } while (!entreeValide);

    // Traiter le choix de l'opérateur
    /*
      if (touche_4x4 == 'E') return;
      if (touche_4x4 == 'U') pontTournant.move(10);
      if (touche_4x4 == 'D') pontTournant.move(-10);
      if (touche_4x4 == 'R') pontTournant.move(1);
      if (touche_4x4 == 'L') pontTournant.move(-1);
    */

    if (touche_5x4 == 'E') return;
    if (touche_5x4 == 'H') pontTournant.move(10);
    if (touche_5x4 == 'B') pontTournant.move(-10);
    if (touche_5x4 == 'D') pontTournant.move(1);
    if (touche_5x4 == 'G') pontTournant.move(-1);

    // Déplacer le pont
    pontTournant.runToPosition();
  }
}

// ------------------------------------------------------------------------------------
// MODE CALIBRATION DES VOIES
// ------------------------------------------------------------------------------------
void modeCalibration()
{
  //char touche_4x4 = '\0';
  char touche_5x4 = '\0';
  bool entreeValide = false;
  int voie = voieEntree;
  String message = "";

  while (!JESUS_CHRIST)
  {
    // Afficher le menu
    message = "Voie:" + String(voie) + " Offset:" + String(configPT.tabVoie[voie]);
    afficherLigne(message, 1);
    afficherLigne(F("Voie: R/L Offset:U/D"), 2);
    afficherLigne(F("V:Valider E:Quitter"), 3);


    // Attendre la réponse de l'opérateur
    do {
      /*
        touche_4x4 = clavier4x4.getKey();
        entreeValide = ((touche_4x4 == 'U') ||
                      (touche_4x4 == 'D') ||
                      (touche_4x4 == 'R') ||
                      (touche_4x4 == 'L') ||
                      (touche_4x4 == 'V') ||
                      (touche_4x4 == 'E'));
      */
      touche_5x4 = obtentionTouche();
      entreeValide = ((touche_5x4 == 'H') ||
                      (touche_5x4 == 'B') ||
                      (touche_5x4 == 'D') ||
                      (touche_5x4 == 'G') ||
                      (touche_5x4 == 'V') ||
                      (touche_5x4 == 'E'));

    } while (!entreeValide);

    // Traiter le choix de l'opérateur
    /*
        if (touche_4x4 == 'E') return;
        if (touche_4x4 == 'U') configPT.tabVoie[voie]++;
        if (touche_4x4 == 'D') configPT.tabVoie[voie]--;
        if (touche_4x4 == 'R' && voie < NB_MAX_VOIE) voie++;
        if (touche_4x4 == 'L' && voie > 0)           voie--;
    */
    Serial.println(touche_5x4);
    if (touche_5x4 == 'E') return;
    if (touche_5x4 == 'H') configPT.tabVoie[voie]++;
    if (touche_5x4 == 'B') configPT.tabVoie[voie]--;
    if (touche_5x4 == 'D' && voie < NB_MAX_VOIE) voie++;
    if (touche_5x4 == 'G' && voie > 0)           voie--;

    // Procéder à la calibration
    //if (touche_4x4 == 'V')
    if (touche_5x4 == 'V')
    {
      sauverConfigurationPontTournant();
      afficherMessage(F("Sauvegarde OK"), 3, false, TIMEOUT_SAUVEGARDE);
      return;
    }
  }
}

// ------------------------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(9600);
  while (!Serial);

  if (!mcp1.begin_I2C(I2CADDRmcp1))
  {
    Serial.println(F("Erreur : MCP23017 non trouvé."));
    lcd1.init();
    lcd1.backlight();
    lcd1.setCursor(0, 0);
    lcd1.print(F("MCP23017 non trouve."));
    while (1);
  }

  for (int x = 0; x < ROWS_5x4; x++)          // Configuration des broches MCP23017
  {
    mcp1.pinMode(x, OUTPUT);                  // Lignes (GPA0-3) en SORTIE
    mcp1.digitalWrite(x, HIGH);
  }

  for (int y = 0; y < COLS_5x4; y++)          // Les broches GPB commencent à index 8 dans bibliothèque Adafruit
  {
    mcp1.pinMode(y + 8, INPUT_PULLUP);        // Colonnes (GPB0-4) en ENTREE avec Pull-up activé
  }
  Serial.println(F("Clavier 20 touches prêt !"));

  /*
    #ifdef KEYPAD_I2C
      jwire->begin( );               // Wire.begin( );
      clavier4x4.begin( );           // clavier4x4.begin( makeKeymap(keys) );
    #else
      delay(10);
    #endif
  */

  pinMode(buzzerPin, OUTPUT);
  pinMode(capteurOrigineBroche, INPUT);
  pinMode(BPInitPosition, INPUT);
  pinMode(ledHoming, OUTPUT);
  pinMode(enableStepperPin, OUTPUT);
  pinMode(dirStepperPin, OUTPUT);
  pinMode(stepStepperPin, OUTPUT);
  digitalWrite(enableStepperPin, LOW);

  lcd1.init();
  lcd1.backlight();
  afficherTitre(F("PONT TOURNANT"));
  afficherMessage("   Version: " + String(VERSION), 2, false, TIMEOUT_MSG);

  pontTournant.setMaxSpeed(SPEED_NORMAL);
  pontTournant.setAcceleration(ACCEL_NORMAL);

  chargerEEPROM();
  diagnostic();
  proposerHoming();
}

// ------------------------------------------------------------------------------------
// BOUCLE PRINCIPALE
// ------------------------------------------------------------------------------------
void loop()
{
  affichageToucheClavier();

  int retournementChoisi;
  int voieSelectionnee;
  int manoeuvre = saisirTypeManoeuvre();             // Interroger l'opéraeur

  switch (manoeuvre)                                 // Traiter le choix de l'opérateur
  {
    case ABANDON:
      return;

    case MANOEUVRE_ENTREE:                            // Amener locomotive depuis voie entrée vers voie garage
      afficherTitre(F("== ENTREE =="));

      if (configPT.voieCourante != voieEntree)        // Aller sur voie entrée
      {
        manoeuvrerPontTournant(voieEntree, false);
      }
      voieSelectionnee = saisirNumeroVoie();          // Selectionner voie garage
      if (voieSelectionnee == ABANDON)
      {
        return;
      }
      retournementChoisi = demanderRetournement();    // Demander retournement

      if (retournementChoisi == ABANDON)
      {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);   // Aller sur voie garage
      break;

    case MANOEUVRE_SORTIE:
      afficherTitre(F("== SORTIE =="));           // Amener locomotive voie de garage vers voie entrée
      voieSelectionnee = saisirNumeroVoie();      // Selectionner voie garage

      if (voieSelectionnee == ABANDON)
      {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, false);    // Aller sur voie garage
      retournementChoisi = demanderRetournement();        // Demander retournement

      if (retournementChoisi == ABANDON) {
        return;
      }
      manoeuvrerPontTournant(voieEntree, retournementChoisi);   // Aller sur voie entrée
      break;

    case MANOEUVRE_TRANSFERT:                     // Amener locomotive voie garage vers autre voie garage
      afficherTitre(F("== TRANSFERT =="));
      voieSelectionnee = saisirNumeroVoie();      // Selectionner voie garage source

      if (voieSelectionnee == ABANDON)
      {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);   // Aller sur voie garage source
      voieSelectionnee = saisirNumeroVoie();                          // Selectionner voie garage destination

      if (voieSelectionnee == ABANDON)
      {
        return;
      }
      retournementChoisi = demanderRetournement();        // Demander le retournement

      if (retournementChoisi == ABANDON)
      {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);   // Aller sur voie garage destination
      break;

    case MANOEUVRE_MAINTENANCE:                           // Déplacer PT manuellement par pas unitaire ou de dix
      afficherTitre(F("== MAINTENANCE =="));
      modeMaintenance();
      return;

    case MANOEUVRE_CALIBRATION:                           // Ajuster position de chaque voie et mettre à jour EEPROM
      afficherTitre(F("== CALIBRATION =="));
      modeCalibration();
      return;

    case ARRET:                                            // Arretter proporement le pont
      afficherTitre(F("== ARRET =="));
      sauverConfigurationPontTournant();
      afficherMessage(F("Config. sauvegardee "), 3, false, TIMEOUT_MSG);
      pontTournant.stop();                                 // Arrêter et libérer le moteur à pas

      while (true)                                         // Boucler en attendant un redémarrage
      {
        delay(1000);
      }

    // EXCEPTION
    default:
      exception(F("COMMANDE INCONNUE"));
  }
  afficherMessage(F("Manoeuvre OK"), 3, false, TIMEOUT_MSG);     // Finaliser la manoeuvre
}
