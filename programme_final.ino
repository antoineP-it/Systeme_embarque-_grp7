// Inclusions de bibliothèques
#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <SD.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <Wire.h>

// Définition des constantes
#define adresseI2CBME 0x76
#define lightsensor A1
#define RED_B 2
#define GREEN_B 3
#define DELAI_SANS_REBOND 100
#define pinData 4
#define pinClock 5
#define NUM_LEDS 1
#define chipSelect 4
#define SD_CHIP_SELECT_PIN 10 // Utilisé si nécessaire

// Déclarations des objets
RTC_DS1307 rtc;
Adafruit_BME280 bme;
ChainableLED leds(pinData, pinClock, NUM_LEDS);
File myFile;
File backup;
DateTime now;

int revision = 0;
int lastDay = -1;

// Variables pour la gestion des boutons avec anti-rebond
volatile bool drapeau_bouton_rouge = false;
volatile bool drapeau_bouton_vert = false;

// Variables pour les modes
bool mode_std = true;
bool mode_eco = false;
bool mode_mtn = false;
bool actif = true;

// Variables pour mesurer la durée d'appui des boutons
unsigned long redPressStartTime = 0;
unsigned long greenPressStartTime = 0;

// Variables pour la gestion des boutons
unsigned long lastRedButtonPress = 0;
unsigned long lastGreenButtonPress = 0;
const unsigned long debounceDelay = 50; // Délai d'anti-rebond

// Variables pour le mode économique
static unsigned long dernier_enregistrement = 0;
static bool GPS_Actif = true; // Pour alterner l'acquisition GPS

// Structure pour les données GPS
struct GPSData {
  float latitude;
  float longitude;
  float altitude;
};

int t_max = 2048;
int Lumin_Low = 0;
int Lumin_High = 1023;
int Temp_min = -10;
int press_max = 1030;
int press_min = 850;


// Variables pour les temporisations des modes
unsigned long previousLogTime = 0;

// Tableau de valeurs par défauts
const int16_t DEFAUT[15] PROGMEM = {10, 4096, 30, 1, 0, 1023, 1, -10, 60, 1, 0, 50, 1, 850, 1080}; // pour le reset

//Tableaux pour reconnaître les commandes en mode config
typedef void (*Commandes)(int);

const char* const nom_commandes[] PROGMEM = {"LOG_INTERVALL=", "FILE_MAX_SIZE=", "RESET", "VERSION", "TIMEOUT=", "LUMIN=", "LUMIN_LOW=", "LUMIN_HIGH=",
    "TEMP_AIR=", "MIN_TEMP_AIR=", "MAX_TEMP_AIR=", "HYGR=", "HYGR_MINT=", "HYGR_MAXT=", "PRESSURE=", "PRESSURE_MIN=", "PRESSURE_MAX=", "CLOCK=", "DATE="};

const int8_t num_commandes[] PROGMEM = {14, 14, 5, 6, 8, 6, 10, 11, 9, 13, 13, 5, 10, 10, 9, 13, 13, 6, 5};

const Commandes commandes[] PROGMEM = {LOG_INTERVALL, FILE_MAX_SIZE, RESET, VERSION, TIMEOUT, LUMIN, LUMIN_LOW, LUMIN_HIGH,
    TEMP_AIR, MIN_TEMP_AIR, MAX_TEMP_AIR, HYGR, HYGR_MINT, HYGR_MAXT, PRESSURE, PRESSURE_MIN, PRESSURE_MAX};

// Fonction pour gérer la transition entre les modes
void updateMode() {
  unsigned long currentMillis = millis();

  // Vérifier si le bouton rouge est relâché
  if (digitalRead(RED_B) == HIGH && redPressStartTime != 0) {
    unsigned long pressDuration = currentMillis - redPressStartTime;
    redPressStartTime = 0; // Réinitialiser pour la prochaine fois

    // Traitement en fonction de la durée de l'appui
    if (pressDuration >= 5000) {
      if (mode_std) {
        mode_std = false;
        mode_mtn = true;
      } else {
      mode_std = true;
      mode_eco = false;
      mode_mtn = false;
      }
    }
  }

  // Vérifier si le bouton vert est relâché
  if (digitalRead(GREEN_B) == HIGH && greenPressStartTime != 0) {
    unsigned long pressDuration = currentMillis - greenPressStartTime;
    greenPressStartTime = 0; // Réinitialiser pour la prochaine fois

    if (pressDuration >= 5000 && mode_std) {
      mode_std = false;
      mode_eco = true;
    } else if (!mode_std) {
      Serial.println(F("Appuyer sur le bouton rouge pour repasser en mode standard"));
    }
  }

  // Appeler la fonction du mode actuel
  if (mode_std) {
    mode_standard();
  } else if (mode_eco) {
    mode_economie();
  } else if (mode_mtn) {
    mode_maintenance();
  }
}

// Fonction pour gérer l'appui sur le bouton rouge
void handleRedButtonPress() {
  // Démarrer le chronométrage si ce n'est pas déjà fait
  if (redPressStartTime == 0) {
    redPressStartTime = millis();
  }
}

// Fonction pour gérer l'appui sur le bouton vert
void handleGreenButtonPress() {
  // Démarrer le chronométrage si ce n'est pas déjà fait
  if (greenPressStartTime == 0) {
    greenPressStartTime = millis();
  }
}

// Mode standard
void mode_standard() {
  //Serial.println(EEPROM.read(0));
  leds.setColorRGB(0, 0, 255, 0);  // Allume la LED en vert
  //if (millis() - previousLogTime >= EEPROM.read(0)*60000) {
  if (millis() - previousLogTime >= EEPROM.read(0)*1000) {
    previousLogTime = millis();
    char date_heure[40];
    lire_date_heure(date_heure);
    //Serial.println(date_heure);

    // Déclarer les variables localement
    float temperature;
    float humidity;
    float pressure;
    float altitude;
    int raw_light;
    int light;
    lumiere(&raw_light, &light);
    lire_temperature(&temperature, &humidity, &pressure, &altitude);
    donneeIncoherente(&temperature, &humidity, &pressure, &altitude, &light);
/*
    // Affichage pour le débogage
    Serial.print(F("Température: "));
    Serial.print(temperature);
    Serial.println(F(" °C"));

    Serial.print(F("Humidité: "));
    Serial.print(humidity);
    Serial.println(F(" %"));

    Serial.print(F("Pression: "));
    Serial.print(pressure);
    Serial.println(F(" hPa"));

    Serial.print(F("Altitude: "));
    Serial.print(altitude);
    Serial.println(F(" m"));

    
    Serial.print(F("Lumière : "));
    Serial.println(light);
*/
    sauvegarde(date_heure, &temperature, &humidity, &pressure, &altitude, &light);
  }
}

// Mode économique
void mode_economie() {
  if (millis() - previousLogTime >= EEPROM.read(0)*60000 * 2) {
    previousLogTime = millis();
    leds.setColorRGB(0, 0, 0, 255);  // Allume la LED en bleu

    char date_heure[40];
    lire_date_heure(date_heure);
    //Serial.println(date_heure);

    // Déclarer les variables localement
    float temperature;
    float humidity;
    float pressure;
    float altitude;
    int raw_light;
    int light;

    // Déclarer la variable 'gps' de type 'GPSData'
    GPSData gps;
    bool gpsDataAvailable = false;

    // Lire le GPS si GPS_Actif est true
    if (GPS_Actif) {
      if (lire_gps(&gps)) { // lire_gps retourne true
        gpsDataAvailable = true;
        // Afficher les données GPS
        //Serial.print(F("Latitude : "));
        //Serial.println(gps.latitude, 6);
        //Serial.print(F("Longitude : "));
        //Serial.println(gps.longitude, 6);
      } else {
        // Si lire_gps retourne false, ce qui ne devrait pas arriver ici
        //Serial.println(F("Données GPS non capturées cette fois"));
      }
    }

    // Inverser GPS_Actif pour la prochaine mesure
    GPS_Actif = !GPS_Actif;

    lire_temperature(&temperature, &humidity, &pressure, &altitude);
    lumiere(&raw_light, &light);
/*
    // Affichage pour le débogage
    Serial.print(F("Température: "));
    Serial.print(temperature);
    Serial.println(F(" °C"));

    Serial.print(F("Humidité: "));
    Serial.print(humidity);
    Serial.println(F(" %"));

    Serial.print(F("Pression: "));
    Serial.print(pressure);
    Serial.println(F(" hPa"));

    Serial.print(F("Altitude: "));
    Serial.print(altitude);
    Serial.println(F(" m"));

    
    Serial.print(F("Lumière : "));
    Serial.println(light);
*/
    sauvegarde(date_heure, &temperature, &humidity, &pressure, &altitude, &light);
  }
}

// Mode maintenance
void mode_maintenance(){
  leds.setColorRGB(0, 255, 165, 0); // LED RGB en orange
  delay(10000);

  // Arrêt de l'écriture, vous pouvez consulter les données de la carte
  if (SD.begin(chipSelect)) { // Vérifiez si la carte SD est initialisée
    String fileName = generateFileName(revision);
    File dataFile = SD.open(fileName.c_str());
    if (dataFile) {
      Serial.println(F("Arrêt de l'écriture, vous pouvez consulter les données de la carte"));
      while (dataFile.available()) {
        // Lis un octet à la fois du fichier et l'envoie au Moniteur Série
        Serial.write(dataFile.read());
      }
      dataFile.close(); // Ferme le fichier après lecture
      SD.end();
      digitalWrite(SS, HIGH); // Désactive la carte SD
      Serial.println(F("Vous pouvez désormais changer la carte"));
    } else {
      Serial.println(F("Erreur d’accès ou d’écriture sur la carte SD"));
      while (digitalRead(RED_B) == LOW) { // Tant que le bouton rouge est appuyé
        leds.setColorRGB(0, 255, 0, 0); // LED RGB en rouge
        delay(2000);
        leds.setColorRGB(0, 0, 255, 0); // LED RGB en vert
        delay(2000);
      }
    }
  } else {
    Serial.println(F("Erreur d’initialisation de la carte SD en mode maintenance"));
  }
}

// Gestion des erreurs ###################################################################################################################################################################################
void erreur_date() {
  if (!rtc.begin()) {
    //Serial.println(F("Erreur de connexion avec le module RTC"));
    while (!rtc.begin()) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 0, 255);
      delay(1500);
    }
    //leds.setColorRGB(0, 0, 255, 0);
  }
}

void erreur_temp() {
  if (!bme.begin(adresseI2CBME)) {
    //Serial.println(F("Erreur de connexion avec le capteur BME280"));
    while (!bme.begin(adresseI2CBME)) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 255, 0);
      delay(3000);
    }
    //leds.setColorRGB(0, 0, 255, 0);
  }
}

void erreur_SD() {
  if (!SD.begin(chipSelect)) {
    //Serial.println(F("Initialisation de la carte SD a échoué"));
    while (!SD.begin(chipSelect)) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 255, 255, 255);
      delay(1500);
    }
    //leds.setColorRGB(0, 0, 255, 0);
  }
}

// Fonction pour vérifier les données incohérentes
void donneeIncoherente(float* temp, float* humi, float* press, float* alt, int* lumi){
  if(*lumi > Lumin_High || *lumi < Lumin_Low){
    Serial.println(F("Données du capteur de lumière incohérente."));
    while (1) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 255, 0);
      delay(3000);
    }
    //luminosité faible/forte
  }  
  if(*temp > EEPROM.read(16) || *temp < Temp_min){
    Serial.println(F("Données du capteur de température incohérente."));
    while (1) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 255, 0);
      delay(3000);
    }
    //humiditée non prise en compte
  }
  if(*press > press_max || *press < press_min){
    Serial.println(F("Données du capteur de pression incohérente."));
    while (1) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 255, 0);
      delay(3000);
    }
    
  }
}
//Fin gestion des erreurs ###############################################################################################################################################################################

// Gestion des capteurs ################################################################################################################################################################################
void lire_date_heure(char* buffer) {
  erreur_date();
  DateTime now = rtc.now();
  sprintf(buffer, "Date: %02d/%02d/%04d Heure: %02d:%02d:%02d", 
      now.day(), now.month(), now.year(),
      now.hour(), now.minute(), now.second());
}

void lire_temperature(float* temp, float* humi, float* press, float* alt) {
  erreur_temp();
  *temp = bme.readTemperature();
  *humi = bme.readHumidity();
  *press = bme.readPressure() / 100.0F;
  *alt = bme.readAltitude(1029);
}

void lumiere(int* raw_light, int* light) {
  *raw_light = analogRead(lightsensor);
  *light = map(*raw_light, 0, 757, 0, 1023);
}

// Fonction pour lire les données GPS (simulées)
bool lire_gps(GPSData* gps) {
  // Simuler les coordonnées GPS du CESI de Saint-Étienne-du-Rouvray
  gps->latitude = 49.382328;
  gps->longitude = 1.101302;
  gps->altitude = 0.0; // Altitude simulée

  // Retourner true pour indiquer que les données GPS sont disponibles
  return true;
}
// Fin gestion des capteurs ##############################################################################################################################################################################
// gestion sauvegarde ####################################################################################################################################################################################
String generateFileName(int revision) {
  now = rtc.now();
  char fileName[15];
  snprintf(fileName, sizeof(fileName), "%02d%02d%02d%d.log", now.day(), now.month(), now.year() % 100, revision);
  return String(fileName);
}
// Fonction de sauvegarde des données
void sauvegarde(const char* date, float* temp, float* humi, float* press, float* alt, int* lumi) {
  erreur_SD();
  now = rtc.now();
  if (now.day() != lastDay) {
    revision = 0;
    lastDay = now.day();
  }
  String fileName = generateFileName(revision);  
  myFile = SD.open(fileName.c_str(), FILE_WRITE);
  if (myFile) {
    //Serial.println(F("Enregistrement du fichier"));
    //Serial.println(date);
    myFile.print(F("Date : "));
    myFile.print(date);
    myFile.print(F(" Données : Température : "));
    myFile.print(*temp);
    myFile.print(F("°C Humidité : "));
    myFile.print(*humi);
    myFile.print(F("% Pression : "));
    myFile.print(*press);
    myFile.print(F(" hPa Altitude estimée : "));
    myFile.print(*alt);
    myFile.print(F(" m Lumière : "));
    myFile.print(*lumi);
    myFile.println(F(" /1023"));
    myFile.close();
    Serial.println(F("Données enregistrées"));
    myFile = SD.open(fileName.c_str(), FILE_READ);
    //Serial.println(EEPROM.read(2));
    //Serial.println(myFile.size());
    if (myFile && myFile.size() > t_max) {
      Serial.println(F("nouveau fichier"));
      revision++;
    }
    myFile.close();
    
  } else {
    Serial.println(F("Erreur d'ouverture du fichier"));
  }
}
// Fin gestion sauvegarde ###########################################################################################################################################################################################

// Pour le mode configuration  ######################################################################################################################################################################################
void activer_capteur(){
	LUMIN(1);
	TEMP_AIR(1);
	HYGR(1);
	PRESSURE(1);
}
void desactiver_capteur(){
	LUMIN(0);
	TEMP_AIR(0);
	HYGR(0);
	PRESSURE(0);
}

void configurer_parametres(String command){
  command.trim();
  const char* commande = command.c_str();
  for (int8_t i=0; i<15; i++){
    if (strncmp(commande, pgm_read_word(&nom_commandes[i]), pgm_read_word(&num_commandes[i])) == 0){
      if (command[8] == ':'){
        int heure = t1(commande, 1);
        int minute = t1(commande, 2);
        int seconde = t1(commande, 3);
        CLOCK(heure, minute, seconde);
      }else if (command[7] == '/'){
        int mois = t2(commande, 1);
        int jour = t2(commande, 2);
        int annee = t2(commande, 3);
        DATE(mois, jour, annee);
      }else{
		    int nombre = transformation(commande);
		    Commandes cmd = pgm_read_word(&commandes[i]);
        cmd(nombre);
      }
      break;
    }
  }
	actif = false;
}

int transformation(const char* commande){
  const char* valeur = strtok(commande, "=");
	valeur = strtok(NULL, "=");
  return atoi(valeur);
}

void inactif(){
	unsigned long time1 = millis();
	unsigned long time2;
  actif =false; 
	while (!actif){
		time2 = millis();
    //Serial.println(time2 - time1);
		if (time2 - time1 >= 30000){
			return;
		}else if (Serial.available() != 0){
			actif = true;
			configurer_parametres(Serial.readString());
      time1 = millis();
		}
	}
}

void initialisation_EEPROM(){
	for (int i=0; i<15; i++){
		EEPROM.put(i*sizeof(int16_t), pgm_read_word(&DEFAUT[i]));
	}
}

void LOG_INTERVALL(int valeur){
	EEPROM.write(0, valeur);
}

void FILE_MAX_SIZE(int valeur){
	EEPROM.write(2, valeur);
}

void TIMEOUT(int valeur){
	EEPROM.write(4, valeur);
}

void LUMIN(int valeur){
	EEPROM.write(6, valeur);
}

void LUMIN_LOW(int valeur){
	EEPROM.write(8, valeur);
}

void LUMIN_HIGH(int valeur){
	EEPROM.write(10, valeur);
}

void TEMP_AIR(int valeur){
	EEPROM.write(12, valeur);
}

void MIN_TEMP_AIR(int valeur){
	EEPROM.write(14, valeur);
}

void MAX_TEMP_AIR(int valeur){
	EEPROM.write(16, valeur);
}

void HYGR(int valeur){
	EEPROM.write(18, valeur);
}

void HYGR_MINT(int valeur){
	EEPROM.write(20, valeur);
}

void HYGR_MAXT(int valeur){
	EEPROM.write(22, valeur);
}

void PRESSURE(int valeur){
	EEPROM.write(24, valeur);
}

void PRESSURE_MIN(int valeur){
	EEPROM.write(26, valeur);
}

void PRESSURE_MAX(int valeur){
	EEPROM.write(28, valeur);
}

void CLOCK(int heure, int minute, int second){
  DateTime actuel = rtc.now();
  DateTime date_mise_a_jour = DateTime(actuel.year(), actuel.month(), actuel.day(), heure, minute, second);
  rtc.adjust(date_mise_a_jour);
}

void DATE(int mois, int jour, int annee){
  DateTime actuel = rtc.now();
  DateTime date_mise_a_jour = DateTime(annee, mois, jour, actuel.hour(), actuel.minute(), actuel.second());
  rtc.adjust(date_mise_a_jour);
}

void VERSION(){
	Serial.println("Version: 1.0, Numéro de lot: 500");
}

void RESET(){
  initialisation_EEPROM();
}

// Mode configuration
void mode_configuration() {
  leds.setColorRGB(0, 255, 255, 0);
	desactiver_capteur();
  configurer_parametres(Serial.readString());
	inactif();
	activer_capteur();
}
// fin mode config  #################################################################################################################################################################################################

// Gestion des interruptions ########################################################################################################################################################################################
void red_button_ISR() {
  drapeau_bouton_rouge = true;
}
void green_button_ISR() {
  drapeau_bouton_vert = true;
}
// Fin gestion des interruptions ####################################################################################################################################################################################

// Fonction setup
void setup() {
  Serial.begin(9600);
  leds.init();
  pinMode(RED_B, INPUT_PULLUP); // Utilisation des résistances pull-up internes
  pinMode(GREEN_B, INPUT_PULLUP);
  pinMode(SS, OUTPUT); // Pour la carte SD

  //Serial.println(F("Initialisation du système..."));

  // Gestion du mode configuration
  delay(500);
  if (digitalRead(RED_B) == LOW) {
    mode_configuration();
  }
  delay(300);

  // Initialisation des capteurs et des modules
  erreur_date();
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //rtc.adjust(DateTime(2024, 11, 02, 11, 20, 00)); Pour régler l'heure à la main
    //Serial.println(F("RTC ajusté."));
  }
  erreur_temp();
  erreur_SD();

  // Attacher les interruptions
  attachInterrupt(digitalPinToInterrupt(RED_B), red_button_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(GREEN_B), green_button_ISR, FALLING);

  Serial.println(F("Initialisation terminée."));
  lastDay = now.day();
}

// Fonction loop
void loop() {
  // Gestion du bouton rouge
  if (drapeau_bouton_rouge) {
    noInterrupts();
    drapeau_bouton_rouge = false;
    interrupts();

    unsigned long currentMillis = millis();
    if (currentMillis - lastRedButtonPress >= debounceDelay) {
      lastRedButtonPress = currentMillis;
      handleRedButtonPress();
    }
  }

  // Gestion du bouton vert
  if (drapeau_bouton_vert) {
    noInterrupts();
    drapeau_bouton_vert = false;
    interrupts();

    unsigned long currentMillis = millis();
    if (currentMillis - lastGreenButtonPress >= debounceDelay) {
      lastGreenButtonPress = currentMillis;
      handleGreenButtonPress();
    }
  }

  // Mise à jour du mode actif
  updateMode();
}
