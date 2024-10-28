#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include "SPI.h"
#include "SD.h"

// Pour la carte SD
File backup;
File myFile;
const int chipSelect = 4;

//Pour l'horloge RTC
RTC_DS1307 rtc;
Adafruit_BME280 bme;
DateTime now;

// Définition des constantes 
int timeout = 2500;
int log_intervall = 10000;
const long FILE_MAX_SIZE = 2048;  // Limite de taille du fichier (2 Ko)

// Fonction pour créer le nom du fichier au format jour_mois_année_rev.LOG
String generateFileName(int revision) {
  now = rtc.now();
  char fileName[15];
  snprintf(fileName, sizeof(fileName), "%02d%02d%02d%d.log", now.day(), now.month(), now.year() % 100, revision);
  //Serial.println("nom fichier créé");
  return String(fileName);
}

void setup() {
  Serial.begin(9600); // connexion serie 
  
  // Initialisation horloge
  if (!rtc.begin()) {
    Serial.println("Erreur d'initialisation RTC");
    while (1);
  }
  pinMode(SS, OUTPUT);
  erreur_SD();
}

void erreur_SD() {
  if (!SD.begin(chipSelect)) {
    Serial.println("Échec de l'initialisation SD");
    while (!SD.begin(chipSelect)) {
      delay(1500);
    }
  }
}

void sauvegarde(char* date, float* temp, float* humi, float* press, float* alt, int* lumi) {
  erreur_SD();
  Serial.println(generateFileName(0).c_str());
  myFile = SD.open(generateFileName(0).c_str(), FILE_WRITE);  // Ouvrir le fichier révision 0
  Serial.println(myFile);
  if (myFile) {
    Serial.println("Enregistrement dans le fichier révision 0");
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

    //##################Pour gérer le fait que le fichier est trop volumineux #################################
    Serial.println(myFile.size());
    if (myFile.size()> 2048){
      Serial.println("Fichier trop volumineux");
      myFile = SD.open(generateFileName(0).c_str(), FILE_READ);
      String backup_name = generateFileName(1); // création du nouveau fichier avec la révision en 1
      backup = SD.open(backup_name.c_str(), FILE_WRITE); // ouverture du nouveau fichier 
      if (!backup){ // verification de si le fichier backup est ouvert ou non
        Serial.println("Erreur d'ouverture du fichier backup");
      }
      int compteur = myFile.available(); // definit la variable qui va permettre de savoir combien d'octets reste il à copier dans le nouveau fichier
      while (compteur>0) {
        Serial.println(myFile.read()); // Pour voir l'erreur
        if (backup){ // Pour etre sure que le fichier est ouvert
          backup.write(myFile.read());
          Serial.println("ecriture sur le nouveau fichier normalement faite");
        }
        compteur = compteur -1;
        Serial.println(compteur); // Pour voir si le compteur fonctionne bien 
        delay(1000);
      }
      Serial.println("backup faite");
    }
    myFile.close();
    //*******************************************************************************

    //checkAndBackupFile();  // Vérifie la taille après chaque sauvegarde
  } else {
    Serial.println("Erreur d'ouverture du fichier révision 0");
  }

}

void standard() {
  char date_heure[20];
  now = rtc.now();
  // definition de données pour tester la sauvegarde
  snprintf(date_heure, sizeof(date_heure), "%02d/%02d/%02d %02d:%02d", now.year() % 100, now.month(), now.day(), now.hour(), now.minute());
  float temperature = 20.2;
  float humidity = 20.2;
  float pressure = 1013.3;
  float altitude = 232.3;
  int light = 3;
  
  sauvegarde(date_heure, &temperature, &humidity, &pressure, &altitude, &light);
  delay(log_intervall);
}

void loop() {
  standard();
} 
