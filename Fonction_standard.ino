#include <ChainableLED.h>
#include <RTClib.h>

RTC_DS1307 rtc;

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
  if (!rtc.isrunning()) {
    // Ajuster l'horloge RTC à l'heure de compilation (ou ajustement manuel)
    rtc.adjust(DateTime(2024, 10, 23, 14, 30, 0));// Ajuster avec la date et heure actuelle
    //rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  }
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
}

void loop() {
  standard();
  delay(5000);  // Attente de 5 secondes avant la prochaine exécution
}
