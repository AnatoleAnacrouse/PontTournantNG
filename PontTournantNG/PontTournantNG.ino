// ====================================================================================
//                         PONT TOURNANT NOUVELLE GÉNÉRATION
// ====================================================================================
#define VERSION   "V0.7"
// Auteur  : M. EPARDEAU et F. FRANKE
// Date    : 2 avril 2026
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
#define SPEED_NORMAL     50
#define ACCEL_NORMAL     10
#define SPEED_HOMING     20
#define ACCEL_HOMING      5

// ------------------------------------------------------------------------------------
// CONSTANTES D'AFFICHAGE
// ------------------------------------------------------------------------------------
#define TIMEOUT_MSG         1500   // durée affichage messages courts (ms)
#define TIMEOUT_ERREUR      3000   // durée affichage erreurs (ms)
#define TIMEOUT_SAUVEGARDE  1000   // durée confirmation sauvegarde (ms)

// ------------------------------------------------------------------------------------
// CONSTANTES DE DUREE MAX DU HOMING
// ------------------------------------------------------------------------------------
#define DELAY_WATCHDOG   10000 // 10 000 ms = 10 secondes

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
// MOTEUR PAS À PAS PILOTE VIA LE DRIVER A4988
// La broche ENA du driver n'est pas connectée à l'Arduino et restera donc à l'état bas.
// Un courant circulera dans les bobines du moteur même à l'arrêt ce qui permettra au
// pont de rester parfaitement aligné face aux voies. Par contre, le driver A4988 et 
// le moteur vont chauffer. Il est donc impératif de régler correctement le courant 
// via le potentiomètre du driver A4988 à la valeur minimale.
// ------------------------------------------------------------------------------------
const int enaStepperPin  = 10;
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
  MANOEUVRE_TRANSFERT   = 2,
  MANOEUVRE_MAINTENANCE = 3,
  MANOEUVRE_CALIBRATION = 4
};

// ------------------------------------------------------------------------------------
// ALERTE SONORE
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
// FONCTIONS DE GESTION DE L'AFFICHEUR LCD
// ------------------------------------------------------------------------------------
void effacerLigne(const byte line) {
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

// ------------------------------------------------------------------------------------
void afficherTitre(String titre) {

  lcd.clear();
  
  // Calculer le nombre d'espaces à gauche pour centrer
  int espacesGauche = (NB_CHAR - titre.length()) / 2;
  if (espacesGauche < 0) espacesGauche = 0; // si titre trop long

  // Construire la ligne avec espaces à gauche + titre + espaces à droite
  String ligne = "";
  for (int i = 0; i < espacesGauche; i++) { 
    ligne += " "; 
  }
  ligne += titre;
  // Compléter avec espaces à droite pour remplir la ligne
  while (ligne.length() < (NB_CHAR)) ligne += " ";

  // Afficher sur la première ligne (ligne 0)
  lcd.setCursor(0, 0);
  lcd.print(ligne);
}

// ------------------------------------------------------------------------------------
void afficherLigne(String msg, byte ligne = 1) {
  effacerLigne(ligne);
  lcd.setCursor(0, ligne);
  lcd.print(msg);
}

// ------------------------------------------------------------------------------------
void afficherMessage(String msg, byte ligne = 1, bool erreur = false, int duree = TIMEOUT_MSG) {
  effacerLigne(ligne);
  lcd.setCursor(0, ligne);
  lcd.print(msg);
  delay(duree);
  beep(erreur);
}

// ------------------------------------------------------------------------------------
// GESTION DES EXCEPTIONS
// ------------------------------------------------------------------------------------
void exception(String message) {
  afficherTitre("ERREUR IRRECOUVRABLE");
  afficherMessage(message, 1, true, TIMEOUT_ERREUR);
  beep(true);
  // Arrêter le moteur pour sécurité
  pontTournant.stop();
  // Libérer le moteur
  //digitalWrite(enaStepperPin, HIGHT);
  // Boucle infinie ou attente intervention
  while(true) { delay(1000); }
}

// ------------------------------------------------------------------------------------
// SAUVEGARDE DE LA VOIE COURANTE EN EEPROM
// ATTENTION : l'EEPROM est limitée à 100 000 écritures. On ne sauvegarde la voie
//             courante que si la valeur a changée.
// ------------------------------------------------------------------------------------
void sauvegarderVoieCourante() {

  int voieEE;

  // Lire la voie courante en EEPROM
  EEPROM.get(EEPROM_ADDR_VOIE_COURANTE, voieEE);

  // Si la voie courante est différente de la voie en EEPROM
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
  // Après une calibration, il vne faudrait réinitialiser la voie courante à voieEntree
  // à chaque sauvegarde, car cela écrase la position actuelle.
  //configPT.voieCourante = voieEntree; 
  memcpy(configPT.tabVoie, tabVoie, sizeof(tabVoie));
  EEPROM.put(EEPROM_ADDR_VOIES, configPT);
}

// ------------------------------------------------------------------------------------
// Gere en EEPROM la configuration du pont (tabVoie et voieCourante)
// et vérifie que ces données sont presentes et valides.
// Un magic byte permet de determiner le type d'un fichier (ici la présence)
// ------------------------------------------------------------------------------------
void chargerEEPROM() {

  // Lire la configuration en EEPROM
  EEPROM.get(EEPROM_ADDR_VOIES, configPT);

  // Si la configuration n'est pas présente ou est incorrecte
  if (configPT.magic != EEPROM_MAGIC_VALUE) {
    afficherTitre("Init EEPROM...");
    // Initialiser la configuration dans l'EEPROM
    sauverConfigurationPontTournant();
    // Afficher un message de confirmation sur l'écran LCD
    afficherMessage("Valeurs defaut OK", 3, false, TIMEOUT_MSG);
    return; 
  }
  
  // Vérifier la configuration en EEPROM
  afficherTitre("Verif EEPROM...");

  // Vérifier que les données des voies sont dans la plage autorisée
  bool valide = true;
  for (int i = 0; i <= NB_MAX_VOIE; i++) {
    if ((configPT.tabVoie[i] < 0) || (configPT.tabVoie[i] > stepsPerRevolution)) {
      valide = false; 
      break;
    }
  }

  // Vérifier que la voie courante est valide
  if (valide) { 
    if ((configPT.voieCourante  < 0) || (configPT.voieCourante > NB_MAX_VOIE)) {
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
// GESTION DE LA BARRE DE PROGRESSION SUR L'AFFICHEUR LCD
// ------------------------------------------------------------------------------------
inline void afficherProgression(int progress) __attribute__((always_inline));

inline void afficherProgression(int progress) {

  // Ecréter entre 0.0 et 1.0
  if (progress < 0) progress = 0;
  if (progress > 100) progress = 100;

  // Calculer le nombre de blocks à afficher
  int blocs = int((progress/100.0) * NB_CHAR);

  // Afficher les blocks et effacer le reste de la ligne
  lcd.setCursor(0, 2);
  for (int i = 0; i < NB_CHAR; i++) {
    lcd.print(i < blocs ? char(255) : ' ');
  }

  // Afficher le % de progression
  lcd.setCursor(0, 3);
  lcd.print(progress);
  lcd.print("%   ");
}

// ------------------------------------------------------------------------------------
// DIAGNOSTIC
// ------------------------------------------------------------------------------------
void diagnostic() {

  char touche = '\0';
  bool entreeValide = false;

  afficherTitre("== DIAGNOSTIC == ");

 // Afficher les infos de diagnostic
 String message = "Hall:" + String((digitalRead(hallPin) == LOW ? "ACTIF" : "Libre"))
                + " Ver:" + String(VERSION);
  afficherLigne(message, 1);
  message = "Pos:" + String(pontTournant.currentPosition())
          + " Voie:"
          + (configPT.voieCourante == 0 ? "entree" : String(configPT.voieCourante));
  afficherLigne(message, 2);
  afficherLigne("V ou E: Quitter", 3);

  // Attendre la réponse de l'opérateur
  do {
    touche = keypad.getKey();
    entreeValide = ((touche == 'V') || (touche == 'E'));
  } while (!entreeValide);
}

// ------------------------------------------------------------------------------------
// HOMING
// ------------------------------------------------------------------------------------
void homing() {

  afficherLigne("Recherche origine...", 2);
  effacerLigne(3);

  // Réduire vitesse et accélération
  pontTournant.setMaxSpeed(SPEED_HOMING);
  pontTournant.setAcceleration(ACCEL_HOMING);

  // Lancer 3 révolutions du moteur pour garantir que le capteur est bien trouvé
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

  // Si la voie est detectée
  if (digitalRead(hallPin) == LOW) {
    // Définir la position de la voie d'entrée
    pontTournant.setCurrentPosition(0);
    configPT.voieCourante = voieEntree;
    // Et sauvegarder la configuration en EEPROM
    sauvegarderVoieCourante();
    // Informer l'opérateur du succès 
    afficherMessage("Origine OK", 3, false, TIMEOUT_MSG);
    //afficherLigne("Origine OK", 3);
  }
  else { // Informer l'opérateur de l'echec 
    afficherMessage("ECHEC: origine NOK  ", 3, true, TIMEOUT_ERREUR);
  }
  delay(2*TIMEOUT_MSG);

  // Restituer vitesse et accélération
  pontTournant.setMaxSpeed(SPEED_NORMAL);
  pontTournant.setAcceleration(ACCEL_NORMAL);
}

// ------------------------------------------------------------------------------------
// HOMING OPTIONNEL AU DÉMARRAGE
// ------------------------------------------------------------------------------------
void proposerHoming() {

  // Afficher le menu de homing
  afficherTitre("== HOMING == ");
  afficherLigne("V: Oui E: Non", 3);

  // Attendre la réponse de l'opérateur
  char touche = '\0';
  while (touche != 'V' && touche != 'E') {
    touche = keypad.getKey();
  }

  // Lancer le homing
  if (touche == 'V') { homing(); }
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
// La librairie AccelStepper calcul un nombre de pas absolu positif ou négatif
// selon le sens de rotation. Par exemple, la position courante du moteur sera égale 
// à 800 // après deux révolutions d'un moteur à 400 pas par révolution. 
// ------------------------------------------------------------------------------------
long normaliserPosition() {

  // Calculer la position dans l'intervalle [0..stepsPerRevolution]
  long position = pontTournant.currentPosition() % stepsPerRevolution;

  // Si la position est négative calculer la position positive équivalente
  if (position < 0) {
    position += stepsPerRevolution;
  }

  // Reconfigurer le moteur
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
  if (abs(distance) > (stepsPerRevolution / 2)) {
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
  float distance = abs(cible - depart);
  int distanceParcourue = 0;

  // Ne rien faire si le pont est déjà en position
  if (distance == 0.0) {
    afficherProgression(100);
    return;
  }

  // Déplacer le pont en affichant la progression
  pontTournant.moveTo(cible);
  while (pontTournant.distanceToGo() != 0) {
    pontTournant.run();
    // Calculer le reste à faire et n'afficher qu'une fois sur dix
    distanceParcourue = abs(pontTournant.currentPosition() - depart);
    if  ((distanceParcourue%10)==0) {
       afficherProgression(int((distanceParcourue / distance) *100));
    }  
  } 
  afficherProgression(100);
}

// ------------------------------------------------------------------------------------
// REALISER UNE MANOEUVRE D'ENTREE OU DE SORTIE DU PONT TOURNANT
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
  //sauvegarderVoieCourante();

  return OK;
}

// ------------------------------------------------------------------------------------
// GESTION DU MENU PRINCIPAL
// ------------------------------------------------------------------------------------
int saisirTypeManoeuvre() {

  const int nbManoeuvres = 5;
  const char* manoeuvres[] = {
    "Entree", "Sortie", "Transfert", "Maintenance", "Calibration" 
  };

  char touche = '\0';
  bool entreeValide = false;

  int startIndex = 0;             // Index du premier item affiché à l'écran
  int selection = 0;              // Index de l'élément selectionné
  int selectionGlobale = 0;       // Index global de la sélection dans le tableau
  int selectionPrecedente = -1;   // Forcer le premier affichage
  int startIndexPrecedent = -1;

  lcd.clear();

  while (!JESUS_CHRIST) {

    // Affichage seulement si la sélection ou la fenêtre a changé
    if (selection != selectionPrecedente || startIndex != startIndexPrecedent) {
      for (int i = 0; i < NB_LIGNE; i++) {
        int itemIndex = startIndex + i;
        // Construire une ligne fixe de 20 caractères
        char ligne[NB_CHAR + 1];
        memset(ligne, ' ', NB_CHAR);
        ligne[NB_CHAR] = '\0';

        // Si la ligne est selectionnée alors afficher le curseur
        if (itemIndex < nbManoeuvres) {
          ligne[0] = (i == selection) ? '>' : ' ';
          ligne[1] = ' ';
          // Ecrire le Label de l'item depuis la colonne 2, max 18 chars
          const char* label = manoeuvres[itemIndex];
          int len = strlen(label);
          if (len > 18) len = 18;
          memcpy(&ligne[2], label, len);
        }
        else {
          // Ligne vide si pas d'item
          memset(ligne, ' ', NB_CHAR);
          ligne[NB_CHAR] = '\0';
        }
        // Afficher la ligne sur l'écran LCD
        lcd.setCursor(0, i);
        lcd.print(ligne);
      }
      // Etablir la "fenetre" du menu
      selectionPrecedente = selection;
      startIndexPrecedent = startIndex;
    }

    // Attente d'une touche valide
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

    // Gestion des touches
    if (touche == 'U') {
      if (selection == 0) {
        // Si on est en haut de la fenêtre et qu'on peut remonter dans la liste
        if (startIndex > 0) {
          startIndex--;
        }
      } else {
        selection--;
      }
    }
    else if (touche == 'D') {
      if (selection == 3) {
        // Si on est en bas de la fenêtre et qu'on peut descendre dans la liste
        if ((startIndex + 4) < nbManoeuvres) {
          startIndex++;
        }
      } else {
        // Vérifier qu'on ne dépasse pas le nombre total d'items
        if ((startIndex + selection + 1) < nbManoeuvres) {
          selection++;
        }
      }
    }
    else if (touche == 'V') {
      // Calcul de l'index global sélectionné
      selectionGlobale = startIndex + selection;
      return selectionGlobale;
    }
    else if (touche == 'E') {
      return ABANDON;
    }
  }
}

// ------------------------------------------------------------------------------------
// SAISIE NUMÉRO DE VOIE
// ------------------------------------------------------------------------------------
int saisirNumeroVoie() {

  char touche = '\0';
  String saisie = "";

  // Afficher le message de saisie de la voie
  effacerLigne(2);
  effacerLigne(3);
  afficherLigne("Voie (1-" + String (NB_MAX_VOIE) + "):",1);
  lcd.setCursor(0, 2); 
  lcd.print("> ");

  while (!JESUS_CHRIST) {

    // Attendre la saisie de l'opérateur
    touche = keypad.getKey();
    if (!touche) continue;

    // Si c'est un chiffre de 0 à 9
    if ((touche >= '0' && touche <= '9') && (saisie.length() < 2)) {
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
      // Boucler si rien n'est saisie
      if (saisie.length() == 0) { 
        beep(); 
        continue; 
      }

      // Si le numéro de voie n'est pas conforme alors boucler
      // ===> TRAITER LA VOIE DE GARAGE = 0 avec if (voie < 0 ?????
      int voie = saisie.toInt();
      if (voie < 1 || voie > NB_MAX_VOIE) {
        String message = "Voie invalide (1-" + String(NB_MAX_VOIE) + ")";
        //String message = "ERREUR" + String(NB_MAX_VOIE);
        afficherMessage(message, 2, true, TIMEOUT_ERREUR);
        effacerLigne(2);
        effacerLigne(3);
        saisie = "";
        lcd.setCursor(0, 2); lcd.print("> ");
        continue;
      }
      // Sinon retourner la voie
      return voie;
    }
    // Sinon abandonner
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

  // Afficher le message de saisie du retournement
  effacerLigne(2);
  effacerLigne(3);
  afficherLigne("Retournement:       ", 1);

  while (!JESUS_CHRIST) {

    // Afficher le choix Non ou Oui
    for (int i = 0; i < 2; i++) {
      lcd.setCursor(0, i + 2);
      lcd.print(i == selection ? "> " : "  ");
      lcd.print(ouiNon[i]);
    }

    // Attendre la réponse de l'opérateur
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') || (touche == 'D') ||
                      (touche == 'V') || (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix de l'opérateur
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

    // Afficher le menu
    message = "Offset:" + String(pontTournant.currentPosition())
            + ((digitalRead(hallPin) == LOW) ? " Hall ACTIF" : " Hall libre");
    afficherLigne(message, 1);
    afficherLigne("U/D: +/-10 R/L:+/-1" , 2);
    afficherLigne("E: Quitter", 3);      

    // Attendre la réponse de l'opérateur
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix de l'opérateur
    if (touche == 'E') return;
    if (touche == 'U') pontTournant.move(10);
    if (touche == 'D') pontTournant.move(-10);
    if (touche == 'R') pontTournant.move(1);
    if (touche == 'L') pontTournant.move(-1);

    // Déplacer le pont
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
  String message = "";

  while (!JESUS_CHRIST) {

    // Afficher le menu
    message = "Voie:" + String(voie) + " Offset:" + String(configPT.tabVoie[voie]);
    afficherLigne(message, 1);
    afficherLigne("Voie: R/L Offset:U/D", 2);
    afficherLigne("V:Valider E:Quitter", 3);


    // Attendre la réponse de l'opérateur
    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

    // Traiter le choix de l'opérateur
    if (touche == 'E') return;
    if (touche == 'U') configPT.tabVoie[voie]++;
    if (touche == 'D') configPT.tabVoie[voie]--;
    if (touche == 'R' && voie < NB_MAX_VOIE) voie++;
    if (touche == 'L' && voie > 1)           voie--;
    // Procéder à la calibration
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

  pinMode(enaStepperPin, OUTPUT);
  digitalWrite(enaStepperPin, LOW);

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

  // Interroger l'opéraeur
  int manoeuvre = saisirTypeManoeuvre();

  // Traiter le choix de l'opérateur
  switch (manoeuvre) {

    case ABANDON:
      return;

    // Amener une locomotive depuis la voie d’entrée vers une voie du dépôt.
    case MANOEUVRE_ENTREE:
      afficherTitre("== ENTREE ==");

      // Aller sur la voie d'entrée
      if (configPT.voieCourante != voieEntree) {
        manoeuvrerPontTournant(voieEntree, false);
      }

      // Selectionner la voie de garage
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) { return; }

      // Demander le retournement
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) { return; }

      // Aller sur la voie de garage
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);
    break;

    // Amener une locomotive d'une voie du dépôt vers la voie d’entrée
    case MANOEUVRE_SORTIE:
      afficherTitre("== SORTIE ==");

      // Selectionner la voie de garage
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) { return; }

      // Aller sur la voie de garage
      manoeuvrerPontTournant(voieSelectionnee, false);

      // Demander le retournement
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) { return; }

      // Aller sur la voie d'entrée 
      manoeuvrerPontTournant(voieEntree, retournementChoisi);
    break;

    // Amener une locomotive d'une voie du dépôt vers une autre voie du dépôt 
    case MANOEUVRE_TRANSFERT:
      afficherTitre("== TRANSFERT ==");

      // Selectionner la voie de garage source    
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) { return; }

      // Demander le retournement
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) { return; }

      // Aller sur la voie de garage source
      manoeuvrerPontTournant(voieSelectionnee, retournementChoisi);

       // Selectionner la voie de garage destination     
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) { return; }

      // Aller sur la voie de garage destination
      manoeuvrerPontTournant(voieSelectionnee, false);
    break;

    // Déplacer le pont manuellement par pas unitaire ou de dix
    case MANOEUVRE_MAINTENANCE:
      afficherTitre("== MAINTENANCE ==");
      modeMaintenance();
      return;

    // Ajuster la position de chaque voie et mettre à jour l'EEPROM
    case MANOEUVRE_CALIBRATION:
      afficherTitre("== CALIBRATION ==");
      modeCalibration();
      return;

    // EXCEPTION "Cas non prévu"
    default:
      lcd.clear();
      afficherMessage("ERREUR GRAVE", 3, true, TIMEOUT_ERREUR);
      beep(true);
  }

  // Finaliser la manoeuvre
  afficherMessage("Manoeuvre OK", 3, false, TIMEOUT_MSG);
}
