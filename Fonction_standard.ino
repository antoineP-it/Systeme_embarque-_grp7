#include <ChainableLED.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

#define adresseI2CBME 0x76
int lightsensor = A1;


RTC_DS1307 rtc;
Adafruit_BME280 bme;

const int pinData = 3;
const int pinClock = 4;
ChainableLED leds(pinData, pinClock, 1);


void setup() {
  Serial.begin(9600);
  leds.init();
  while (!Serial);  // Attendre que la communication série soit prête
  if (!rtc.begin()) {
    Serial.println("Erreur de connexion avec le module RTC");
    while (1);  // Bloquer le programme si le RTC n'est pas détecté
  }
  if (!rtc.begin()) {
    Serial.println("Erreur de connexion avec le module RTC");
    while (1);  // Bloquer le programme si le RTC n'est pas détecté
  }
  if (!rtc.isrunning()) {
    // Ajuster l'horloge RTC à l'heure de compilation (ou ajustement manuel)
    rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));  // Ajuster avec la date et heure actuelle
  }
  if (!bme.begin(adresseI2CBME)) {
    Serial.println("Erreur de connexion avec le capteur BME280");
    while (1);  // Bloquer le programme si le capteur BME280 n'est pas détecté
  }

}

void lumiere(int* raw_light, int* light){
  *raw_light = analogRead(lightsensor);
  *light = map(*raw_light, 0, 757, 0, 1023);
  //*light = *raw_light;
}

void lire_temperature(float* temp, float* humi, float* press, float* alt){
  *temp = bme.readTemperature();
  *humi = bme.readHumidity();
  *press = bme.readPressure() / 100.0F;
  *alt = bme.readAltitude(1029);
}

// Fonction qui lit l'heure et la date et les place dans un tableau de caractères
void lire_date_heure(char* buffer) {
  DateTime now = rtc.now();
  // Format : "Date : JJ/MM/AAAA Heure : HH:MM:SS"
  sprintf(buffer, "Date: %02d/%02d/%04d Heure: %02d:%02d:%02d", 
          now.day(), now.month(), now.year(),
          now.hour(), now.minute(), now.second());
}

void standard() {
  Serial.println("Mode standard");
  leds.setColorRGB(0, 0, 255, 0);  // Allume la LED en vert
  char date_heure[40];  // Déclare un tableau de 40 caractères pour stocker la date et l'heure
  lire_date_heure(date_heure);  // Appelle la fonction pour remplir le tableau
  Serial.println(date_heure);  // Affiche la date et l'heure dans le moniteur série
  // pour la température
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
  Serial.println("Light : /1023");
  Serial.println(light);
  }

void loop() {
  standard();
  delay(5000);  // Attente de 5 secondes avant la prochaine exécution
}
