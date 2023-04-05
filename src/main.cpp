// Every minute it looks for a JSON file containing
// solar = kW generated
// battery = % battery
// grid = kW to/from the grid (negative is from)
// usage = kW currently being consumed by the house
// timestamp = a timestamp representing last update from the sensor
//{
//  "solar": 0,
//  "battery": 71,
//  "grid": 0,
//  "usage": 0.281
//  "timestamp": "20230418135055" 
//}

//
// json conversion 5 to 6 stuff I used is here https://arduinojson.org/v6/doc/upgrade/
// 
// TODO: Solis API - from https://solis-service.solisinverters.com/helpdesk/attachments/2043393248854
// No small feat
// 
// Python version:
// Body = '{"pageSize":100,  "id": "xxx", "sn": "yyy" }'
// Content_MD5 = base64.b64encode(hashlib.md5(Body.encode('utf-8')).digest()).decode('utf-8')
// encryptStr = (VERB + "\n"
//    + Content_MD5 + "\n"
//    + Content_Type + "\n"
//    + Date + "\n"
//    + CanonicalizedResource)
// h = hmac.new(secretKey, msg=encryptStr.encode('utf-8'), digestmod=hashlib.sha1)
// Sign = base64.b64encode(h.digest())
// Authorization = "API " + KeyId + ":" + Sign.decode('utf-8')
//
// Potential References: https://github.com/tzikis/ArduinoMD5
// Cryptosuite (for SHA1)
// bas64 (for base64encode)

#include <Arduino.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "fetch.h"
#include "configManager.h"
#include "timeSync.h"
#include <LiquidCrystal.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

// set up the important globulars
// put the following in a file in include called settings.h
// const char solarURL[] = "SOLIS_API_URL";
// const char solisKey[] = "YOUR_SOLIS_KEY";
// const char solisSecret[] = "YOUR_SOLIS_SECRET";
// const char solisId[] = "YOUR_SOLIS_SYSTEM_ID";
// const char solisSn[] = "YOUR_INVERTER_SERIAL_NUMBER";
// TODO: make it work properly with Solis API

#include "settings.h"

// LCD business
LiquidCrystal lcd(D1, D2, D4, D5, D6, D7);        // select the pins used on the LCD panel
int backLight = D8;
#define MAX 192
#define MED 100
#define DIM 40
#define MIN 10 

// Dreadful iconography
// ====================

// rubbish solar symbol
byte solarsprite[8] = {
  B00000, B10101, B01110, B11011, B11011, B01110, B10101, B00000
};
  // rubbish lightning symbol
byte gridsprite[8] = {
  B00111, B01110, B11000, B11111, B00111, B01110, B01100, B10000
};
  // rubbish plug symbol
byte mainssprite[8] = {
  B00000, B01010, B01010, B11111, B11111, B01110, B00100, B11100
  };

byte upsprite[8] = {
  B00100, B01110, B11011, B10001, B00000, B00000, B00000, B00000
  };

byte downsprite[8] = {
  B00000, B00000, B00000, B00000, B10001, B11011, B01110, B00100
  };

// JSON business
int jsonend = 0;
boolean startJson = false;

#define JSON_BUFF_DIMENSION 2500
String text; 

int oldBatteryValue=0;
String oldTimestampValue="";

// Button business
int buttonValue = 0;

// task definitions
struct task
{    
    unsigned long rate;
    unsigned long previous;
};

task connectWeb = { .rate = 30000, .previous = 0 };
task buttonPoll = { .rate = 200, .previous = 0 };
task buttonPress = { .rate = 5000, .previous = 0 };

// LCD brightness function
void lcdbright(int brightness) {
    pinMode(backLight, OUTPUT);
    analogWrite(backLight, brightness);
}

// Yer actural value displaying function
void lcdShow(String solar, String battery, String grid, String usage) {  
// create the battery icon first of all
  int batteryInt=battery.toInt();
  byte batterysprite[8] = {
    B01110, B01110, B11111, B11111, B11111, B11111, B11111, B11111
  };
  if (batteryInt < 90) {batterysprite[1]= B01010;}
  if (batteryInt < 75) {batterysprite[2]= B10001;}
  if (batteryInt < 60) {batterysprite[3]= B10001;}
  if (batteryInt < 45) {batterysprite[4]= B10001;}
  if (batteryInt < 30) {batterysprite[5]= B10001;}
  if (batteryInt < 15) {batterysprite[6]= B10001;}
  lcd.createChar(3, batterysprite);
 // also need to turn the grid value into a float for later
  float gridFloat=grid.toFloat();
 // first row
  lcd.setCursor(0,0);
// solar
  lcd.write(0);
  lcd.print(" ");
  lcd.print(solar.substring(0,4));
  lcd.print("kW      ");
  Serial.print("Solar: ");
  Serial.print(solar);
  Serial.println(" kW");
// battery
  lcd.setCursor(9,0);
  lcd.write(3);
  Serial.print("Battery: ");
  if ( batteryInt == oldBatteryValue ) {
    Serial.print("=");
    lcd.print("=");
  }
  if ( batteryInt < oldBatteryValue) {
    Serial.print("v");
    lcd.write(5);
  }
  if ( batteryInt > oldBatteryValue) {
    Serial.print("^");
    lcd.write(4);
  }
  lcd.setCursor(12,0);
  lcd.print(battery);
  lcd.print("%");
  Serial.print(battery);
  Serial.println("%");
// Second row
  lcd.setCursor(0,1);
  // grid
  lcd.write(1);
  Serial.print("Grid: ");
  if (gridFloat < 0) {
  // incoming
    lcd.write(5);
    lcd.print(grid.substring(1,4));
    Serial.print("v");
    Serial.print(grid.substring(1,4));
  }
  if (gridFloat > 0)  {
  // outgoing
    lcd.write(4);
    lcd.print(grid.substring(0,4));
    Serial.print("^");
    Serial.print(grid.substring(0,4));
  }
  if (gridFloat == 0)  {
  // none
    lcd.print(" ");
    lcd.print(grid.substring(0,4));
    Serial.print(grid.substring(0,4));
  }
  lcd.print("kW       ");
  Serial.println(" kW");
// usage
  lcd.setCursor(9,1);
  lcd.write(2);
  lcd.print(usage.substring(0,4));
  lcd.print("kW");
  Serial.print("Usage: ");
  Serial.print(usage.substring(0,4));
  Serial.println(" kW");
  oldBatteryValue = batteryInt;
}  

//to parse json data recieved from OWM
void parseJson(const char * jsonString) {
  //StaticJsonBuffer<4000> jsonBuffer;
  const size_t bufferSize = 2*JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + 4*JSON_OBJECT_SIZE(1) + 3*JSON_OBJECT_SIZE(2) + 3*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 2*JSON_OBJECT_SIZE(7) + 2*JSON_OBJECT_SIZE(8) + 720;
  DynamicJsonDocument jsonDoc(bufferSize);

  // FIND FIELDS IN JSON TREE
  auto error= deserializeJson(jsonDoc,jsonString);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  String battery = jsonDoc["battery"];
  String solar = jsonDoc["solar"];
  String grid = jsonDoc["grid"];
  String usage = jsonDoc["usage"];
  String timestamp = jsonDoc["timestamp"];

  // no point in doing anything unless the value has changed
  if ( timestamp != oldTimestampValue) {
    Serial.print("Data updated at: ");
    Serial.println(timestamp); 
    lcdShow(solar, battery, grid, usage);
    oldTimestampValue=timestamp;
  }
  else {
    Serial.println("No new data yet");
  }
}




void setup() {
  
  Serial.begin(115200);
  delay(1000); // wait for a second to let Serial connect
  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin();

  // start the LCD business
  lcd.createChar(0, solarsprite);
  lcd.createChar(1, gridsprite);
  lcd.createChar(2, mainssprite);
  lcd.createChar(4, upsprite);
  lcd.createChar(5, downsprite);

  lcd.begin(16, 2);  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcdbright(DIM);
  


}


void loop() {

// Do the framework business
    WiFiManager.loop();
    updater.loop();
    configManager.loop();

// check for button press 
  if (buttonPoll.previous==0 || (millis() - buttonPoll.previous > buttonPoll.rate )) {
    buttonPoll.previous = millis();
    buttonValue = analogRead(A0); // get the button value
    if (buttonValue == 1024) {
        if (buttonPress.previous==0 || (millis() - buttonPress.previous > buttonPress.rate )) {
// reset everything
      lcdbright(DIM);
      }
      }
    else {
      buttonPress.previous=millis();
    }
    if ((buttonValue < 1024) && (buttonValue > 900)) {
      lcdbright(MED);      
    }
  }

// Do the polling of the website business
  if (connectWeb.previous==0 || (millis() - connectWeb.previous > connectWeb.rate )) {
      time_t curTime=time(nullptr);
      Serial.print ("Time is: ");
      char curTimestamp [30];
      sprintf(curTimestamp, "%s, %01d %s %04d %02d:%02d:%02d %s",
               dayShortStr(weekday(curTime)), day(curTime), monthShortStr(month(curTime)),
              year(curTime), hour(curTime), minute(curTime), second(curTime), "GMT");
      Serial.println(curTimestamp);
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
      
      String response = "";

      fetch.GET(solarURL);
      
      while (fetch.busy())  {
            if (fetch.available())
            {
                response = fetch.readString();
            }
        }
        fetch.clean();

   if (response != "" ) {
     Serial.println(response);
     parseJson(response.c_str());
   }           


  connectWeb.previous = millis();
  }
}