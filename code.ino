bool mode_standard = false;
bool mode_economique = false;
bool mode_maintenance = false;
bool actif = true;
bool config = false;
entier Log_intervall = 10;
entier Timeout = 30;
entier File_max_size = 2;

void setup{
	Serial.begin(9600); 
  	pinMode(RED_B, INPUT); 
  	pinMode(GREEN_B, INPUT);
	delay(5000);
	if (digitalRead(RED_B)==HIGH){
		mode_configuration();
	}
	delay(3000);
	interruptions();
  	mode_standard = true;
}

void configuration(){
	Serial.println("mode configuration")
	digitalWrite(LED_RGB, HIGH);
	desactiver_capteur();
	configurer_parametres();
	inactif();
	activer_capteur();
}

void activer_capteur(){
	LUMIN(1);
	TEMP_AIR(1);
	HYGT(1);
	PRESSURE(1);
}

void desactiver_capteur(){
	LUMIN(0);
	TEMP_AIR(0);
	HYGT(0);
	PRESSURE(0);
}

void configurer_parametres(){
	string commande;
	int i,n;
	config = !config;
	while (config){
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("1: LOG_INTERVALL=10 (définition de l'intervalle entre 2 mesures, 10 minutes par défaut)")
		Serial.println("2: FILE_MAX_SIZE=4096 (définition de la taille maximale d'un fichier de log, une taille de 4ko provoque son archivage)")
		Serial.println("3: RESET (réinitialisation de l'ensemble des paramètres à leurs valeurs par défaut)")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
		Serial.println("Les différentes commandes de configuration possibles sont:")
	}
}

void inactif(){
	unsigned long time1 = millis();
	unsigned long time2;
	Serial.println("Appuyez sur l'un des bouton si actif");
	while (!actif){
		time2 = millis();
		if (time2 - time1 >= 15000){
			return;
		}else if (digitalRead(RED_B)==HIGH || digitalRead(RED_B)==HIGH){
			Serial.println("Innactivité annulé");
			actif = !actif;
			configurer_parametres();
		}
	}
}

void loop{
	if (mode_standard){
		standard();
	}else if (mode_economique){
		Log_intervall = LOG_INTERVALL(Log_intervall*2);
		economique(Log_intervall);
		Log_intervall = LOG_INTERVALL(Log_intervall/2);
	}else if (mode_maintenance){
		maintenance();
	}
}
