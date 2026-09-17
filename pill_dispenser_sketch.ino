/*-----A few things to note:
- I was unavailable to work on the pill dispenser during the last two semesters, so Komal did most of the work you see here before she graduated. 
Unfortunately it seems like she didn't reach out for help for certain topics like EEPROM and the Hall Effect sensor.
- The EEPROM library is for the internal module, not the external one that will be used. You can ignore the code related to that for now.
- I do want to reimplement the Hall Effect sensor, since the module's current limit is adjustable.
*/

const int STEP_PIN = 4;   // D4 -> DRV8825 STEP
const int DIR_PIN  = 3;   // D3 -> DRV8825 DIR
float fractionalSteps = 0.0;

#define HALL_PIN 7
#include <Wire.h>
#include <ArduinoHttpClient.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <EEPROM.h> 
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// ---------- CONFIG ----------

const char* ssid = "SSID"; // You'll have to change this to the network name you'll be using to interface with the Arduino board.
const char* password = "password"; //Same thing for the password.
const char* server = "android-supportive-housing.onrender.com"; // VPS server address
//const char* server = "165.232.143.201";   // VPS server address
//int port = 3000;
int port = 433
String device_id = "pill_3";
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ---------- EEPROM STRUCT ----------

struct ScheduleData {

  uint32_t dispense_at;  // epoch seconds
  uint32_t pill_id;

};

WiFiSSLClient wifi;

const int EEPROM_COUNT_ADDR = 0;
const int EEPROM_DATA_START = 1;


// ---------- EEPROM MANAGEMENT FUNCTIONS ----------

// Remove a schedule entry from EEPROM and compact remaining entries

void removeScheduleFromEEPROM(int index) {
  int scheduleCount = EEPROM.read(EEPROM_COUNT_ADDR);

  // Validate index

  if (index < 0 || index >= scheduleCount) {
    Serial.println("Invalid index for removal");
    return;

  }


  // Shift all subsequent entries down by one position

  for (int i = index; i < scheduleCount - 1; i++) {

    int srcAddr = EEPROM_DATA_START + (i + 1) * sizeof(ScheduleData);
    int destAddr = EEPROM_DATA_START + i * sizeof(ScheduleData);


    ScheduleData data;
    EEPROM.get(srcAddr, data);
    EEPROM.put(destAddr, data);

  }


  // Decrement the schedule count

  EEPROM.write(EEPROM_COUNT_ADDR, scheduleCount - 1);

  Serial.print("Removed schedule at index ");
  Serial.print(index);
  Serial.print(", count now: ");
  Serial.println(scheduleCount - 1);
}


// ---------- LCD DISPLAY FUNCTIONS ----------


// Find the next upcoming pill time (earliest scheduled)

// Returns 0 if no pills are scheduled

uint32_t getNextPillTime() {

  int scheduleCount = EEPROM.read(EEPROM_COUNT_ADDR);
  uint32_t nextTime = 0;

  for (int i = 0; i < scheduleCount; i++) {

    int baseAddr = EEPROM_DATA_START + i * sizeof(ScheduleData);
    ScheduleData data;
    EEPROM.get(baseAddr, data);

    // Find the earliest scheduled time

    if (nextTime == 0 || data.dispense_at < nextTime) {
      nextTime = data.dispense_at;
    }
  }

  return nextTime;
}


// Update LCD line 2 with next pill time or status message

void updateLCDStatus() {

  lcd.setCursor(0, 1);
  lcd.print("                    "); // Clear line 2 (20 chars)
  lcd.setCursor(0, 1);
  uint32_t nextTime = getNextPillTime();

  if (nextTime == 0) {

    lcd.print("No pills pending");
    
  } else {

    DateTime next(nextTime - 25200);
    char timeStr[20];
    sprintf(timeStr, "Next pill: %02d:%02d", next.hour(), next.minute());
    lcd.print(timeStr);
  }
}


// Show dispensing message on LCD

void showDispensingMessage() {

  lcd.setCursor(0, 1);
  lcd.print("                    "); // Clear line 2
  lcd.setCursor(0, 1);
  lcd.print("Dispensing...");
}


// ---------- SETUP ----------

void setup() {

  // Motor pin setup

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  Serial.begin(9600);

  while (!Serial);

  Serial.println("Booting...");
  Wire.begin();
//I commented out the code related to the RTC module, as I didn't have it on-hand while testing initially and it was causing the rest of the program to freeze.
/*

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }else{
    Serial.println("rtc found");
  } */

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pill Dispenser");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");


  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

  }
  Serial.println("\nWiFi connected!");
  
//Will likely uncomment this after integrating the RTC module.
/*

  // Set RTC time to compile time (first upload only)
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } */

  clearEEPROM();

  EEPROM.write(EEPROM_COUNT_ADDR, 0); // clear schedules on boot (optional)

  // Update LCD with initial status

  updateLCDStatus();
}


// ---------- LOOP ----------

void loop() {

  Serial.println("LOOP RUNNING");

  pollForSchedules();      // get schedules from VPS
  checkSchedules();
  updateLCDStatus();       // refresh LCD with next pill time
  delay(5000);             // poll every 5 seconds

      if (currentTime > 0 && curentTime >= data.dispense_at) {

    // PDT is UTC minus 7 hours (25200 seconds)

    unsigned long localTime = currentTime - 25200;

   

    DateTime now(localTime); // Use the adjusted local time

   

    Serial.print("Current Time (PDT): ");
    Serial.print(now.year());
    Serial.print('/');
    Serial.print(now.month());
    Serial.print('/');
    Serial.print(now.day());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(':');

    // Add a leading zero to minutes if it's less than 10 (e.g., 12:05)

    if(now.minute() < 10) Serial.print('0');
    Serial.print(now.minute());
    Serial.print(':');

    if(now.second() < 10) Serial.print('0');
    Serial.println(now.second());

  }

    // Create the localTime variable here so this function knows what it is!

    unsigned long localTime = currentTime - 25200;
    Serial.print("Current Local Epoch: ");
    Serial.println(localTime);

}


void clearEEPROM() {

  for (int i = 0; i < 256; i++) {
    EEPROM.write(i, 0);

  }
}


// ---------- POLLING ----------

void pollForSchedules() {

  Serial.println("Polling for schedules...");
  HttpClient client = HttpClient(wifi, server, port);
  client.get("/api/wait-for-schedule?deviceId=" + device_id);
  int status = client.responseStatusCode();
  String body = client.responseBody();
  Serial.print("Status: ");
  Serial.println(status);
  Serial.print("Body: ");
  Serial.println(body);


  if (status == 200 && body.length() > 0) {

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {

      Serial.println("Failed to parse schedule JSON");
      return;

    }


    JsonArray schedules = doc["schedules"];
    for (JsonObject schedule : schedules) {

      Serial.println("Printing Schedules...");
      serializeJson(schedule, Serial);
      Serial.println();
      uint32_t dispenseAt = schedule["dispense_time"];
      uint32_t pill_id = schedule["pill_id"];
      saveScheduleToEEPROM(dispenseAt, pill_id);

    }


    // hasSchedule = true;

  } else if (status == 204) {

    Serial.println("No schedules yet.");

  } else {

    Serial.println("Poll failed.");

  }


  client.stop();

}


void saveScheduleToEEPROM(uint32_t dispenseAt, uint32_t pill_id) {

  int index = EEPROM.read(EEPROM_COUNT_ADDR);
  int baseAddr = EEPROM_DATA_START + index * sizeof(ScheduleData);
  ScheduleData data;
  data.dispense_at = dispenseAt;
  data.pill_id = pill_id;
  EEPROM.put(baseAddr, data);
  EEPROM.write(EEPROM_COUNT_ADDR, index + 1);
  Serial.print("Saved schedule at epoch: ");
  Serial.println(dispenseAt);
  Serial.print("for pill id: ");
  Serial.println(pill_id);

}


void checkSchedules() {

  Serial.println("reading from eeprom");

  int i = 0;


  while (i < EEPROM.read(EEPROM_COUNT_ADDR)) {

    int baseAddr = EEPROM_DATA_START + i * sizeof(ScheduleData);
    ScheduleData data;
    EEPROM.get(baseAddr, data);
    Serial.println("got data from eeprom");
    Serial.print("Schedule time for Epoch: ");
    Serial.println(data.dispense_at);


    // Ask the Wi-Fi chip what the current internet time is

    unsigned long currentTime = WiFi.getTime();


    //if (rtc.now().unixtime() >= data.dispense_at) {

    if (currentTime > 0 && currentTime >= data.dispense_at) {

      Serial.print("Dispensing pill ");
      Serial.println(data.pill_id);

      // Show dispensing message on LCD
      showDispensingMessage();
      postDispensedPills(data.dispense_at, data.pill_id);
      motorMovement();
      // Remove the dispensed schedule from EEPROM and compact
      removeScheduleFromEEPROM(i);
      // Refresh LCD to show next pill time after dispensing
      updateLCDStatus();
      // Don't increment i - next entry has shifted to current index

    } else {

      i++;  // Only increment if we didn't remove an entry

    }
  }
}

void motorMovement() {

  // Wheel has 15 slots: 360 / 15 = 24 degrees per slot
  // Motor step angle: 1.8 degrees per step
  // Steps needed: 24 / 1.8 = 13.33 ≈ 13 steps
  // I think I've resolved the issue with the compartment wheel becoming misaligned, but just in case
  // I plan on changing this to work with a 10-slotted wheel. Stick with this version for now.

  int stepsToTake = 14;
  fractionalSteps += 0.2857;

  if (fractionalSteps >= 1.0) {

    stepsToTake = 15;
    fractionalSteps -= 1.0;

  }

  digitalWrite(DIR_PIN, HIGH);

  Serial.print("MOTOR moving ");
  Serial.print(stepsToTake);
  Serial.println(" steps");

  for (int i = 0; i < stepsToTake; i++) {

    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1000);

  }

}

void postDispensedPills(uint32_t dispense_at, uint32_t pill_id) {

  HttpClient client(wifi, server, port);

Serial.println("post calll");
  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["pill_id"] = pill_id;
  doc["dispense_time"] = dispense_at;

  String jsonBody;

  serializeJson(doc, jsonBody);

  Serial.println(jsonBody);
  client.beginRequest();
  client.post("/api/pill-dispensed");
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", jsonBody.length());
  client.beginBody();
  client.print(jsonBody);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  Serial.print("HTTP Status Code: ");
  Serial.println(statusCode);
  client.stop();

} 
