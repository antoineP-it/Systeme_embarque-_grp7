#define RED_B 2
#define GREEN_B 3
#define DELAI_SANS_REBOND 100

// Variables pour la gestion des boutons avec anti-rebond
volatile bool drapeau_bouton_rouge = false;
volatile bool drapeau_bouton_vert = false;
volatile unsigned long temps_appui_valide_bouton_r = 0;
volatile unsigned long temps_appui_valide_bouton_g = 0;
const unsigned long intervalle_anti_rebond = 30000; // 30 ms en microsecondes

// Variables pour les modes
bool mode_std = true;
bool mode_eco = false;
bool mode_mtn = false;

// Variables pour mesurer la durée d'appui des boutons
unsigned long redPressStartTime = 0;
unsigned long greenPressStartTime = 0;

// Prototypes des fonctions
void red_button_ISR();
void green_button_ISR();
void updateMode();
void mode_standard();
void mode_configuration();
void mode_economie();
void mode_maintenance();

void setup() {
    Serial.begin(9600);
    pinMode(RED_B, INPUT_PULLUP); // Utilisation des résistances pull-up internes
    pinMode(GREEN_B, INPUT_PULLUP);

    delay(500);
    if (digitalRead(RED_B) == LOW) {
        mode_configuration();
    }
    delay(300);

    // Attacher les interruptions
    attachInterrupt(digitalPinToInterrupt(RED_B), red_button_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(GREEN_B), green_button_ISR, CHANGE);

    mode_std = true;
}

void loop() {
    // Gestion des boutons
    if (drapeau_bouton_rouge) {
        noInterrupts();
        drapeau_bouton_rouge = false;
        interrupts();

        if (digitalRead(RED_B) == LOW) {
            // Bouton rouge pressé
            redPressStartTime = millis();
            Serial.println("Appui bouton rouge");
            delay(3000);
        } else {
            // Bouton rouge relâché
            unsigned long temps_d_appui = millis() - redPressStartTime;
            Serial.println("Relâche bouton rouge");
            Serial.print("Durée de l'appui : ");
            Serial.println(temps_d_appui);

            if (temps_d_appui >= 5000) {
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
    }

    if (drapeau_bouton_vert) {
        noInterrupts();
        drapeau_bouton_vert = false;
        interrupts();

        if (digitalRead(GREEN_B) == LOW) {
            // Bouton vert pressé
            greenPressStartTime = millis();
            Serial.println("Appui bouton vert");
            delay(3000);
        } else {
            // Bouton vert relâché
            unsigned long temps_d_appui = millis() - greenPressStartTime;
            Serial.println("Relâche bouton vert");
            Serial.print("Durée de l'appui : ");
            Serial.println(temps_d_appui);

            if (temps_d_appui >= 5000 && mode_std) {
                mode_std = false;
                mode_eco = true;
            } else if (!mode_std) {
                Serial.println("Appuyer sur le bouton rouge pour repasser en mode standard");
            }
        }
    }

    // Mise à jour du mode
    updateMode();
}

// Fonction pour gérer la transition entre les modes
void updateMode() {
    if (mode_std) {
        mode_standard();
    } else if (mode_eco) {
        mode_economie();
    } else if (mode_mtn) {
        mode_maintenance();
    }
}

// ISR pour le bouton rouge
void red_button_ISR() {
    unsigned long temps_ecoule = micros();
    if (temps_ecoule - temps_appui_valide_bouton_r > intervalle_anti_rebond) {
        temps_appui_valide_bouton_r = temps_ecoule;
        drapeau_bouton_rouge = true;
    }
}

// ISR pour le bouton vert
void green_button_ISR() {
    unsigned long temps_ecoule = micros();
    if (temps_ecoule - temps_appui_valide_bouton_g > intervalle_anti_rebond) {
        temps_appui_valide_bouton_g = temps_ecoule;
        drapeau_bouton_vert = true;
    }
}

// Fonction pour le mode standard
void mode_standard() {
    // Utilisation d'un délai non bloquant
    static unsigned long dernier_temps = 0;
    if (millis() - dernier_temps >= 5000) {
        Serial.println("Entrée en mode standard");
        dernier_temps = millis();
    }
}

// Fonction pour le mode configuration
void mode_configuration() {
    Serial.println("Entrée en mode configuration");
    delay(3000); // Si possible, remplacer par un délai non bloquant
}

// Fonction pour le mode économie
void mode_economie() {
    static unsigned long dernier_temps = 0;
    if (millis() - dernier_temps >= 5000) {
        Serial.println("Entrée en mode économie");
        dernier_temps = millis();
    }
}

// Fonction pour le mode maintenance
void mode_maintenance() {
    static unsigned long dernier_temps = 0;
    if (millis() - dernier_temps >= 5000) {
        Serial.println("Entrée en mode maintenance");
        dernier_temps = millis();
    }
}
