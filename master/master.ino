#include "RF24Network.h"
#include "RF24.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

////////////   Defines   ////////////
#define SENDAPPROACHES 20
#define NR_OF_SENSORS 10
#define BUTTON_ADC_1 6
#define BUTTON_ADC_2 7
#define SENSOR_TIMEOUT 60000 //in ms

//  1       5
//  2   4   6   8
//  3       7

#define BUTTON_MIN_1 760
#define BUTTON_MAX_1 800

#define BUTTON_MIN_2 300
#define BUTTON_MAX_2 350

#define BUTTON_MIN_3 420
#define BUTTON_MAX_3 480

#define BUTTON_MIN_4 620
#define BUTTON_MAX_4 660

#define BUTTON_MIN_5 760
#define BUTTON_MAX_5 800

#define BUTTON_MIN_6 300
#define BUTTON_MAX_6 350

#define BUTTON_MIN_7 420
#define BUTTON_MAX_7 480

#define BUTTON_MIN_8 620
#define BUTTON_MAX_8 660
//////////////////////////////////



// Battery

byte batteryChar[7][8] = {
  {0b01110,0b11011,0b10001,0b10001,0b10001,0b10001,0b10001,0b11111},  //leer
  {0b01110,0b11011,0b10001,0b10001,0b10001,0b10001,0b11111,0b11111},
  {0b01110,0b11011,0b10001,0b10001,0b10001,0b11111,0b11111,0b11111},
  {0b01110,0b11011,0b10001,0b10001,0b11111,0b11111,0b11111,0b11111},
  {0b01110,0b11011,0b10001,0b11111,0b11111,0b11111,0b11111,0b11111},
  {0b01110,0b11011,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111},
  {0b01110,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111}   //voll
};

byte tankChar[4][8] = {
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b11111},  //leer
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b11111,0b11111,0b11111},
  {0b10001,0b10001,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111},
  {0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111,0b11111}   //voll
};


///////////////////////////////////////


////////////   Structures   ////////////

char newName[11] = "";
//char symbolcounter = ;
unsigned long millisBuffer = 0;

unsigned char newLimit = 50;
unsigned char arrowPos = 0;
unsigned char arrowPosOld = 0;
unsigned char activeListEntry = 0;
unsigned char arrayEntries = 0;
unsigned char VIEW = 1;        // sesorView = 1 // configView = 2 // changeNameView = 3  // sensorInfoView = 4
unsigned char button = 0;

bool buttonPress = false;
bool refresh = false;
bool oldSensor = false;
bool requestCfg = false;
bool sendCfg = false;
bool gotCfg = false;
bool set = false;
bool nameChanged = false;
bool limitChanged = false;


struct payload_L {
  byte line;
  byte row;
  char message[21];
} text_L;

struct sensor_DATA {
  byte id = 0;
  char plant[11];
  byte moisture;
  byte limit;
  bool autoWatering;
  byte battery;
  byte waterlevel;
};

struct sensor_DATA_T {
  sensor_DATA data;
  unsigned long ms;
};

struct config_DATA {
  uint16_t nodeID = 0;
  char sensorName[11] = "Test";
  byte limit = 60;
  bool automatic = true;
  byte pumptime = 2;
  byte seconds = 2;
  byte minutes = 0;
  byte hours = 0;
} configuration;

char symbolBuffer[20] = { -1 };



/////////////////////////////////////////


/***** Configure the chosen CE,CS pins *****/
RF24 radio(9, 10);
RF24Network network(radio);
RF24Mesh mesh(radio, network);
LiquidCrystal lcd(7, 8, 2, 3, 4, 5);

sensor_DATA_T sensorList[NR_OF_SENSORS];
sensor_DATA sensorDATA;


////////////   Functions   ////////////
void SetSensorName(char sName[]);
void SetSensorLimit(byte lim);
void SetSensorAutomatic(bool autom);
bool SendConfig(uint8_t nId);
bool RequestConfig(uint8_t nId);
void ShowSensorList();
unsigned char buttonPressed();
void MenuUpDown();
void PrintArrow();
void CleanSensorList();
void SubmenuSensorConfig();
void ChangeNameRoutine();
void SetSpecificConfig();
void ChangeLimitRoutine();
void ShowSensorInfo();
///////////////////////////////////////


void setup() {

  Serial.begin(115200);
  /*
  lcd.createChar(0, degreeChar);
  lcd.createChar(1, waterChar);
  lcd.createChar(2, arrowChar);
  lcd.createChar(3, batteryChar[0]);
  lcd.createChar(4, batteryChar[1]);
  lcd.createChar(5, batteryChar[2]);
  lcd.createChar(6, batteryChar[3]);
  lcd.createChar(7, batteryChar[4]);
  lcd.createChar(8, batteryChar[5]);
  lcd.createChar(9, batteryChar[6]);
  lcd.createChar(10,tankChar[0]);
  lcd.createChar(11,tankChar[1]);
  lcd.createChar(12,tankChar[2]);
  lcd.createChar(13,tankChar[3]);*/
  lcd.begin(20, 4);
  lcd.print("Warte auf Sensoren..");
  delay(1000);
  mesh.setNodeID(0);        // Set the nodeID to 0 for the master node
  mesh.begin();         // Connect to the mesh
  delay(100);
  lcd.clear();
 // PrintArrow();
}


void loop() {
  if (gotCfg == true) {
    SendConfig(configuration.nodeID);
  }
  button = ButtonPressed();
  SubmenuSensorConfig();
  ChangeNameRoutine();
  ChangeLimitRoutine();
  ShowSensorInfo();
  MenuUpDown();
  mesh.update();        // Call mesh.update to keep the network updated
  mesh.DHCP();          // In addition, keep the 'DHCP service' running on the master node so addresses will be assigned to the sensor nodes
  if (network.available()) {      // Check for incoming data from the sensors
    RF24NetworkHeader header;
    network.peek(header);

    switch (header.type) {

      case 'C':   network.read(header, &configuration, sizeof(configuration));
        //setSpecificConfig();
        gotCfg = true;
        if(nameChanged == true){
          strcpy(configuration.sensorName, newName);
          nameChanged= false;
        }
        if(limitChanged == true){
          configuration.limit=newLimit;
          limitChanged = false;
        }

        Serial.println("Angeforderte Config erhalten");
        break;

      case 'S':   network.read(header, &sensorDATA, sizeof(sensorDATA));
        oldSensor = false;
        for (unsigned char sensorId = 0; sensorId < NR_OF_SENSORS; sensorId++) {
          if (sensorDATA.id == sensorList[sensorId].data.id) {
            sensorList[sensorId].data = sensorDATA;
            sensorList[sensorId].ms = millis();
            oldSensor = true;

          }
        }

        if (oldSensor == false) {
          sensorList[arrayEntries].data = sensorDATA;
          sensorList[arrayEntries].ms = millis();
          arrayEntries++;
          //oldSensor=false;
        }
        Serial.print("ID: ");
        Serial.print(sensorDATA.id);
        Serial.print("  Name: ");
        Serial.print(sensorDATA.plant);
        Serial.print("  Feuchte: ");
        Serial.print(sensorDATA.moisture);
        Serial.print("  Limit: ");
        Serial.print(sensorDATA.limit);
        Serial.print("  Auto: ");
        Serial.print(sensorDATA.autoWatering);
        Serial.print("  Wasserpegel: ");
        Serial.println(sensorDATA.waterlevel);
        Serial.print("  Battery: ");
        Serial.println(sensorDATA.battery);

        if (requestCfg == true) {
          if (RequestConfig(uint8_t(sensorList[activeListEntry].data.id)) == true)
            //if (RequestConfig(1) == true)
            Serial.print("Config erfolgreich angefordert von: ");
          Serial.println(sensorList[activeListEntry].data.id);
          // gotCfg = true;
        }

        refresh = true;


        break;

      default:    network.read(header, 0, 0);
        Serial.println(header.type);
        break;
    }
  }


  CleanSensorList();
  if (refresh) ShowSensorList();
}


//////////////////////////////////////////////////////

void SetSensorName(char sName[]) {
  strcpy(configuration.sensorName, sName);
}

void SetSensorLimit(byte lim) {
  configuration.limit = lim;
}

void SetSensorAutomatic(bool autom) {
  configuration.automatic = autom;
}

bool SendConfig(uint8_t nId) {
  Serial.println("Send Config");
  gotCfg = false;
  byte sendFailed = 0;
  for (uint8_t i = 0; i < NR_OF_SENSORS; i++) {
    if (mesh.addrList[i].nodeID == nId) {  //Searching for node one from address list
      RF24NetworkHeader header(mesh.addrList[i].address, OCT);
      header.type = 'C';
      // RF24NetworkHeader header(/*conf.nodeID*/1,'C');
      while (!network.write(header, &configuration, sizeof(configuration)) && sendFailed < SENDAPPROACHES) {
        sendFailed++;
      }
      if (sendFailed == SENDAPPROACHES) {
        Serial.println("Senden fehlgeschlagen");
        return false;
      }
      else {
        Serial.println("Gesendet");
        return true;
      }
    }
  }
}

bool RequestConfig(uint8_t nId) {
  requestCfg = false;
  Serial.println("Request Config");
  byte sendFailed = 0;
  for (uint8_t i = 0; i < NR_OF_SENSORS; i++) {
    if (mesh.addrList[i].nodeID == nId) {  //Searching for node one from address list
      RF24NetworkHeader header(mesh.addrList[i].address, OCT);
      header.type = 'R';
      while (!network.write(header, &nId, sizeof(nId)) && sendFailed < SENDAPPROACHES) {
        sendFailed++;
      }
      if (sendFailed == SENDAPPROACHES) {
        Serial.println("Request fehlgeschlagen");
        return false;
      }
      else {
        Serial.println("Request erfolgreich");
        return true;
      }
    }
  }
}

void ShowSensorList() {
  char buff[21];
  unsigned char nrOfSensors = 0;
  //  unsigned char r = 0;
  lcd.clear();

  if (VIEW==1) {
    arrowPos=0;
    activeListEntry=0;
    for (byte i = 0; i < NR_OF_SENSORS; i++) {


      if (sensorList[i].data.id > 0 /*&& (millis()-sensorList[i].ms)<SENSOR_TIMEOUT*/) {
        //    Serial.println(millis()-sensorList[i].ms);
        //  Serial.print("ID __  ");
        //    Serial.println(sensorList[i].data.id);
        nrOfSensors++;
        lcd.setCursor(1, i); // i-1
        lcd.print(sensorList[i].data.plant);
        //  Serial.println(sensorList[i].data.plant);
        lcd.setCursor(13, i);
        sprintf(buff, "%2d", sensorList[i].data.moisture);
        lcd.print(buff);
        lcd.print("/");
        lcd.setCursor(16, i);
        sprintf(buff, "%2d", sensorList[i].data.limit);
        lcd.print(buff);
        lcd.setCursor(19, i);
        //    r++;
      //  if (sensorList[i].data.refill) {
       //   lcd.write(byte(1));
     //   }        
        //else{
         BatteryState(i);
         lcd.setCursor(18, i);
         TankLevel(i);
          //Serial.println(millis()-sensorList[i].ms);
     //   }

      } else {
        lcd.setCursor(1, i);
        lcd.print("                   ");
      }

    }
    if(nrOfSensors==0){
      lcd.clear();
      lcd.setCursor(1, 0); // i-1
      lcd.print("Kein Sensor");
       lcd.setCursor(1, 1); // i-1
      lcd.print("verbunden"); 
    }
  }
  refresh=false;
 }


unsigned char ButtonPressed() {
  unsigned char button = 0;
  unsigned int val1 = analogRead(BUTTON_ADC_1);
  unsigned int val2 = analogRead(BUTTON_ADC_2);
  if (val1 < 900 || val2 < 900)buttonPress = true;
  if (val1 > BUTTON_MIN_1 && val1 < BUTTON_MAX_1)button = 1;
  else if (val1 > BUTTON_MIN_2 && val1 < BUTTON_MAX_2)button = 2;
  else if (val1 > BUTTON_MIN_3 && val1 < BUTTON_MAX_3)button = 3;
  else if (val1 > BUTTON_MIN_4 && val1 < BUTTON_MAX_4)button = 4;

  else if (val2 > BUTTON_MIN_5 && val2 < BUTTON_MAX_5)button = 5;
  else if (val2 > BUTTON_MIN_6 && val2 < BUTTON_MAX_6)button = 6;
  else if (val2 > BUTTON_MIN_7 && val2 < BUTTON_MAX_7)button = 7;
  else if (val2 > BUTTON_MIN_8 && val2 < BUTTON_MAX_8)button = 8;
  else button = 0;
  delay(150);
  return button;
}

void MenuUpDown() {
  // unsigned char pressed = buttonPressed();
  if (buttonPress == true && (VIEW == 1 || VIEW == 2)) {
    switch (button) {
      case 0:
        break;

      case 7:
        if (arrowPos < 3)arrowPos = arrowPosOld + 1;
        if ( activeListEntry < 255 && VIEW == 1)activeListEntry++;
        // delay(200);
        break;

      case 5:
        if (arrowPos > 0)arrowPos = arrowPosOld - 1;
        if (activeListEntry > 0 && VIEW == 1)activeListEntry--;
        //delay(200);
        break;

      default:
        Serial.println("Fehlerhafter Tastendruck");
        break;
    }
    PrintArrow();
  }

}

void PrintArrow() {
  lcd.setCursor(0, arrowPosOld);
  lcd.print(" ");
  lcd.setCursor(0, arrowPos);
  arrowPosOld = arrowPos;
  //lcd.write(byte(2));
  lcd.print(">");
  delay(200);
}

void CleanSensorList() {
  for (byte i = 0; i < NR_OF_SENSORS; i++) {
    if ((millis() - sensorList[i].ms) > SENSOR_TIMEOUT && sensorList[i].ms > 0) {
      sensorList[i].data.id = 0;
      refresh = true;
      arrayEntries--;
    }
  }
  // Aufrücken:

  for (byte i = 0; i < NR_OF_SENSORS - 1; i++) {
    if (sensorList[i].data.id == 0 && sensorList[i].data.id < NR_OF_SENSORS) {
      sensorList[i] = sensorList[i + 1];
      sensorList[i + 1].data.id = 0;
    }


  }

}


//// Submenu Pflanzenconfig ändern

void SubmenuSensorConfig() {
  if (button == 1) {
    //changeNameView = false;
    //sensorView = false;
    VIEW = 2;
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print(sensorList[arrowPos].data.plant);
    lcd.setCursor(1, 1);
    lcd.print("Name aendern");
    lcd.setCursor(1, 2);
    lcd.print("Sensorinfo");
    lcd.setCursor(1, 3);
    lcd.print("Limit aendern");
    arrowPos=1;
    //activeListEntry=0;

  }
  if (button == 2) {
    lcd.clear();
    VIEW = 1;
    arrowPos=0;
    activeListEntry=0;
  }

  if (button == 6) {
    switch (arrowPos){
      case 1:
      VIEW = 3; //ChangeNameRoutine
      break;

      case 2:
      VIEW = 4;//ShowSensorInfo
      break;

      case 3:
      VIEW = 5;//ChangeLimitRoutine
      break;
    }
  }


}

void ChangeNameRoutine() {
if (VIEW==3){ 
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Name: ");

 
  unsigned char x = 1;
  
  unsigned char k = 7;
  char buchstabe = 0x61;
  set = false;
  unsigned char dirButton;
  lcd.setCursor(k, 0);
  lcd.print(buchstabe);
  //if (changeNameView == true) {
  bool switched = false;
  while (set == false) {
    dirButton = ButtonPressed();

  

    
 
    

    switch (dirButton) {
      case 5 : //up
      buchstabe++;

     lcd.setCursor(k, 0);
     lcd.print(buchstabe);
      
        break;
      case 4 : //left
        if (k > 7)k--;

        lcd.setCursor(k, 0);
        break;
      case 7 : //down
        
         buchstabe--;
       lcd.setCursor(k, 0);
       lcd.print(buchstabe);
        break;
        
      case 8 : //right
        //if (x < 20)x++;
        buchstabe=32;
        break;
        
      case 3 : //set
        if (k < 17) {
          lcd.setCursor(k, 0);
          newName[k - 7] = buchstabe;
         lcd.print(buchstabe);
         if(k==16){
          newName[10] = 0x00;
          requestCfg = true;
          //changeNameView = false;
         // sensorView = true;
         //VIEW = 1;
          nameChanged=true;
          set = true;
         
          lcd.clear();
          VIEW=1;
          arrowPos=0;
          activeListEntry=0;
         }else{
          k++;
          lcd.setCursor(k, 0);
          lcd.print(buchstabe);
         }       
        }
        break;
    }
    if(dirButton == 0){
    if(millis()-millisBuffer > 1000)millisBuffer = millis();
    
    if( (millis()-millisBuffer < 500) && !switched){
      //millisBuffer = millis();
      switched=true;
      lcd.setCursor(k, 0);
      lcd.print(' ');
    }
    if ((millis()-millisBuffer > 500) && switched){
      switched=false;
      lcd.setCursor(k, 0);
      lcd.print(buchstabe);
    }
  }
  //lcd.print(buchstabe);
  //  lcd.setCursor(x, y);

  }
}

}

void SetSpecificConfig() {
  configuration.nodeID = 2;
  strcpy(configuration.sensorName, "Basilik");
  configuration.limit = 40;
  configuration.automatic = true;
  configuration.pumptime = 2;
  configuration.seconds = 2;
  configuration.minutes = 0;
  configuration.hours = 0;
}

void ChangeLimitRoutine(){
  if (VIEW==5){ 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(sensorList[activeListEntry].data.plant);
  lcd.setCursor(0, 1);
  lcd.print("Aktuelles Limit: ");
  lcd.setCursor(0, 2);
  lcd.print("Neues Limit:");
   lcd.setCursor(17, 1);
  lcd.print(sensorList[activeListEntry].data.limit);
  lcd.setCursor(17, 2);
 
  unsigned char x = 50;
  set = false;
  unsigned char dirButton;
  lcd.print(x);
  //if (changeNameView == true) {
  while (set == false) {
 
    dirButton = ButtonPressed();

    switch (dirButton) {
      case 5 : //up
        if (x < 0x7a)x++;
        lcd.setCursor(17, 2);
        lcd.print(x);
        break;
      
      case 7 : //down
        if (x > 0x20)x--;
        lcd.setCursor(17, 2);
        lcd.print(x);
        break;
     
      case 3 : //set
          newLimit=x;
          requestCfg = true;
          set = true;
          limitChanged=true;
          lcd.clear();
          VIEW=1;  

        break;
    }

   
  }
  
}

}

void ShowSensorInfo(){
  if (VIEW==4){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Name: ");
  lcd.setCursor(6,0);
  lcd.print(sensorList[activeListEntry].data.plant);
  lcd.setCursor(0,1);
  lcd.print("ID: ");
  lcd.setCursor(4,1);
  lcd.print(sensorList[activeListEntry].data.id);

  lcd.setCursor(0,2);
  lcd.print("Limit: ");
  lcd.setCursor(7,2);
  lcd.print(sensorList[activeListEntry].data.limit);

  lcd.setCursor(11,2);
  lcd.print("Autom.: ");
  lcd.setCursor(19,2);
  lcd.print(sensorList[activeListEntry].data.autoWatering);
  

  lcd.setCursor(0,3);
  lcd.print("Battery: ");
  lcd.setCursor(9,3);
  lcd.print(sensorList[activeListEntry].data.battery);
  lcd.setCursor(11,3);
  lcd.print("%");
  delay(1000);
  VIEW = 1;
  arrowPos=0;
  activeListEntry=0;
  while(ButtonPressed()==0);
  lcd.clear();
  }  
}

void BatteryState(byte i){
if(sensorList[i].data.battery<10)lcd.createChar(i, batteryChar[0]);
else if(sensorList[i].data.battery<25)lcd.createChar(i, batteryChar[1]);
else if(sensorList[i].data.battery<40)lcd.createChar(i, batteryChar[2]);
else if(sensorList[i].data.battery<55)lcd.createChar(i, batteryChar[3]);
else if(sensorList[i].data.battery<70)lcd.createChar(i, batteryChar[4]);
else if(sensorList[i].data.battery<85)lcd.createChar(i, batteryChar[5]);
else if(sensorList[i].data.battery<=100)lcd.createChar(i, batteryChar[6]); 
lcd.setCursor(19, i);
lcd.write(byte(i));

}

void TankLevel(byte i){
  if(sensorList[i].data.waterlevel==0)lcd.createChar(i+4,tankChar[0]);
  else if(sensorList[i].data.waterlevel==1)lcd.createChar(i+4,tankChar[1]);
  else if(sensorList[i].data.waterlevel==2)lcd.createChar(i+4,tankChar[2]);
  else if(sensorList[i].data.waterlevel==3)lcd.createChar(i+4,tankChar[3]);
  lcd.setCursor(18, i);
  lcd.write(byte(i+4));
  
}

/*
void printSymbol(char id){
  char adress = -1;
 
for (char adrPos = 0; adrPos <7; adrPos++){
  for (char i = 0; i<symbolBuffer.size-1;i++){
    if (adrPos == symbolBuffer[i]){
      break; 
    }
    else{
      adress = adrPos;
    }
  }
}
  if(!symbolEnabled){
    
  }
}*/


