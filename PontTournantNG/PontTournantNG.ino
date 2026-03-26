// ====================================================================================
//                         PONT TOURNANT NOUVELLE GÉNÉRATION
// ====================================================================================
#define VERSION   "V0.3"
// Auteur  : M. EPARDEAU et F. FRANKE
// Date    : 26 mars 2026
// Projet  : Contrôle d’un pont tournant motorisé pour maquette ferroviaire
//           - Permet de déplacer une locomotive entre une voie d’entrée et une voie 
//           du dépôt, avec ou sans retournement, via un pont tournant motorisé.
//           - Une fonction de "homing" permet de rechercher la position de la voie
//           de garage.
//           - Un diagnostique est lancé au démarrage afin de présenter la position 
//           du moteur, la voie courante et l'état capteur Hall
//           - Le mode MAINTENANCE permet de déplacer manuellement le pont par 
//           pas de 1 ou 10.
//           - Un mode CALIBRATION permet d'ajuster précisément la position de 
//            chaque voie et de sauvegarder en EEPROM la configuration.
//           - Les position des voies ainsi que la position courante du pont sont 
//           sauvegardées en EEPROM. Un magic byte (0xA5) permet de vérifier 
//           l'intégrité des données et de les réinitialiser si elles sont corrompues.
// ------------------------------------------------------------------------------------
// CONFIGURATION
// - Arduino ou compatible
// - Moteur pas à pas : 400 pas/tour, piloté via un driver moteur A4988, 
//                      pins Step (12), Dir (11), ENA est laissée libre
// - Capteur Hall     : pin A0 pour détection position zéro (pull-up interne activé)
// - LCD I2C 20 x 4   : adresse 0x27, 20x4, pins SDA (A4), SCL (A5)
// - Clavier 4x4      : lignes (9,8,7,6), colonnes (5,4,3,2)
// - Buzzer           : pin 13
// - EEPROM           : pour la sauvegarde des positions des voies 
//                      et de la voie courante
// ====================================================================================

// ------------------------------------------------------------------------------------
// LIBRAIRIES
// ------------------------------------------------------------------------------------
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <AccelStepper.h>
#include <EEPROM.h>

// ------------------------------------------------------------------------------------
// CONFIGURATION GÉNÉRALE
// ------------------------------------------------------------------------------------
#define JESUS_CHRIST   false
#define OK             0
#define ABANDON       -1

// ------------------------------------------------------------------------------------
// CONSTANTES MOTEUR
// ------------------------------------------------------------------------------------
#define SPEED_NORMAL     700
#define ACCEL_NORMAL     100
#define SPEED_HOMING      50
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
#define DELAY_WATCHDOG    10000

// ------------------------------------------------------------------------------------
// CAPTEUR HALL & BUZZER
// ------------------------------------------------------------------------------------
const int hallPin   = A0;
const int buzzerPin = 13;

// ------------------------------------------------------------------------------------
// LCD I2C 20x4
// ------------------------------------------------------------------------------------
#define NB_LIGNE 4
#define NB_CHAR  20

LiquidCrystal_I2C lcd(0x27, NB_CHAR, NB_LIGNE);

// ------------------------------------------------------------------------------------
// KEYPAD 4x4 
// U : Up / avancer               D : Down / reculer
// L : Left / Déplacer à gauche   R : Right / Déplacer à droite
// V : Valider                    E : Echap / Annuler
// ------------------------------------------------------------------------------------
#define ROWS 4
#define COLS 4

const char kpKeys[ROWS][COLS] = {
  {'1','2','3','U'},
  {'4','5','6','D'},
  {'7','8','9','E'},
  {'L','0','R','V'}
};

byte rowKpPin[ROWS] = {9, 8, 7, 6};
byte colKpPin[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(kpKeys), rowKpPin, colKpPin, ROWS, COLS);

// ------------------------------------------------------------------------------------
// MOTEUR PAS À PAS
// La broche ENA du driver n'est pas connectée à l'Arduino et restera donc à l'état bas.
// Un courant circulera dans les bobines du moteur même à l'arrêt ce qui permettra au
// pont de rester parfaitement aligné face aux voies. Par contre, le driver A4988 et 
// le moteur vont chauffer. Il est donc impératif de l est plus efficace de régler 
// correctement le potentiomètre de courant sur le driver A4988 à la valeur minimale
// afin d'éviter une surchauffe.
// ------------------------------------------------------------------------------------
const int dirStepperPin  = 11;
const int stepStepperPin = 12;

AccelStepper pontTournant(AccelStepper::DRIVER, stepStepperPin, dirStepperPin);

// ------------------------------------------------------------------------------------
// POSITIONS DES VOIES (1..40) — valeurs par défaut
// Chargées / écrasées depuis EEPROM au démarrage
// ------------------------------------------------------------------------------------
#define NB_MAX_VOIE 40

const int tabVoie[NB_MAX_VOIE + 1] = {
    0,
   10,  20,  30,  40,  50,  60,  70,  80,  90, 100,
  110, 120, 130, 140, 150, 160, 170, 180, 190, 200,
  210, 220, 230, 240, 250, 260, 270, 280, 290, 300,
  310, 320, 330, 340, 350, 360, 370, 380, 390, 400
};

const byte voieEntree = 0;
const int stepsPerRevolution = 400;
//int voieCourante = voieEntree;

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

struct ConfigurationPontTournant {
  byte magic;
  int tabVoie[NB_MAX_VOIE + 1];
  int voieCourante;
};

ConfigurationPontTournant configPT;

// ------------------------------------------------------------------------------------
// ENUM MENU PRINCIPAL
// Évite les magic numbers dans loop()
// ------------------------------------------------------------------------------------
enum TypeManoeuvre {
  MANOEUVRE_ENTREE      = 0,
  MANOEUVRE_SORTIE      = 1,
  MANOEUVRE_MAINTENANCE = 2,
  MANOEUVRE_CALIBRATION = 3
};

// ------------------------------------------------------------------------------------
// OUTILS
// ------------------------------------------------------------------------------------
void beep(bool error = false) {
  if (error) {
    //tone(buzzerPin, 500, 400);
    delay(450);
    //tone(buzzerPin, 500, 400);
    delay(450);
  } else {
    //tone(buzzerPin, 1000, 120);
    delay(150);
  }
  //noTone(buzzerPin);
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
void effacerLigne(const byte line) {
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
void afficherLigne(String msg, byte ligne = 1) {
  effacerLigne(ligne);
  lcd.setCursor(0, ligne);
  lcd.print(msg);
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
void afficherMessage(String msg, byte ligne = 1, bool erreur = false, int duree = TIMEOUT_MSG) {
  effacerLigne(ligne);
  lcd.setCursor(0, ligne);
  lcd.print(msg);
  delay(duree);
  beep(erreur);
}

// ------------------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------------------
void afficherTitre(String titre) {

  lcd.clear();
  
  // Calculer le nombre d'espaces à gauche pour centrer
  int espacesGauche = (NB_CHAR - titre.length()) / 2;
  if (espacesGauche < 0) espacesGauche = 0; // si titre trop long

  // Construire la ligne avec espaces à gauche + titre + espaces à droite
  String ligne = "";
  for (int i = 0; i < espacesGauche; i++) ligne += " ";
  ligne += titre;
  // Compléter avec espaces à droite pour remplir la ligne
  while (ligne.length() < (NB_CHAR)) ligne += " ";

  // Afficher sur la première ligne (ligne 0)
  lcd.setCursor(0, 0);
  lcd.print(ligne);
}

// ------------------------------------------------------------------------------------
// SAUVEGARDE DE LA VOIE COURANTE EN EEPROM
// ATTENTION : l'EEPROM est limitée à 100 0000 écritures. On ne sauvegarde la voie
//               courante que si la valeur a changée.
// ------------------------------------------------------------------------------------
void sauvegarderVoieCourante() {
  int voieEE;

  //EEPROM.get(offsetof(ConfigurationPontTournant, configPT.voieCourante), voieEE);
  EEPROM.get(EEPROM_ADDR_VOIE_COURANTE, voieEE);

  // On compare courante à la voie en EEPROM
  if (configPT.voieCourante != voieEE) {
    // On ne sauvegarde que s'il y a une différence
    EEPROM.put(EEPROM_ADDR_VOIE_COURANTE, configPT.voieCourante);                                                      
  }
}

// ------------------------------------------------------------------------------------
// SAUVEGARDE DE LA CONFIGURATION COMPLETE DU PONT EN EEPROM
// ------------------------------------------------------------------------------------
void sauverConfigurationPontTournant() {
  configPT.magic = EEPROM_MAGIC_VALUE;
  configPT.voieCourante = voieEntree;
  memcpy(configPT.tabVoie, tabVoie, sizeof(tabVoie));
  EEPROM.put(EEPROM_ADDR_VOIES, configPT);
}

// ------------------------------------------------------------------------------------
// Gere en EEPROM la configuration du pont (tabVoie et voieCourante)
// et vérifie que ces donnes sont presentes et valides.
// Un magic byte permet de determiner le type d'un fichier
// ------------------------------------------------------------------------------------
void chargerEEPROM() {

  EEPROM.get(EEPROM_ADDR_VOIES, configPT);

  if (configPT.magic != EEPROM_MAGIC_VALUE) {
    afficherTitre("Init EEPROM...");
    // Initialisation par défaut si vierge                                                      
    sauverConfigurationPontTournant();
    // Afficher un message de confirmation sur l'écran LCD
    afficherMessage("Valeurs defaut OK", 3, false, TIMEOUT_MSG);
    return; 
  }
  
  afficherTitre("Verif EEPROM...");
  // Si les données de la configuration sont en EEPROM
  // il faut vérifier que ces données sont dans la plage autorisée [0, stepsPerRevolution]
  bool valide = true;
  for (int i = 0; i <= NB_MAX_VOIE; i++) {
    if ((configPT.tabVoie[i] < 0) || (configPT.tabVoie[i] > stepsPerRevolution)) {
      valide = false; 
      break;
    }
  }
  // Vérifier que la voie courante est valide
  if (valide) { 
    if ((configPT.voieCourante  < 0) || (configPT.voieCourante > stepsPerRevolution)) {
      valide = false; 
    }
  }

  // Si l'EEPROM est corrompue 
  if (!valide) {
    // Prévenir l'opérateur
    afficherMessage("EEPROM invalide !", 3, true, TIMEOUT_ERREUR);
    // Réinitialiser l'EEPROM
    sauverConfigurationPontTournant();
    afficherMessage("EEPROM reinitialisée", 3, true, TIMEOUT_MSG);
  }
}

// ------------------------------------------------------------------------------------
// BARRE DE PROGRESSION LCD
// ------------------------------------------------------------------------------------
void afficherProgression(float progress) {

  // Clamp entre 0.0 et 1.0
  if (progress < 0.0) progress = 0.0;
  if (progress > 1.0) progress = 1.0;

  int total = 20;
  int blocs = (int)(progress * total);

  lcd.setCursor(0, 2);
  for (int i = 0; i < total; i++) {
    lcd.print(i < blocs ? char(255) : ' ');
  }

  lcd.setCursor(0, 3);
  lcd.print((int)(progress * 100));
  lcd.print("%   ");
}

// ------------------------------------------------------------------------------------
// DIAGNOSTIC
// ------------------------------------------------------------------------------------
void diagnostic() {

  char touche = '\0';
  bool entreeValide = false;

  afficherTitre("== DIAGNOSTIC == ");

 String message = "Hall:" + String((digitalRead(hallPin) == LOW ? "ACTIF" : "Libre"))
                + " Ver:" + String(VERSION);
  afficherLigne(message, 1);
  message = "Pos:" + String(pontTournant.currentPosition())
          + " Voie:"
          + (configPT.voieCourante == 0 ? "garage" : String(configPT.voieCourante));

  afficherLigne(message, 2);
  afficherLigne("V ou E: Quitter", 3);

  do {
    touche = keypad.getKey();
    entreeValide = ((touche == 'V') || (touche == 'E'));
  } while (!entreeValide);
}

// ------------------------------------------------------------------------------------
// HOMING AVEC CAPTEUR HALL
// ------------------------------------------------------------------------------------
void homing() {

  afficherLigne("Recherche origine...", 2);
  // Vitesse et accélérations réduites
  pontTournant.setMaxSpeed(SPEED_HOMING);
  pontTournant.setAcceleration(ACCEL_HOMING);

  // Planifier 3 révolutions du moteur pour garantir que le capteur est bien trouvé
  pontTournant.moveTo(pontTournant.currentPosition() + 3 * stepsPerRevolution);

  // Armer un watchdog pour éviter une boucle infinie
  unsigned long start = millis();

  // Arreter la rotation du moteur si le capteur Hall est détecté
  // ou si le nombre de révolutions est atteint
  while ((digitalRead(hallPin) == HIGH) && (pontTournant.distanceToGo() > 0))  {
    // Sortir de la boucle si le watchdog est tombé
    if ((millis() - start) > DELAY_WATCHDOG) break;
    pontTournant.run();
  }
  pontTournant.stop();
  delay(200);

  // Si la voie est detectée
  if (digitalRead(hallPin) == LOW) {
    // Définir la position de la voie d'entrée
    pontTournant.setCurrentPosition(0);
    configPT.voieCourante = voieEntree;
    sauvegarderVoieCourante();
    // Informer l'opérateur du succès 
    afficherLigne("Origine OK", 3);
  }
  else {
    // Informer l'opérateur d'l'echec 
    afficherLigne("ECHEC: origine NOK ", 3);
  }

  delay(2*TIMEOUT_MSG);
  // Vitesse et accélérations réduites
  pontTournant.setMaxSpeed(SPEED_NORMAL);
  pontTournant.setAcceleration(ACCEL_NORMAL);
}

// ------------------------------------------------------------------------------------
// HOMING OPTIONNEL AU DÉMARRAGE
// ------------------------------------------------------------------------------------
void proposerHoming() {

  afficherTitre("== HOMING == ");
  afficherLigne("V: Oui E: Non", 3);
  //lcd.print("Derniere voie: ");
  //lcd.print(configPT.voieCourante);

  char touche = '\0';
  while (touche != 'V' && touche != 'E') {
    touche = keypad.getKey();
  }

  if (touche == 'V') homing();
}

// ------------------------------------------------------------------------------------
// VOIE OPPOSÉE (retournement)
// Retourne la voie opposée à la voie en paramètre.
// Cette fonction ne fonctionne pas si le pont n'est pas symétrique.
// ------------------------------------------------------------------------------------
int voieOpposee(int voie) {
  return ((NB_MAX_VOIE / 2 + voie - 1) % NB_MAX_VOIE) + 1;
}

// ------------------------------------------------------------------------------------
// NORMALISATION POSITION MOTEUR
// La librairie AccelStepper calcul un nombre de pas absolue
// Si par exemple je fais deux tours d'un moteur 40 pas
// la position courante du moteur sera égale à 80
// Le calcul du modulo parmet de ramener la position
// dans l'intervalle [10..stepsPerRevolution]
// ------------------------------------------------------------------------------------
long normaliserPosition() {

  long position = pontTournant.currentPosition() % stepsPerRevolution;

  if (position < 0) {
    position += stepsPerRevolution;
  }

  pontTournant.setCurrentPosition(position);
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
long calculerPlusCourtChemin(long posActuelle, long posCible) {

  // Calculer la distance entre la position actuelle et la position cible
  long distance = posCible - posActuelle;

  // Si la distance absolue est supérieure à la moitié d'un tour complet
  if (abs(distance) > stepsPerRevolution / 2) {
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
void deplacerPontTournant(long cible) {

  // Définir la position de départ et le nombre de pas
  long depart = pontTournant.currentPosition();
  long distanceTotale = abs(cible - depart);

  // Rien à faire si le pont est déjà en position
  if (distanceTotale == 0) {
    afficherProgression(1.0);
    return;
  }

  // Déplacer le pont en affichant la progression
  pontTournant.moveTo(cible);
  while (pontTournant.distanceToGo() != 0) {
    pontTournant.run();
    long distanceParcourue = abs(pontTournant.currentPosition() - depart);
    float progress = (float)distanceParcourue / (float)distanceTotale;
    afficherProgression(progress);
  }
  afficherProgression(1.0);
}

// ------------------------------------------------------------------------------------
// REALISER UNE MENOEUVRE D'ENTREE OU DE SORTIE DU PONT TOURNANT
// ------------------------------------------------------------------------------------
int manoeuvrerPontTournant(const int versVoie, const int retournement) {

  int voieCible = versVoie;

  // Si retournement alors choisir la voie d'en face
  if (retournement) voieCible = voieOpposee(voieCible);

  // Si on est deja sur la voie  alors ne rien faire
  if (voieCible == configPT.voieCourante) return OK;

  // Mettre à jour l'affichage
  afficherLigne("Rotation...", 1);

  // Normaliser si necessaire
  int posActuelle = normaliserPosition();

  // Optimiser le trajet du pont roulant
  int distance = calculerPlusCourtChemin(posActuelle, configPT.tabVoie[voieCible]);

  // Deplacer le pont
  deplacerPontTournant(posActuelle + distance);

  // Remettre a jour la configuration
  configPT.voieCourante = voieCible;
  sauvegarderVoieCourante();

  return OK;
}

// ------------------------------------------------------------------------------------
// GESTION DU MENU PRINCIPAL
// ------------------------------------------------------------------------------------
int saisirTypeManoeuvre() {

  const char* manoeuvres[] = {
    "Entree", "Sortie", "Maintenance", "Calibration" 
  };
  const int nbManoeuvres = 4;
  char touche = '\0';
  bool entreeValide = false;
  byte selection = 0;

  lcd.clear();
  int selectionPrecedente = -1;  // forcer le premier affichage

  while (!JESUS_CHRIST) {

    if (selection != selectionPrecedente) {
      for (int i = 0; i < nbManoeuvres; i++) {

        // Construire une ligne fixe de 20 caractères (pas de débordement)
        char ligne[21];
        memset(ligne, ' ', 20);
        ligne[20] = '\0';

        // Indicateur sélection sur les 2 premières colonnes
        ligne[0] = (i == selection) ? '>' : ' ';
        ligne[1] = ' ';

        // Label à partir de la colonne 2, max 18 chars
        const char* label = manoeuvres[i];
        int len = strlen(label);
        if (len > 18) len = 18;
        memcpy(&ligne[2], label, len);
//
        lcd.setCursor(0, i);
        lcd.print(ligne);
      }
      selectionPrecedente = selection;
    }

    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

    if (touche == 'U' && selection > 0) selection--;
    if (touche == 'D' && selection < nbManoeuvres - 1) selection++;
    if (touche == 'V') return selection;
    if (touche == 'E') return ABANDON;

    delay(100);
  }
}

// ------------------------------------------------------------------------------------
// SAISIE NUMÉRO DE VOIE
// ------------------------------------------------------------------------------------
int saisirNumeroVoie() {

  char touche = '\0';
  String saisie = "";

  effacerLigne(2);
  effacerLigne(3);
  afficherMessage("Voie (1-" + String (NB_MAX_VOIE) + "):",1);
  lcd.setCursor(0, 2); lcd.print("> ");

  while (!JESUS_CHRIST) {

    // Attendre la saisie
    touche = keypad.getKey();
    if (!touche) continue;

    // Si c'est un chiffre de 0 à 9
    if ((touche >= '0' && touche <= '9') && saisie.length() < 2) {
      saisie += touche;
      lcd.setCursor(2, 2);
      lcd.print("   ");
      lcd.setCursor(2, 2);
      lcd.print(saisie);
    }

    // Sinon si c'est la touche backspace
    else if (touche == 'L' && saisie.length() > 0) {
      saisie.remove(saisie.length() - 1);
      lcd.setCursor(2, 2);
      lcd.print("   ");
      lcd.setCursor(2, 2);
      lcd.print(saisie);
    }

    // Sinon si c'est une validation
    else if (touche == 'V') {
      // Si rien n'est saisie on boucle
      if (saisie.length() == 0) { 
        beep(); 
        continue; 
      }

      // On boucle si le numéro de voie n'est pas conforme
      // TRAITER LA VOIE DE GARAGE = 0
      int voie = saisie.toInt();
      if (voie < 1 || voie > NB_MAX_VOIE) {
        String message = "ERREUR" + String(NB_MAX_VOIE);
        afficherMessage(message, 2, true, TIMEOUT_ERREUR);
        effacerLigne(2);
        saisie = "";
        continue;
      }
      // Sinon on retourne la voie
      return voie;
    }
    // Sinon on abandonne
    else if (touche == 'E') return ABANDON;
  }
}

// ------------------------------------------------------------------------------------
// SAISIE RETOURNEMENT
// ------------------------------------------------------------------------------------
int demanderRetournement() {

  const char* ouiNon[] = {"Non", "Oui"};
  char touche = '\0';
  bool entreeValide = false;
  byte selection = 0;

  effacerLigne(2);
  effacerLigne(3);
  afficherMessage("Retournement:", 1);

  while (!JESUS_CHRIST) {

    // Proposer un choix
    for (int i = 0; i < 2; i++) {
      lcd.setCursor(0, i + 2);
      lcd.print(i == selection ? "> " : "  ");
      lcd.print(ouiNon[i]);
    }

    // Gérer la saisie
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') || (touche == 'D') ||
                      (touche == 'V') || (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix
    if (touche == 'U' && selection > 0) selection--;
    if (touche == 'D' && selection < 1) selection++;
    if (touche == 'V') return selection;
    if (touche == 'E') return ABANDON;
  }
}

// ------------------------------------------------------------------------------------
// MODE MAINTENANCE
// ------------------------------------------------------------------------------------
void modeMaintenance() {

  char touche = '\0';
  bool entreeValide = false;
  String message = "";

  while (!JESUS_CHRIST) {

    // Affichier le menu
    message = "Offset:" + String(pontTournant.currentPosition())
            + ((digitalRead(hallPin) == LOW) ? " Hall ACTIF" : " Hall libre");
    afficherLigne(message, 1);

    afficherLigne("U/D: +/-10 R/L:+/-1" , 2);
    afficherLigne("E: Quitter", 3);      

    // Gérer la saisie
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix
    if (touche == 'U') pontTournant.move(10);
    if (touche == 'D') pontTournant.move(-10);
    if (touche == 'R') pontTournant.move(1);
    if (touche == 'L') pontTournant.move(-1);
    if (touche == 'E') return;

    pontTournant.runToPosition();
    //delay(100);
  }
}

// ------------------------------------------------------------------------------------
// MODE CALIBRATION DES VOIES
// ------------------------------------------------------------------------------------
void modeCalibration() {

  char touche = '\0';
  bool entreeValide = false;
  byte voie = voieEntree;
  String saisie = "";

  while (!JESUS_CHRIST) {

    // Affichier le menu
    saisie = "Voie:" + String(voie) + " Offset:" + String(configPT.tabVoie[voie]);
    afficherLigne(saisie, 1);
    afficherLigne("Voie: R/L Offset:U/D", 2);
    afficherLigne("V:Valider E:Quitter", 3);


    // Gérer la saisie
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix
    if (touche == 'U') configPT.tabVoie[voie]++;
    if (touche == 'D') configPT.tabVoie[voie]--;
    if (touche == 'R' && voie < NB_MAX_VOIE) voie++;
    if (touche == 'L' && voie > 1)           voie--;
    if (touche == 'E') return;
    if (touche == 'V') {
      sauverConfigurationPontTournant();
      afficherMessage("Sauvegarde OK", 3, false, TIMEOUT_SAUVEGARDE);
      return;
    }
  }
}
                       
// ------------------------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------------------------
void setup() {

  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  pinMode(hallPin, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  afficherTitre("PONT TOURNANT");
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
void loop() {

  int retournementChoisi;
  int voieSelectionnee;
  int manoeuvre = saisirTypeManoeuvre();

  switch (manoeuvre) {

    case ABANDON:
      return;

    // Amener une locomotive depuis la voie d’entrée vers une voie du dépôt.
    case MANOEUVRE_ENTREE:
      afficherTitre("== ENTREE ==");
      if (configPT.voieCourante != voieEntree) {
        manoeuvrerPontTournant(voieEntree, false);
      }
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);
    break;

    // Amener une locomotive d'une voie du dépôt vers la voie d’entrée
    case MANOEUVRE_SORTIE:
      afficherTitre("== SORTIE ==");
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }
      manoeuvrerPontTournant(voieSelectionnee, false);
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }     
      manoeuvrerPontTournant(voieEntree, retournementChoisi);
    break;

    // Déplacer le pont manuellement par pas unitaire ou de dix
    case MANOEUVRE_MAINTENANCE:
      afficherTitre("== MAINTENANCE ==");
      modeMaintenance();
      return;

    // Ajuster précisément la position de chaque voie
    // et mettre à jour l'EEPROM
    case MANOEUVRE_CALIBRATION:
      afficherTitre("== CALIBRATION ==");
      modeCalibration();
      return;

    default:
      // Cas non prévu 
      lcd.clear();
      afficherMessage("ERREUR GRAVE", 3, true, TIMEOUT_ERREUR);
      return;
  }

  afficherMessage("Manoeuvre OK", 3, false, TIMEOUT_MSG);
}
