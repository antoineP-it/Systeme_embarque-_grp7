#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "SPI.h"
#include "SD.h"

#define adresseI2CBME 0x76
int lightsensor = A1;

RTC_DS1307 rtc;
Adafruit_BME280 bme;

File myFile;  // Utilisation du type correct pour la manipulation des fichiers SD
const int chipSelect = 4;

const int pinData = 3;
const int pinClock = 4;
ChainableLED leds(pinData, pinClock, 1);

int timeout = 5000;
int log_intervall = 10000;

static unsigned long dernier_enregistrement = 0;
static bool GPS_Actif = true; // Pour alterner l'acquisition GPS
unsigned long log_interval_economique = log_intervall * 2;
unsigned long temps_actuel = millis();

struct GPSData {
    float latitude;
    float longitude;
    float altitude;
};

// Prototypes des fonctions
void erreur_date();
void erreur_temp();
void erreur_SD();
void lire_date_heure(char* buffer);
void lire_temperature(float* temp, float* humi, float* press, float* alt);
void lumiere(int* raw_light, int* light);
void sauvegarde(char* date, float* temp, float* humi, float* press, float *alt, int* lumi);
bool lire_gps(GPSData* gps); // Prototype de lire_gps()
void economique();

void erreur_date() {
    if (!rtc.begin()) {
        Serial.println("Erreur de connexion avec le module RTC");
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
        Serial.println("Erreur de connexion avec le capteur BME280");
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
        Serial.println("Initialisation failed");
        while (!SD.begin(chipSelect)) {
            leds.setColorRGB(0, 255, 0, 0);
            delay(1500);
            leds.setColorRGB(0, 255, 255, 255);
            delay(1500);
        }
        leds.setColorRGB(0, 0, 255, 0);
    }
}

void setup() {
    Serial.begin(9600);
    leds.init();
    while (!Serial);  // Attendre que la communication série soit prête
    erreur_date();
    if (!rtc.isrunning()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Ajuster avec la date et heure actuelle
    }
    erreur_temp();
    pinMode(SS, OUTPUT);
    erreur_SD();
}

void sauvegarde(char* date, float* temp, float* humi, float* press, float* alt, int* lumi) {
    erreur_SD();
    myFile = SD.open("Save.txt", FILE_WRITE);
    if (myFile) {
        Serial.println("Enregistrement sur Save.txt");
        myFile.print("Date : ");
        myFile.print(date);
        myFile.print(" Données : Température : ");
        myFile.print(*temp);
        myFile.print("°C Humidité : ");
        myFile.print(*humi);
        myFile.print("% Pression : ");
        myFile.print(*press);
        myFile.print(" hPa Altitude estimée : ");
        myFile.print(*alt);
        myFile.print(" m Lumière : ");
        myFile.print(*lumi);
        myFile.println(" /1023");
        myFile.close();
        Serial.println("Données enregistrées");
    } else {
        Serial.println("Erreur d'ouverture de Save.txt");
    }
}

void lumiere(int* raw_light, int* light) {
    *raw_light = analogRead(lightsensor);
    *light = map(*raw_light, 0, 757, 0, 1023);
}

void lire_temperature(float* temp, float* humi, float* press, float* alt) {
    erreur_temp();
    *temp = bme.readTemperature();
    *humi = bme.readHumidity();
    *press = bme.readPressure() / 100.0F;
    *alt = bme.readAltitude(1029);
}

void lire_date_heure(char* buffer) {
    erreur_date();
    DateTime now = rtc.now();
    sprintf(buffer, "Date: %02d/%02d/%04d Heure: %02d:%02d:%02d",
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second());
}

void economique() {
    Serial.println("Mode economique");
    leds.setColorRGB(0, 0, 0, 255);  // Allume la LED en bleu
    char date_heure[40];  // Déclare un tableau de 40 caractères pour stocker la date et l'heure
    lire_date_heure(date_heure);  // Appelle la fonction pour remplir le tableau
    Serial.println(date_heure);  // Affiche la date et l'heure dans le moniteur série

    // Déclarer la variable 'gps' de type 'GPSData'
    GPSData gps;
    // Déclarer la variable 'gpsDataAvailable'
    bool gpsDataAvailable = false;

    // Lire le GPS si GPS_Actif est true
    if (GPS_Actif) {
        if (lire_gps(&gps)) { // lire_gps retourne true
            gpsDataAvailable = true;
            // Afficher les données GPS
            Serial.print("Latitude : ");
            Serial.println(gps.latitude, 6);
            Serial.print("Longitude : ");
            Serial.println(gps.longitude, 6);
        } else {
            // Si lire_gps retourne false, ce qui ne devrait pas arriver ici
            Serial.println("Données GPS non capturées cette fois");
        }
    }

    // Inverser GPS_Actif pour la prochaine mesure
    GPS_Actif = !GPS_Actif;

    float temperature;
    float humidity;
    float pressure;
    float altitude;
    lire_temperature(&temperature, &humidity, &pressure, &altitude);

    // Affichage pour le débogage
    Serial.print("Température: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidité: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Pression: ");
    Serial.print(pressure);
    Serial.println(" hPa");

    Serial.print("Altitude: ");
    Serial.print(altitude);
    Serial.println(" m");

    int raw_light;
    int light;
    lumiere(&raw_light, &light);
    Serial.print("Lumière : ");
    Serial.println(light);

    delay(timeout);

    sauvegarde(date_heure, &temperature, &humidity, &pressure, &altitude, &light);

    delay(log_intervall);
}

// Définition de la fonction lire_gps()
bool lire_gps(GPSData* gps) {
    // Simuler les coordonnées GPS du CESI de Saint-Étienne-du-Rouvray
    gps->latitude = 49.382328;
    gps->longitude = 1.101302;
    gps->altitude = 0.0; // Altitude simulée

    // Retourner true pour indiquer que les données GPS sont disponibles
    return true;
}

void loop() {
    economique();
}
