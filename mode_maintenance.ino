#include <SPI.h>
#include <SD.h>

const int chipSelect = 4; //la broche 

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
            root = SD.open("/"); // Ouvre le répertoire racine de la carte SD
            
            // Boucle pour parcourir chaque fichier ou dossier
            while(true){ 
                File entry =  root.openNextFile(); // Ouvre le prochain fichier du répertoire
                if (! entry) { 
                  break; // plus de fichier
                }
                Serial.println(entry.name()); // Affiche le nom du fichier ou dossier sur le moniteur de série
                entry.close(); // Fermeture des fichiers ou dossiers
                }
            root.close(); // Ferme le répertoire racine
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

