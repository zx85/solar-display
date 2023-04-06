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
// json stuff is here https://arduinojson.org/v6/doc/upgrade/
// todo: https://github.com/tzapu/WiFiManager
// TODO: https and Solis API

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>

// set up the important globulars
// put the following in a file in include called settings.h
// (note: only works on htt at the moment)
// TODO: make it work with the Solis API with https magnificence
//
// char ssid[] = "YOURSSID";        
// char pass[] = "YOURWIFIPASSWORD"; 
// const char solarserver[] = "WEBSERVERIPADDRESS";
// const char solarpath[] = "/PATH/TO/FILE.json";

#include "settings.h"

// WiFi business
WiFiClient client;
int status = WL_IDLE_STATUS;

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

// Loop variables business
unsigned long lastConnectionTime = 10 * 60 * 1000;  // last time connected, in milliseconds
const unsigned long postInterval =  1 * 30 * 1000;  // posting interval of 30 seconds
unsigned long lastButtonPoll = 10 * 60 * 1000;      // last time button was checked in milliseconds
const unsigned long buttonPollInterval = 200;       // 200ms button poll
unsigned long lastButtonPress = 10 * 60 * 1000;     // last time a a button was pressed
const unsigned long buttonInterval = 5 * 1000;      // 5 seconds screen brightening
int oldButtonValue=0;
int oldBatteryValue=0;
String oldTimestampValue="";

// Button business
int buttonValue = 0;

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
  lcd.clear();
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

// print Wifi status
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  lcdbright(MED);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connected - IP:");
  lcd.setCursor(0,1);
  lcd.print(ip);
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  delay(3000);
  lcd.clear();
  lcdbright(DIM);
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

// to request data from OWM
void makehttpRequest() {
  // close any connection before send a new request to allow client make connection to server
  client.stop();

  Serial.println("Making the API call");
  // if there's a successful connection:
  if (client.connect(solarserver, 80)) {
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    client.print("GET ");
    client.print(solarpath);
    client.println(" HTTP/1.1");
    client.println("Host: 192.168.75.4");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    
    char c = 0;
    while (client.available()) {
      c = client.read();
      // since json contains equal number of open and close curly brackets, this means we can determine when a json is completely received  by counting
      // the open and close occurences,
      //     Serial.print(c);
      if (c == '{') {
        startJson = true;         // set startJson true to indicate json message has started
        jsonend++;
      }
      if (c == '}') {
        jsonend--;
      }
      if (startJson == true) {
        text += c;
      }
      // if jsonend = 0 then we have have received equal number of curly braces 
      if (jsonend == 0 && startJson == true) {
        parseJson(text.c_str());  // parse c string text in parseJson function
        text = "";                // clear text string for the next time
        startJson = false;        // set startJson to false to indicate that a new message has not yet started
      }
    delay(1); // random delay just because
    }
  }
  else {
    // if no connction was made:
    Serial.println("connection failed");
    return;
  }
}

void connectWiFi() {
  int cursorPosition=0;
  lcd.print(F("   Connecting"));  
  Serial.println(F("Setup starting"));
  WiFi.begin(ssid,pass);
  Serial.println("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(cursorPosition,2); 
    lcd.print(".");
    cursorPosition++;
  }
  Serial.println("WiFi Connected");
}


void setup() {
  
  Serial.begin(115200);
  delay(1000); // wait for a second to let Serial connect

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
  
  connectWiFi();
  printWiFiStatus();
}


void loop() {
// check for button press 
if (millis() - lastButtonPoll > buttonPollInterval) {
  lastButtonPoll = millis();
  buttonValue = analogRead(A0); // get the button value
  if (buttonValue == 1024) {
    if (millis() - lastButtonPress > buttonInterval) {
// reset everything
    lcdbright(DIM);
    }
    }
  else {
    lastButtonPress=millis();
  }
  if ((buttonValue < 1024) && (buttonValue > 900)) {
    lcdbright(MED);      
  }
}

if (millis() - lastConnectionTime > postInterval) {
    // note the time that the connection was made:
    lastConnectionTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconecting WiFi");
      connectWiFi();
      printWiFiStatus();
    }
    makehttpRequest();

oldButtonValue=buttonValue;

}

}