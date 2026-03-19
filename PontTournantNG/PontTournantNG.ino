// ====================================================================================
//                         PONT TOURNANT NOUVELLE GÉNÉRATION
// ====================================================================================
#define VERSION   "V0.1"
// Auteur  : M. EPARDEAU et F. FRANKE
// Date    : 17 mars 2026
// Projet  : Contrôle d’un pont tournant motorisé pour maquette ferroviaire
//           - Permet de déplacer une locomotive entre une voie d’entrée et une voie 
//           du dépôt, avec ou sans retournement, via un pont tournant motorisé.
//           - Une fonction de "homing" permet de rechercher la position de la voie
//           de garage.          
//           - Un diagnostique est lancé au démarrage afin de présenter la position 
//           du moteur, la voie courante et l'état capteur Hall
//           - Le mode MAINTENANCE permet de déplacer manuellement le pont par 
//           pas de 1 ou 10.
//           - Un mode CALIBRATION permet d'ajuster précisement la position de 
//            chaque voie et de sauvegarder en EEPROM la configuration.
//           - Les position des voies ainsie que la position courante du pont sont 
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
#define ACCEL_NORMAL     200
#define SPEED_HOMING      50
#define ACCEL_HOMING     100

// ------------------------------------------------------------------------------------
// CONSTANTES D'AFFICHAGE
// ------------------------------------------------------------------------------------
#define TIMEOUT_MSG        1500   // durée affichage messages courts (ms)
#define TIMEOUT_ERREUR      800   // durée affichage erreurs (ms)
#define TIMEOUT_SAUVEGARDE  800   // durée confirmation sauvegarde (ms)

// ------------------------------------------------------------------------------------
// CAPTEUR HALL & BUZZER
// ------------------------------------------------------------------------------------
const int hallPin   = A0;
const int buzzerPin = 13;

// ------------------------------------------------------------------------------------
// LCD I2C 20x4
// ------------------------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 20, 4);

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
const int stepsPerRevolution = 400;

AccelStepper pontTournant(AccelStepper::DRIVER, stepStepperPin, dirStepperPin);

// ------------------------------------------------------------------------------------
// POSITIONS DES VOIES (1..40) — valeurs par défaut
// Chargées / écrasées depuis EEPROM au démarrage
// ------------------------------------------------------------------------------------
#define NB_MAX_VOIE 40

int tabVoie[NB_MAX_VOIE + 1] = {
    0,
   10,  20,  30,  40,  50,  60,  70,  80,  90, 100,
  110, 120, 130, 140, 150, 160, 170, 180, 190, 200,
  210, 220, 230, 240, 250, 260, 270, 280, 290, 300,
  310, 320, 330, 340, 350, 360, 370, 380, 390, 400
};

const byte voieEntree = 0;
int voieCourante = voieEntree;

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
void beep() {
  tone(buzzerPin, 1000, 120);
  delay(150);
  noTone(buzzerPin);
}

// ------------------------------------------------------------------------------------
void lcdClearLine(byte line) {
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

// ------------------------------------------------------------------------------------
// EEPROM — SAUVEGARDE DE LA VOIE COURANTE
// ATTENTION : l'EEPROM est limitée à 100 0000 écritures. On ne sauvegarde la voie
//               courante que si la valeur a changée.
// ------------------------------------------------------------------------------------
void sauvegarderVoieCourante() {
  
  int voie;
  EEPROM.get(EEPROM_ADDR_VOIE_COURANTE, voie);
  
  if (voieCourante != voie) {
    EEPROM.put(EEPROM_ADDR_VOIE_COURANTE, voieCourante);
    EEPROM.put(EEPROM_MAGIC_ADDR, (byte)EEPROM_MAGIC_VALUE);
  }
}

// ------------------------------------------------------------------------------------
// Gere en EEPROM la configuration du pont (tabVoie et voieCourante)
// et vérifie que ces donnes sont presentes et valides.
// Un magic byte permet de determiner le type d'un fichier
// ------------------------------------------------------------------------------------
void chargerEEPROM() {

  // Détecte si l'EEPROM a déjà été initialisée
  byte magic;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);

  // --- Première utilisation ou EEPROM vierge ---

  // Si l'EEPROM est vierge
  // alors suvegarder les données (tableau des voies et voie courante)
  // puis marquer l'EEPROM comme initialisée (magic byte)
  if (magic != EEPROM_MAGIC_VALUE) {
    EEPROM.put(EEPROM_ADDR_VOIES, tabVoie);
    EEPROM.put(EEPROM_ADDR_VOIE_COURANTE, voieCourante);
    EEPROM.put(EEPROM_MAGIC_ADDR, (byte)EEPROM_MAGIC_VALUE);

    // Afficher un message de confirmation sur l'écran LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Init EEPROM...");
    lcd.setCursor(0, 1);
    lcd.print("Valeurs defaut OK");
    delay(TIMEOUT_MSG);

    // Pas besoin de lire, on vient d'écrire les valeurs
    return;
  }

  // --- Lire et valider tabVoie ---

  // Lire le tableau des voies depuis l'EEPROM 
  // dans un buffer temporaire afin de valider les voies
  int buf[NB_MAX_VOIE + 1];
  EEPROM.get(0, buf);

  // Vérifier que les données sont dans la plage autorisée 
  // [0, stepsPerRevolution]
  bool valide = true;
  for (int i = 1; i <= NB_MAX_VOIE; i++) {
    if (buf[i] < 0 || buf[i] > stepsPerRevolution) {
      valide = false;
      break;
    }
  }

  // Si les donnees ne sont pas corrompues
  // copier le buffer dans le tableau actif
  if (valide) {
    memcpy(tabVoie, buf, sizeof(tabVoie));

  //sinon l'EEPROM est corrompue
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EEPROM invalide !");
    lcd.setCursor(0, 1);
    lcd.print("Valeurs defaut OK");
    delay(2000);
    // Réécrire les valeurs saines pour corriger l'EEPROM
    EEPROM.put(0, tabVoie);
  }

  // --- Charger la voie courante ---

  // Charger voieCourante
  int voie;
  EEPROM.get(EEPROM_ADDR_VOIE_COURANTE, voie);
  if (voie >= 1 && voie <= NB_MAX_VOIE) {
    voieCourante = voie;
  } else {
    voieCourante = voieEntree;
    sauvegarderVoieCourante();
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
void Diagnostic() {

  char touche = '\0';
  bool entreeValide = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  == DIAGNOSTIC ==  ");

  lcd.setCursor(0, 1);
  lcd.print(" Pos:");
  lcd.print(pontTournant.currentPosition());
  lcd.print("  Voie:");
  lcd.print(voieCourante);

  lcd.setCursor(0, 2);
  lcd.print("Hall:");
  lcd.print(digitalRead(hallPin) == LOW ? "ACTIF" : "Libre");
  lcd.print(" Ver:");
  lcd.print(VERSION);

  lcd.setCursor(0, 3);
  lcd.print("V ou E : Quitter");

  do {
    touche = keypad.getKey();
    entreeValide = ((touche == 'V') || (touche == 'E'));
  } while (!entreeValide);
}

// ------------------------------------------------------------------------------------
// HOMING AVEC CAPTEUR HALL
// ------------------------------------------------------------------------------------
void homing() {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Homing capteur Hall");
  lcd.setCursor(0, 1);
  lcd.print("Recherche origine...");

  // Vitesse et accélérations réduites
  pontTournant.setMaxSpeed(SPEED_HOMING);
  pontTournant.setAcceleration(ACCEL_HOMING);

  // Planifier 4 révolution du moteur pour garantir que le capteur est bien trouvé
  pontTournant.moveTo(pontTournant.currentPosition() + 3 * stepsPerRevolution);

  // Arreter la rotation du moteur si le capteur Hall est détecté
  // ou si la position cible est atteinte
  while ((digitalRead(hallPin) == LOW) && (pontTournant.distanceToGo() >0))  {
    pontTournant.run();
  }
  pontTournant.stop();
  delay(200);

  // Si la voie est detectée
  if (digitalRead(hallPin) == HIGH) {
    // Définir la position de la voie d'entrée
    pontTournant.setCurrentPosition(0);
    voieCourante = voieEntree;
    sauvegarderVoieCourante();
    // Informer l'opérateur du succès 
    lcd.setCursor(0, 3);
    lcd.print("Origine OK          ");
  }
  else {
    // Informer l'opérateur d'l'echec 
    lcd.setCursor(0, 3);
    lcd.print("ECHEC : origine NOK ");
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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Faire le homing ?");
  lcd.setCursor(0, 1);
  lcd.print("V : Oui   E : Non");
  lcd.setCursor(0, 3);
  lcd.print("Derniere voie: ");
  lcd.print(voieCourante);

  char touche = '\0';
  while (touche != 'V' && touche != 'E') {
    touche = keypad.getKey();
  }

  if (touche == 'V') homing();
}

// ------------------------------------------------------------------------------------
// VOIE OPPOSÉE (retournement)
// Retourne la voie opposée à la voie en paramètre.
// Cette fonction ne fonctionne pas si le pont n'est pas symétrique
// Cette fonction n'est pas valide si la première voie est la voie 1. Dans ce cas, 
// il faudrait calculer : return ((NB_MAX_VOIE / 2 + voie - 1) % NB_MAX_VOIE) + 1;
// ------------------------------------------------------------------------------------
int voieOpposee(int voie) {
  return ((NB_MAX_VOIE / 2 + voie) % NB_MAX_VOIE);
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
void pontTournantAllerA(long cible) {

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
// DÉPLACEMENT DU PONT TOURNANT
// ------------------------------------------------------------------------------------
int deplacerPT(const int versVoie, const int retournement) {

  int voieCible = versVoie;

  // Si retournement alors choisir la voie d'en face
  if (retournement) voieCible = voieOpposee(voieCible);

  // Si on est deja sur la voie  alors ne rien faire
  if (voieCible == voieCourante) return OK;

  // Mettre à jour l'affichage
  lcdClearLine(3);
  lcd.setCursor(0, 3);
  lcd.print("Rotation...");

  // Normaliser si necessaire
  int posActuelle = normaliserPosition();

  // Optimiser le trajet du pont roulant
  int distance = calculerPlusCourtChemin(posActuelle, tabVoie[voieCible]);

  // Deplacer le pont
  pontTournantAllerA(posActuelle + distance);

  // Remettre a jour la configuration
  voieCourante = voieCible;
  sauvegarderVoieCourante();

  return OK;
}

// ------------------------------------------------------------------------------------
// MENU PRINCIPAL
// ------------------------------------------------------------------------------------
const char* manoeuvres[] = {
  "Entree", "Sortie", "Maintenance", "Calibration" 
};

const int nbManoeuvres = 4;

int saisirTypeManoeuvre() {

  char touche = '\0';
  bool entreeValide = false;
  int selection = 0;

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

  char touche;
  String saisie = "";

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Voie (1-");
  lcd.print(NB_MAX_VOIE);
  lcd.print("):");
  lcd.setCursor(0, 1);
  lcd.print("> ");

  while (!JESUS_CHRIST) {

    touche = keypad.getKey();

    if (!touche) continue;

    if (touche >= '0' && touche <= '9' && saisie.length() < 2) {
      saisie += touche;
      lcd.setCursor(2, 1);
      lcd.print("   ");
      lcd.setCursor(2, 1);
      lcd.print(saisie);
    }

    else if (touche == 'L' && saisie.length() > 0) {
      saisie.remove(saisie.length() - 1);
      lcd.setCursor(2, 1);
      lcd.print("   ");
      lcd.setCursor(2, 1);
      lcd.print(saisie);
    }

    else if (touche == 'V') {
      if (saisie.length() == 0) { 
        beep(); 
        continue; 
      }
      int voie = saisie.toInt();
      if (voie < 1 || voie > NB_MAX_VOIE) {
        beep();
        lcd.setCursor(0, 2);
        lcd.print("Erreur: 1 a ");
        lcd.print(NB_MAX_VOIE);
        delay(TIMEOUT_ERREUR);
        lcdClearLine(2);
        saisie = "";
        continue;
      }
      return voie;
    }

    else if (touche == 'E') return ABANDON;
  }
}

// ------------------------------------------------------------------------------------
// SAISIE RETOURNEMENT
// ------------------------------------------------------------------------------------
const char* ouiNon[] = {"Non", "Oui"};

int demanderRetournement() {

  char touche = '\0';
  bool entreeValide = false;
  int selection = 0;

  while (!JESUS_CHRIST) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Retournement ?");

    for (int i = 0; i < 2; i++) {
      lcd.setCursor(0, i + 1);
      lcd.print(i == selection ? "> " : "  ");
      lcd.print(ouiNon[i]);
    }

    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

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

  while (!JESUS_CHRIST) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("== MAINTENANCE ==");
    lcd.setCursor(0, 1);
    lcd.print("U:+10  D:-10");
    lcd.setCursor(0, 2);
    lcd.print("R:+1   L:-1");
    lcd.setCursor(0, 3);
    lcd.print("E:Quitter");

    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'E'));
    } while (!entreeValide);

    if (touche == 'U') pontTournant.move(10);
    if (touche == 'D') pontTournant.move(-10);
    if (touche == 'R') pontTournant.move(1);
    if (touche == 'L') pontTournant.move(-1);
    if (touche == 'E') return;

    pontTournant.runToPosition();
    delay(300);
  }
}

// ------------------------------------------------------------------------------------
// MODE CALIBRATION DES VOIES
// ------------------------------------------------------------------------------------
void modeCalibration() {

  char touche = '\0';
  bool entreeValide = false;
  int voie = 1;

  while (!JESUS_CHRIST) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" == CALIBRATION ==");

    lcd.setCursor(0, 1);
    lcd.print("Voie ");
    lcd.print(voie);
    lcd.print(" Offset:");
    lcd.print(tabVoie[voie]);
    //lcd.print("    ");

    lcd.setCursor(0, 2);
    lcd.print("Offset:U=+  D=-");

    lcd.setCursor(0, 3);
    lcd.print("Voie:R=+ L=- V=Val");

    do {
      touche = keypad.getKey();
      entreeValide = ((touche == 'U') ||
                      (touche == 'D') ||
                      (touche == 'R') ||
                      (touche == 'L') ||
                      (touche == 'V') ||
                      (touche == 'E'));
    } while (!entreeValide);

    if (touche == 'U') tabVoie[voie]++;
    if (touche == 'D') tabVoie[voie]--;
    if (touche == 'R' && voie < NB_MAX_VOIE) voie++;
    if (touche == 'L' && voie > 1)           voie--;

    if (touche == 'V') {
      EEPROM.put(0, tabVoie);
      EEPROM.put(EEPROM_MAGIC_ADDR, (byte)EEPROM_MAGIC_VALUE);
      lcdClearLine(3);
      lcd.setCursor(0, 3);
      lcd.print("Sauvegarde OK");
      beep();
      delay(TIMEOUT_SAUVEGARDE);
    }

    if (touche == 'E') return;
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
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pont Tournant ");
  lcd.print(VERSION);
  delay(TIMEOUT_MSG);

  pontTournant.setMaxSpeed(SPEED_NORMAL);
  pontTournant.setAcceleration(ACCEL_NORMAL);

  // Chargement EEPROM avec validation
  chargerEEPROM();

  // Diagnostic au démarrage (infos position, capteur Hall, version)
  Diagnostic();

  // Homing optionnel
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
      if (voieCourante != voieEntree) {
        deplacerPT(voieEntree, false);
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

    // Amener une locomotive d'une voie du dépôt vers la voie d’entrée
    case MANOEUVRE_SORTIE:
      voieSelectionnee = saisirNumeroVoie();
      if (voieSelectionnee == ABANDON) {
        return;
      }
      deplacerPT(voieSelectionnee, false);
      retournementChoisi = demanderRetournement();
      if (retournementChoisi == ABANDON) {
        return;
      }     
      deplacerPT(voieEntree, retournementChoisi);
    break;

    // Déplacer le pont manuellement par pas unitaire ou de dix
    case MANOEUVRE_MAINTENANCE:
      modeMaintenance();
      return;

    // Ajuster précisément la position de chaque voie
    // et mettre à jour l'EEPROM
    case MANOEUVRE_CALIBRATION:
      modeCalibration();
      return;

    default:
      // Cas non prévu 
      lcd.clear();
      lcd.print("ERREUR DE TRAITEMENT");
      delay (TIMEOUT_MSG);
      return;
  }

  lcdClearLine(3);
  lcd.setCursor(0, 3);
  lcd.print("Manoeuvre OK");
  delay(TIMEOUT_MSG);
}
