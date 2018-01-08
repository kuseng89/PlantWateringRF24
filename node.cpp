#include <Arduino.h>
#include <Battery.h>
#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
//#include <SPI.h>
#include <LowPower.h>
#include <EEPROM.h>

//#include <Serial.h>



//////   DEFINES   ///////////

#define NODEID 2

#define TANKPIN_LOW A1
#define TANKPIN_MIDDLE A2
#define TANKPIN_HIGH A3
#define TANKPIN_ENABLE 5
#define SENSORPIN A4
#define SENSORPOWERPIN 2
#define PUMPPIN 3
#define BATTERYPIN A0
#define RF24_ENABLE 4

#define MILLISECONDS 0
#define SECONDS 10
#define MINUTES 0
#define HOURS 0

#define PUMPTIME 2
#define MEASURE_RANGE 10
#define SENDAPPROACHES 20



//////   GLOBALS   ///////////

/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(9, 10);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

Battery battery = Battery(6200, 9000, BATTERYPIN,2); // also specify an activationPin for on-demand configurations

String inputString = "";         // a string to hold incoming data
bool stringComplete = false;  // whether the string is complete
bool nosleep = false;
bool changeCfg = false;

bool newData = false;

char soilMoisture = 0;
unsigned int sleepingTime = 0;
//char limit = 0;


struct payload_t {
  unsigned long ms;
  unsigned long counter;
};

struct config_DATA {
  uint16_t nodeID;
  char sensorName[11];
  char limit;
  bool automatic;
  char pumptime;
  char seconds;
  char minutes;
  char hours;
} configuration;

struct sensor_DATA {
  char id = NODEID;
  char plant[11] = "Pflanze";
  char moisture = 100;
  char limit = 0;
  bool autoWatering = true;
  char battery = 5;
  char waterlevel = 3;
} sensorData;




/////////////
sensor_DATA GetSensorData();
config_DATA GetSensorConfig();
char MeasureSoilMoisture();
char GetLimit();
bool GetAutomatic();
unsigned int CalculateSleeptime();
void PwrDown(int seconds);
bool SendSensorData(sensor_DATA sdat);
void SetConfig(config_DATA cfg);
bool SendSensorConfig(config_DATA cfgdat);
void StartWatering();
void GetConfig();
void GetName();
char GetBatteryLoad();
char GetWaterLevel();
void Sleep(period_t period);
//////////




void setup() {
  battery.begin();
  delay(100);
  GetConfig();
  Serial.begin(115200);
  mesh.setNodeID(NODEID);            // Set the nodeID manually
  Serial.println(F("Connecting to the mesh..."));
  if (mesh.begin())Serial.println(F("Connected"));
  else Serial.println(F("Connection Failed"));

  pinMode(PUMPPIN, OUTPUT);
  pinMode(SENSORPOWERPIN, OUTPUT);
  pinMode(TANKPIN_ENABLE, OUTPUT);
  pinMode(RF24_ENABLE, OUTPUT);
  sleepingTime = CalculateSleeptime();
  Serial.println("Feuchtigkeits Ueberwachung v0.1");
  Serial.print("Grenzwert: ");
  Serial.println(GetLimit());
  delay(100);

}




void loop() {
  digitalWrite(RF24_ENABLE,HIGH);
  if(!mesh.checkConnection())mesh.renewAddress();
  mesh.update();
  while (network.available()) {
    RF24NetworkHeader header;
    payload_t payload;
    network.peek(header);
    switch (header.type) {

      case 'C':    //Neue Config erhalten
      if (changeCfg == true){
        network.read(header, &configuration, sizeof(configuration));
        Serial.print("Neue Config// ID: ");
        Serial.print(configuration.nodeID);
        Serial.print(" Name: ");
        Serial.print(configuration.sensorName);
        Serial.print("  Limit: ");
        Serial.print(configuration.limit);
        Serial.print("  Auto: ");
        Serial.print(configuration.automatic);
        Serial.print("  Pumptime: ");
        Serial.print(configuration.pumptime);
        Serial.print("  Sleep: ");
        Serial.print(configuration.hours);
        Serial.print(":");
        Serial.print(configuration.minutes);
        Serial.print(":");
        Serial.println(configuration.seconds);
        SetConfig(configuration);

        SendSensorData(GetSensorData());
        changeCfg = false;
        }

        break;

      case 'R':  // Config-Request
        //Serial.println(" R erhalten");
        nosleep = true;
        uint8_t idBuffer;
        network.read(header, &idBuffer, sizeof(idBuffer));
        Serial.print("idBuffer: ");
        Serial.println(idBuffer);
        if (idBuffer == NODEID) {
          changeCfg = true;
          idBuffer = 0;
        }
        break;



      default:
        Serial.println("Ankommende Daten sind fehlerhaft");
        break;
    }

  }
  if (changeCfg == true) {
    Serial.println("changeCfg == true");
    GetConfig();
    SendSensorConfig(configuration);
    delay(500);
  }
  else {
   Serial.println("changeCfg == false");
   // if(nosleep==false){
    digitalWrite(RF24_ENABLE,LOW);
    Serial.println("sleep");
    PwrDown(sleepingTime);
    digitalWrite(RF24_ENABLE,HIGH);
    // delay(2000);
    Serial.println("wake up");
    Serial.print("Name: ");
    Serial.println(configuration.sensorName);
    char moist = MeasureSoilMoisture();
    SendSensorData(GetSensorData());
    if (moist < configuration.limit) {
      StartWatering();
    }

  }//}
}


//////// Functions  ////////////////


sensor_DATA GetSensorData() {
  sensor_DATA data;
  GetName();
  strcpy(data.plant, configuration.sensorName);
  data.moisture = MeasureSoilMoisture();
  data.limit = GetLimit();
  data.autoWatering = GetAutomatic();
  data.battery = GetBatteryLoad();
  data.waterlevel = GetWaterLevel();

  return data;
}

config_DATA GetSensorConfig() {
  config_DATA data;
  data.nodeID = NODEID;
  data.sensorName[11];
  data.limit;
  data.automatic;
  data.pumptime;
  data.seconds;
  data.minutes;
  data.hours;

  return data;
}


char MeasureSoilMoisture() {
  digitalWrite(SENSORPOWERPIN, HIGH);
  delay(500);
  float soilmoist = 0;
  analogRead(SENSORPIN);
  delay(100);
  for (int i = MEASURE_RANGE; i > 0; i--) {
    soilmoist += analogRead(SENSORPIN);
    delay(20);
  }
  digitalWrite(SENSORPOWERPIN, LOW);

  soilmoist /= MEASURE_RANGE;
  char soilmoistt = round((880 - soilmoist) / 880 * 100);
  Serial.println(soilmoistt);

  return soilmoistt;
}

bool GetAutomatic() {
  bool automatic;
  EEPROM.get(1, automatic);
  return  automatic;
}

void StartWatering() {
  Serial.println("Giessen");
  Serial.end();
  digitalWrite(PUMPPIN, HIGH);
  for (int k = 0; k < PUMPTIME; k++) {
    delay(1000);
  }
  digitalWrite(PUMPPIN, LOW);
  delay(200);
  Serial.begin(115200);
  delay(200);

}

void PwrDown(int seconds) {
  Serial.end();
  while (seconds >= 8) {
    Sleep(SLEEP_8S);
    seconds -= 8;
  }
  if (seconds >= 4)    {
    Sleep(SLEEP_4S);
    seconds -= 4;
  }
  if (seconds >= 2)    {
    Sleep(SLEEP_2S);
    seconds -= 2;
  }
  if (seconds >= 1)    {
    Sleep(SLEEP_1S);
    seconds -= 1;
  }
  Serial.begin(115200);
}


void Sleep(period_t period) {
  LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}


unsigned int CalculateSleeptime() {
  unsigned int sleeptime =  SECONDS + (MINUTES * 60) + (HOURS * 3600) ;
  return sleeptime;
}


void SetLimit(char limit) {
  EEPROM.put(0, limit);
}


char GetLimit() {
  char limit;
  EEPROM.get(0, limit);
  Serial.print("Limit: ");
  Serial.println(limit);
  sensorData.limit = limit;
  return  limit;
}


void SetAuto(char automatic) {
  EEPROM.put(1, automatic);
}


bool GetAuto() {
  bool a;
  EEPROM.get(1, a);
  sensorData.autoWatering = a;
  return  a;
}

void GetName() {
  for (unsigned char i = 6; i < 17; i++) {
    EEPROM.get(i, configuration.sensorName[i - 6]);
  }//return sName;
  strcpy(sensorData.plant, configuration.sensorName);
}

void SetName(char* sName) {
  for (unsigned char i = 6; i < 17; i++) {
    EEPROM.put(i, sName[i - 6]);
  }

}

char GetPumptime() {
  char tim;  // "Palme     "
  EEPROM.get(2, tim);
  return tim;
}

void SetPumptime(char tim) {
  EEPROM.put(2, tim);
}

void GetSleeptime() {
  // char sName[11];  // "Palme     "
  EEPROM.get(3, configuration.seconds);
  EEPROM.get(4, configuration.minutes);
  EEPROM.get(5, configuration.hours);
  //return sName;
}

void SetSleeptime(char secs, char mins, char hrs) {
  EEPROM.put(3, secs);
  EEPROM.put(4, mins);
  EEPROM.put(5, hrs);
}



void GetConfig() {
  configuration.nodeID = NODEID;
  //configuration.sensorName[11] = "Test";
  GetName();
  configuration.limit = GetLimit();
  configuration.automatic = GetAuto();
  configuration.pumptime = GetPumptime;
  GetSleeptime();

  //configuration.seconds = SECONDS;
  // configuration.minutes = MINUTES;
  //configuration.hours = HOURS;



}

bool SendSensorData(sensor_DATA sdat) {
  char sendFailed = 0;
  while (!mesh.write(&sdat, 'S', sizeof(sdat)) && sendFailed < SENDAPPROACHES) {
    sendFailed++;
  }
  //delay(1000);
  if (sendFailed == SENDAPPROACHES) return false;
  else return true;
}

void SetConfig(config_DATA cfg) {

  //TODO SetSensorName
  SetLimit(cfg.limit);
  SetAuto(cfg.automatic);
  SetName(cfg.sensorName);

}

bool SendSensorConfig(config_DATA cfgdat) {
  char sendFailed = 0;
  while (!mesh.write(&cfgdat, 'C', sizeof(cfgdat)) && sendFailed < SENDAPPROACHES) {
    sendFailed++;
  }
  if (sendFailed == SENDAPPROACHES) return false;
  else return true;
}

char GetBatteryLoad(){
  Serial.print("Batterielevel: ");
  Serial.println(battery.level());
  return battery.level();
}

char GetWaterLevel(){
  char waterlevel=0;
  digitalWrite(TANKPIN_ENABLE,HIGH);
  // 3 pins auslesen
//  digitalWrite(TANK_ENABLE_PIN,HIGH);
  analogRead(TANKPIN_LOW);   //Dummyread
  analogRead(TANKPIN_MIDDLE);
  analogRead(TANKPIN_HIGH);
  delay(1000);
  /*
  Serial.print("LOW: ");
  Serial.println(analogRead(TANKPIN_LOW));
    Serial.print("MIDDLE: ");
  Serial.println(analogRead(TANKPIN_MIDDLE));
    Serial.print("HIGH: ");
  Serial.println(analogRead(TANKPIN_HIGH));
  delay(2000);
  */
  int level = analogRead(TANKPIN_LOW);
  if (level < 700){
    waterlevel=1;
    level = analogRead(TANKPIN_MIDDLE);
  }
  if (level < 700){
    waterlevel=2;
    level = analogRead(TANKPIN_HIGH);
  }
  if (level < 700){
    waterlevel=3;
   }

  Serial.print("Wasserpegel: ");
  Serial.println(waterlevel);

 // digitalWrite(TANK_ENABLE_PIN,LOW);
 digitalWrite(TANKPIN_ENABLE,LOW);
 return waterlevel;
}
