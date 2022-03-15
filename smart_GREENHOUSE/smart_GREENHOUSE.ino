#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <SimpleTimer.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <EEPROM.h>

//EEPROM adresy
//0 - rezim vodniho cerpadla
//1 - rezim osvetleni
//2 - sensor intenzivity osvetleni
//3 - stav svetel (0/1)
//4 - stav cerpadla (0/1)

#define DIST_TRIG_PIN 18 //ultrazvukovy sensor vzdálenosti (trigger pin)
#define DIST_ECHO_PIN 19 //ultrazvukovy sensor vzdálenosti (echo pin)
#define MOIST_PIN 34 //senzor vlhkosti zeminy
#define LIGHT_PIN 35 //senzor intenzity světla
#define WATER_RELAY 16 //rele 1
#define LIGHT_RELAY 17 //rele 2
#define RELAY_PIN3 25 //rele 3
#define RELAY_PIN4 26 //rele 4
#define TEMP_MOIST_PIN  13 // teplota a vlhkost vzduchu

//velikost pameti flash do ktere budeme zapisovat stavove hodnoty a senzory
#define EEPROM_SIZE 6

//definovaní typu digitalniho sensoru DHT11
#define DHT_SENSOR_TYPE DHT11
DHT dht_sensor(TEMP_MOIST_PIN, DHT_SENSOR_TYPE);




//tyto promenne a konstatnta jsou zde kvuli docileni chodu bez delay (automatické zalevaní)
int pumpState = LOW;
unsigned long previousMillis = 0;
const long interval = 5000; //interval zaleveni pri automatickem spusteni = 5 vterin


//tyto hex kody jsou zde kvuli designu v aplikaci Blynk (pro nastavovani barev)
const String BLYNK_BLACK =   "#000000";
const String BLYNK_GREEN =  "#23C48E";

//parametry pro Blynk komunikaci
char auth[] = "XWzhVHiHr3tG9HDNhM6mGnoi3kffkyVj"; //OAuth kod
char ssid[] = "test"; //SSID (název) Wifi AP
char pass[] = "0123456789"; //heslo k Wifi

//int Minutes;
//int WaterTimer;
//int TimerSens;
bool LightMode; //rezim svetel (1 - auto, 0 - manual)
bool WaterMode; //rezim zalevani (1 - auto, 0 - manual)
int LIntensity; //intenzita osvetleni
int SoilMoist; //vlhkost zeminy

int Moisture;

// vytovření timeru, který bude volat metodu se snimanim senzoru
BlynkTimer timer;

BLYNK_WRITE(V0) //vyber spousteciho rezimu svetel (auto/manual)
{
  if (param.asInt() == 1) {
    LightMode = true;
    //zapis hodnot do eeprom
    EEPROM.write(1, LightMode);
    EEPROM.commit();
    //designove "zablokovani" tlacitka
    Blynk.setProperty(V11, "onLabel", "disabled");
    Blynk.setProperty(V11, "offLabel", "disabled");


    Blynk.setProperty(V10, "onLabel", "SET");
    Blynk.setProperty(V10, "offLabel", "SET");
  }
  else {
    Blynk.syncVirtual(V11);
    LightMode = false;
    EEPROM.write(1, LightMode);
    EEPROM.commit();
    Blynk.setProperty(V10, "onLabel", "disabled");
    Blynk.setProperty(V10, "offLabel", "disabled");

    //designove "odblokovani" tlacitka
    Blynk.setProperty(V11, "onLabel", "ON");
    Blynk.setProperty(V11, "offLabel", "OFF");
  }
}

BLYNK_WRITE(V10)
{
  //pokud je aktivni auto rezim osvetleni, po stisknuti tlacitka (V10) se zaznamena hodnota ze senzoru intenzity svetla a poté se bude porovnávat s aktualne namerenou
  if (LightMode) {
    LIntensity = analogRead(LIGHT_PIN);
    EEPROM.write(2, LIntensity / 17); //tento parametr jsem musel vydelit 17 (pote ho zase vynasobim), protože se do 1 bytu vejdou cisla v rozmezi 0 az 255
    EEPROM.commit(); //potvrzeni ulozeni hodnot do eeprom pameti
  }
}

BLYNK_WRITE(V11) {
  //pokud mame zvoleny manualni rezim (pouze zpristupnime tlacitko, aby mohlo spinat)
  if (!LightMode) {
    digitalWrite(LIGHT_RELAY, param.asInt());
    EEPROM.write(3, param.asInt());
    EEPROM.commit();
  }
}


BLYNK_WRITE(V14)
{
  //pokud mame zvoleny manualni rezim (pouze zpristupnime tlacitko, aby mohlo spinat)
  if (!WaterMode) {

    digitalWrite(WATER_RELAY, param.asInt());
    EEPROM.write(4, param.asInt());
    EEPROM.commit();
  }
}


BLYNK_WRITE(V15) {  //vyber spousteciho rezimu cerpadla (auto/manual)

  if (param.asInt() == 1) {
    WaterMode = true;
    EEPROM.write(0, WaterMode);
    EEPROM.commit();
    Blynk.setProperty(V14, "onLabel", "disabled");
    Blynk.setProperty(V14, "offLabel", "disabled");

  }
  else {
    WaterMode = false;
    digitalWrite(WATER_RELAY, HIGH);
    EEPROM.write(0, WaterMode);
    EEPROM.commit();
    Blynk.setProperty(V14, "onLabel", "WATERING");
    Blynk.setProperty(V14, "offLabel", "WATER");

  }
}

//pote se jedntoka uspesne pripoji na server, aktualizuje vsechny virtualni piny
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V11, V14, V15, V10, V0);
}



void sensCheck() { //metoda, která sbira data ze sensoru je volaná časovačem

  //kod pro ovladani ultrazvukového senzoru
  //generovani pulsu
  digitalWrite(DIST_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(DIST_TRIG_PIN, LOW);
  //měření délky času, za ktery se pulz vrati
  float duration_us = pulseIn(DIST_ECHO_PIN, HIGH);
  //vypocitani vzdálenosti z casu
  float distance_cm = 0.017 * duration_us;



  //kod pro cteni DHT11 senzoru (teplota a vlhkost vzduchu)
  //vlhkost
  float humi  = dht_sensor.readHumidity();
  //teplota
  float tempC = dht_sensor.readTemperature();



  //ziskani parametru ze senzoru vlhkosti zeminy
  SoilMoist = analogRead(MOIST_PIN);

  //ziskani parametru ze senzoru intenzity světla
  int light = analogRead(LIGHT_PIN);
  //Serial.println(light);

  //podmínka, ktera porovnava hodnotu nahranehe intenziy svetla a okolni, pokud je mensi, tak zapne svetla (pokud je LightMode = 1 -> automaticke zapinani osvetleni)
  if (LightMode) {
    if (LIntensity >= light) {
      digitalWrite(LIGHT_RELAY, LOW);
    }
    else {
      digitalWrite(LIGHT_RELAY, HIGH);
    }
  }


  if (SoilMoist > 3700 && WaterMode == 1) {
    digitalWrite(WATER_RELAY, LOW);
  }
  else {
    digitalWrite(WATER_RELAY, HIGH);
  }
  //testovaci vypis hodnot namerenych ze vsech senzoru
  Serial.print("Distance: ");
  Serial.println(distance_cm);

  Serial.print("Humidity: ");
  Serial.println(humi);

  Serial.print("Temperature: ");
  Serial.println(tempC);

  Serial.print("Soil moisture: ");
  Serial.println(SoilMoist);

  Serial.print("Light intesity: ");
  Serial.println(light);

  //Odeslani hodnot ze senzoru do virtualnich pinu v aplikaci Blynk
  Blynk.virtualWrite(V1, tempC);  //V5 is for Humidity
  Blynk.virtualWrite(V2, humi);  //V6 is for Temperat
  Blynk.virtualWrite(V3, light);
  Blynk.virtualWrite(V4, distance_cm);
}


void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  //vsechny parametry se po zapnuti nahraji z eeprom pameti do promennych
  LightMode = EEPROM.read(1);
  WaterMode = EEPROM.read(0);
  LIntensity = EEPROM.read(2) * 17;

  //pokud mame manualni rezim tak se z eeprom nahraji posledni stavy
  if (LightMode == 0)
  {
    digitalWrite(LIGHT_RELAY, EEPROM.read(3));
  }

  if (WaterMode == 0)
  {
    digitalWrite(LIGHT_RELAY, EEPROM.read(4));
  }

  dht_sensor.begin();
  //nastaveni časovace ktery bude volat kazdych 10 vterin funkci sensCheck()
  timer.setInterval(5000, sensCheck);
  //zahajenni pripojovani k Blynk serveru
  Blynk.begin(auth, ssid, pass, IPAddress(46, 4, 122, 35), 12080);

  //pokud by se blynk nedokazal pripojit k wifi, tak prepne osvetleni i zalevani do automatického rezimu
  if (!Blynk.connected()) {
    LightMode = 1;
    WaterMode = 1;
  }

  // konfigurace pinu pro ultrazvukovy senzor
  pinMode(DIST_TRIG_PIN, OUTPUT);
  pinMode(DIST_ECHO_PIN, INPUT);

  pinMode(LIGHT_RELAY, OUTPUT);
  pinMode(WATER_RELAY, OUTPUT);

}

void loop() {
  Blynk.run();
  timer.run();

  //automaticke zalevani bez pouziti funkce delay()
  
  //if (SoilMoist > 3700 && WaterMode == 1) {
    
    //unsigned long currentMillis = millis();
    //if (currentMillis - previousMillis >= interval) {
      
      //previousMillis = currentMillis;
      //if (pumpState == LOW && SoilMoist > 3700 && WaterMode == 1) {
       // pumpState = HIGH;
     // } else {
     //   pumpState = LOW;
     // }
      //digitalWrite(WATER_RELAY, pumpState);
   // }
  //}
}
