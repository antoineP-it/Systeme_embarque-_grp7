// Inclusions de bibliothèques
#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
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
File myFile;  // Pour la manipulation des fichiers SD

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

// Variables pour les temporisations des modes
unsigned long previousLogTime = 0;

// Tableau de valeurs par défauts
const int16_t DEFAUT[] PROGMEM = {10, 4096, 30, 1, 255, 768, 1, -10, 60, 1, 0, 50, 1, 850, 1080};

// Prototypes des fonctions
void red_button_ISR();
void green_button_ISR();
void updateMode();
void mode_standard();
void mode_configuration();
void mode_economie();
void mode_maintenance();
void erreur_date();
void erreur_temp();
void erreur_SD();
void lire_date_heure(char* buffer);
void lire_temperature(float* temp, float* humi, float* press, float* alt);
void lumiere(int* raw_light, int* light);
void sauvegarde(const char* date, float temp, float humi, float press, float alt, int lumi);
bool lire_gps(GPSData* gps);
void handleRedButtonPress();
void handleGreenButtonPress();

// Fonction setup
void setup() {
  Serial.begin(9600);
  leds.init();
  pinMode(RED_B, INPUT_PULLUP); // Utilisation des résistances pull-up internes
  pinMode(GREEN_B, INPUT_PULLUP);
  pinMode(SS, OUTPUT); // Pour la carte SD

  Serial.println(F("Initialisation du système..."));

  // Gestion du mode configuration
  delay(500);
  if (digitalRead(RED_B) == LOW) {
    mode_configuration();
  }
  Serial.println("c good");
  delay(300);

  // Initialisation des capteurs et des modules
  erreur_date();
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println(F("RTC ajusté."));
  }
  erreur_temp();
  erreur_SD();

  // Attacher les interruptions
  attachInterrupt(digitalPinToInterrupt(RED_B), red_button_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(GREEN_B), green_button_ISR, FALLING);

  Serial.println(F("Initialisation terminée."));
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
  if (millis() - previousLogTime >= EEPROM.read(0)*1000) {
    previousLogTime = millis();
    leds.setColorRGB(0, 0, 255, 0);  // Allume la LED en vert

    char date_heure[40];
    lire_date_heure(date_heure);
    Serial.println(date_heure);

    // Déclarer les variables localement
    float temperature;
    float humidity;
    float pressure;
    float altitude;
    int raw_light;
    int light;

    lire_temperature(&temperature, &humidity, &pressure, &altitude);
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

    lumiere(&raw_light, &light);
    Serial.print(F("Lumière : "));
    Serial.println(light);
*/
    sauvegarde(date_heure, temperature, humidity, pressure, altitude, light);
  }
}

// Mode économique
void mode_economie() {
  if (millis() - previousLogTime >= EEPROM.read(0)*1000 * 2) {
    previousLogTime = millis();
    leds.setColorRGB(0, 0, 0, 255);  // Allume la LED en bleu

    char date_heure[40];
    lire_date_heure(date_heure);
    Serial.println(date_heure);

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
        Serial.println(F("Données GPS non capturées cette fois"));
      }
    }

    // Inverser GPS_Actif pour la prochaine mesure
    GPS_Actif = !GPS_Actif;

    lire_temperature(&temperature, &humidity, &pressure, &altitude);
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

    lumiere(&raw_light, &light);
    Serial.print(F("Lumière : "));
    Serial.println(light);
*/
    sauvegarde(date_heure, temperature, humidity, pressure, altitude, light);
  }
}

// Mode maintenance
void mode_maintenance(){
  leds.setColorRGB(0, 255, 165, 0); // LED RGB en orange
  delay(5000);

  // Arrêt de l'écriture, vous pouvez consulter les données de la carte
  if (SD.begin(chipSelect)) { // Vérifiez si la carte SD est initialisée
    File dataFile = SD.open("Save.txt");
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
      // Remplacer 'red_button' par 'RED_B'
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




// Fonctions d'erreur
void erreur_date() {
  if (!rtc.begin()) {
    Serial.println(F("Erreur de connexion avec le module RTC"));
    while (!rtc.begin()) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 0, 255);
      delay(1500);
    }
    leds.setColorRGB(0, 0, 255, 0);
  }
}

void erreur_temp() {
  if (!bme.begin(adresseI2CBME)) {
    Serial.println(F("Erreur de connexion avec le capteur BME280"));
    while (!bme.begin(adresseI2CBME)) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 0, 255, 0);
      delay(3000);
    }
    leds.setColorRGB(0, 0, 255, 0);
  }
}

void erreur_SD() {
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Initialisation de la carte SD a échoué"));
    while (!SD.begin(chipSelect)) {
      leds.setColorRGB(0, 255, 0, 0);
      delay(1500);
      leds.setColorRGB(0, 255, 255, 255);
      delay(1500);
    }
    leds.setColorRGB(0, 0, 255, 0);
  }
}

// Gestion des capteurs
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

// Fonction de sauvegarde des données
void sauvegarde(const char* date, float temp, float humi, float press, float alt, int lumi) {
  erreur_SD();
  myFile = SD.open("Save.txt", FILE_WRITE);
  if (myFile) {
    Serial.println(F("Enregistrement sur Save.txt"));
    myFile.print(F("Date : "));
    myFile.print(date);
    myFile.print(F(" Données : Température : "));
    myFile.print(temp);
    myFile.print(F("°C Humidité : "));
    myFile.print(humi);
    myFile.print(F("% Pression : "));
    myFile.print(press);
    myFile.print(F(" hPa Altitude estimée : "));
    myFile.print(alt);
    myFile.print(F(" m Lumière : "));
    myFile.print(lumi);
    myFile.println(F(" /1023"));
    myFile.close();
    Serial.println(F("Données enregistrées"));
  } else {
    Serial.println(F("Erreur d'ouverture de Save.txt"));
  }
}

// Gestion des interruptions
void red_button_ISR() {
  drapeau_bouton_rouge = true;
}

void green_button_ISR() {
  drapeau_bouton_vert = true;
}

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
  Serial.println(command);
  command.trim();
  const char* commande = command.c_str();
	if (strncmp(commande, "LOG_INTERVALL=", 14) == 0){
		int nombre = transformation(commande);
		LOG_INTERVALL(nombre);
	}else if (strncmp(commande, "FILE_MAX_SIZE=", 14) == 0){
		int nombre = transformation(commande);
		FILE_MAX_SIZE(nombre);
	}else if (command == "RESET"){
		RESET();
	}else if (command == "VERSION"){
		VERSION();
	}else if (strncmp(commande, "TIMEOUT=", 8) == 0){
		int nombre = transformation(commande);
		TIMEOUT(nombre);
	}else if (strncmp(commande, "LUMIN=", 6) == 0){
		int nombre = transformation(commande);
		LUMIN(nombre);
	}else if (strncmp(commande, "LUMIN_LOW=", 10) == 0){
		int nombre = transformation(commande);
		LUMIN_LOW(nombre);
	}else if (strncmp(commande, "LUMIN_HIGH=", 11) == 0){
		int nombre = transformation(commande);
		LUMIN_HIGH(nombre);
	}else if (strncmp(commande, "TEMP_AIR=", 9) == 0){
		int nombre = transformation(commande);
		TEMP_AIR(nombre);
	}else if (strncmp(commande, "MIN_TEMP_AIR=", 13) == 0){
		int nombre = transformation(commande);
		MIN_TEMP_AIR(nombre);
	}else if (strncmp(commande, "MAX_TEMP_AIR=", 13) == 0){
		int nombre = transformation(commande);
		MAX_TEMP_AIR(nombre);
	}else if (strncmp(commande, "HYGR=", 5) == 0){
		int nombre = transformation(commande);
		HYGR(nombre);
	}else if (strncmp(commande, "HYGR_MINT=", 10) == 0){
		int nombre = transformation(commande);
		HYGR_MINT(nombre);
	}else if (strncmp(commande, "HYGR_MAXT=", 10) == 0){
		int nombre = transformation(commande);
		HYGR_MAXT(nombre);
	}else if (strncmp(commande, "PRESSURE=", 9) == 0){
		int nombre = transformation(commande);
		PRESSURE(nombre);
	}else if (strncmp(commande, "PRESSURE_MIN=", 13) == 0){
		int nombre = transformation(commande);
		PRESSURE_MIN(nombre);
	}else if (strncmp(commande, "PRESSURE_MAX=", 13) == 0){
		int nombre = transformation(commande);
		PRESSURE_MAX(nombre);
	}else if (strncmp(commande, "CLOCK=", 6) == 0){
		const char* valeur = strtok(commande, "=");
		valeur = strtok(NULL, ":");
    int heure = atoi(valeur);
    valeur = strtok(NULL, ":");
    int minute = atoi(valeur);
    valeur = strtok(NULL, ":");
    int seconde = atoi(valeur);
		CLOCK(heure, minute, seconde);
	}else if (strncmp(commande, "DATE=", 5) == 0){
		const char* valeur = strtok(commande, "=");
		valeur = strtok(NULL, "/");
    int mois = atoi(valeur);
    Serial.println(mois);
    valeur = strtok(NULL, "/");
    int jour = atoi(valeur);
    Serial.println(jour);
    valeur = strtok(NULL, "/");
    int annee = atoi(valeur);
    Serial.println(annee);
    DATE(jour, mois, annee);
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