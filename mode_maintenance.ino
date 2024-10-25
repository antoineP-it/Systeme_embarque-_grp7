#include <SPI.h>
#include <SD.h>

const int chipSelect = 4; //la broche 

void mode_maintenance(){
	Serial.println("Entrée en mode maintenance");
    leds.setColorRGB(0, 255, 165, 0);//LED RGB en orange
  	delay(5000);
    if (myFile) {
    // Vérifie si le fichier `myFile` a été correctement ouvert pour l'écriture
        Serial.println("Arrêt de l'écriture, vous pouvez consulter les données de la carte")
        File dataFile = SD.open("message.txt"); 
        // Si le fichier message.txt est ouvert avec succès, on peut commencer à le lire
        
            while (dataFile.available()) {
            // Lis un octet à la fois du fichier et l'envoie au Moniteur Série
                     Serial.write(dataFile.read());
                     
        myFile.close(); // Ferme le fichier en écriture après avoir terminé l'écriture dans `myFile`
        SD.end(); 
        // Arrête proprement l'accès à la carte SD
        digitalWrite(SS, HIGH);
        // Met la broche SS (Chip Select) à HIGH pour désactiver la carte SD
        Serial.println("Vous pouvez désormais changer la carte");
            }
    else 
        
        Serial.println("Erreur d’accès ou d’écriture sur la carte SD")
        while(!red_button){ //tant que le bouton rouge est pas activé ça va clignoter en boucle
             leds.setColorRGB(0, 255, 0, 0); //LED RGB en rouge
             delay(2000);
             leds.setColorRGB(0, 0, 255, 0); //LED RGB en vert
             delay(2000);
        } 

}   
