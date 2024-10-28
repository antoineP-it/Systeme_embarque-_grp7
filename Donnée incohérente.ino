void donneeIncoherente(float* temp, float* humi, float* press, float* alt, float* lumi){
    
    if(*lumi > 1023 || *lumi < 0){
        Serial.println("Données du capteur de lumière incohérente.")
    }
    if(*humi > 85 || *humi < -40){
        Serial.println("Données du capteur d'humidité incohérente.")
    }   
    if(*temp > 85 || *temp < -40){
        Serial.println("Données du capteur de température incohérente.")
    }
    if(*press > 1100 || *press < 300){
        Serial.println("Données du capteur de pression incohérente.")
    }
}
