// Inclusions de bibliothèques
#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <SD.h>

// Définition des constantes
#define adresseI2CBME 0x76
#define lightsensor A1
#define RED_B 2
#define GREEN_B 3
#define DELAI_SANS_REBOND 100
#define pinData 3
#define pinClock 4
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

// Variables pour mesurer la durée d'appui des boutons
unsigned long redPressStartTime = 0;
unsigned long greenPressStartTime = 0;

// Variables pour la gestion des boutons
unsigned long lastRedButtonPress = 0;
unsigned long lastGreenButtonPress = 0;
const unsigned long debounceDelay = 50; // Délai d'anti-rebond

// Variables pour les délais
const unsigned long timeout = 5000;
const unsigned long log_intervall = 10000;

// Variables pour le mode économique
static unsigned long dernier_enregistrement = 0;
static bool GPS_Actif = true; // Pour alterner l'acquisition GPS
const unsigned long log_interval_economique = log_intervall * 2;

// Structure pour les données GPS
struct GPSData {
    float latitude;
    float longitude;
    float altitude;
};

// Variables pour les temporisations des modes
unsigned long previousLogTime = 0;

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
        Serial.println(F("Relâche bouton rouge"));
        Serial.print(F("Durée de l'appui : "));
        Serial.println(pressDuration);

        // Traitement en fonction de la durée de l'appui
        if (pressDuration >= 5000) {
            Serial.println(F("Changement de mode via le bouton rouge"));
            if (mode_std) {
                mode_std = false;
                mode_mtn = true;
                Serial.println(F("Passage en mode maintenance"));
            } else {
                mode_std = true;
                mode_eco = false;
                mode_mtn = false;
                Serial.println(F("Retour en mode standard"));
            }
        }
    }

    // Vérifier si le bouton vert est relâché
    if (digitalRead(GREEN_B) == HIGH && greenPressStartTime != 0) {
        unsigned long pressDuration = currentMillis - greenPressStartTime;
        greenPressStartTime = 0; // Réinitialiser pour la prochaine fois
        Serial.println(F("Relâche bouton vert"));
        Serial.print(F("Durée de l'appui : "));
        Serial.println(pressDuration);

        if (pressDuration >= 5000 && mode_std) {
            mode_std = false;
            mode_eco = true;
            Serial.println(F("Passage en mode économique"));
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
        Serial.println(F("Appui bouton rouge détecté"));
    }
}

// Fonction pour gérer l'appui sur le bouton vert
void handleGreenButtonPress() {
    // Démarrer le chronométrage si ce n'est pas déjà fait
    if (greenPressStartTime == 0) {
        greenPressStartTime = millis();
        Serial.println(F("Appui bouton vert détecté"));
    }
}

// Mode standard
void mode_standard() {
    if (millis() - previousLogTime >= log_intervall) {
        previousLogTime = millis();
        Serial.println(F("Mode standard"));
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

        sauvegarde(date_heure, temperature, humidity, pressure, altitude, light);
    }
}

// Mode économique
void mode_economie() {
    if (millis() - previousLogTime >= log_intervall * 2) {
        previousLogTime = millis();
        Serial.println(F("Mode economique"));
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
                Serial.print(F("Latitude : "));
                Serial.println(gps.latitude, 6);
                Serial.print(F("Longitude : "));
                Serial.println(gps.longitude, 6);
            } else {
                // Si lire_gps retourne false, ce qui ne devrait pas arriver ici
                Serial.println(F("Données GPS non capturées cette fois"));
            }
        }

        // Inverser GPS_Actif pour la prochaine mesure
        GPS_Actif = !GPS_Actif;

        lire_temperature(&temperature, &humidity, &pressure, &altitude);

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

        sauvegarde(date_heure, temperature, humidity, pressure, altitude, light);
    }
}

// Mode maintenance
void mode_maintenance(){
    Serial.println(F("Entrée en mode maintenance"));
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

// Mode configuration
void mode_configuration() {
    Serial.println(F("Entrée en mode configuration"));
    delay(3000); // Si possible, remplacer par un délai non bloquant
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
