#define BLYNK_TEMPLATE_ID "TMPL3fSYgW9wX"
#define BLYNK_TEMPLATE_NAME "Battery Management system"
#define BLYNK_AUTH_TOKEN "OQUjUVNxads1O2zG_mFVkwksuEHCRx29"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

BlynkTimer timer;
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define RELAY_PIN 25
#define BUZZER_PIN 26

bool faultActive = false;
unsigned long faultClearTime = 0;
const unsigned long recoveryDelay = 5000; // 5 seconds

int lcdPage = 0;

unsigned long lcdPreviousMillis = 0;
const unsigned long lcdInterval = 2000;
bool relayState = true;
#define CELL1 34
#define CELL2 35
#define CELL3 32
#define CELL4 33

unsigned long previousMillis = 0;
const unsigned long interval = 1000;

float cellVoltage[4];
enum SystemMode {
  NORMAL,
  DEGRADED,
  FAILSAFE,
  SHUTDOWN
};

SystemMode currentMode = NORMAL;

unsigned long faultTimestamp = 0;
int adcValue[4];
int previousVoltage[4] = {0,0,0,0};
int freezeCount[4] = {0,0,0,0};

void setup() {
  Serial.begin(115200);

Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.begin(115200);
  lcd.init();
lcd.backlight();

lcd.setCursor(0, 0);
lcd.print("Battery System");
lcd.setCursor(0, 1);
lcd.print("Starting...");
  pinMode(RELAY_PIN, OUTPUT);
pinMode(BUZZER_PIN, OUTPUT);

digitalWrite(RELAY_PIN, HIGH);   // Relay ON (normal operation)
digitalWrite(BUZZER_PIN, LOW);   // Buzzer OFF
}

float readCellVoltage(int pin) {
  int adcValue = analogRead(pin);

  // Convert ADC value to voltage (ESP32 ADC 0-4095, 3.3V reference)
  float voltage = (adcValue / 4095.0) * 3.3;

  // Scale for lithium cell simulation
  voltage = voltage * 1.25;

  return voltage;
}

void loop() {
  Blynk.run();
timer.run();
  unsigned long currentMillis = millis();

if (currentMillis - previousMillis < interval) {
  return;
}

previousMillis = currentMillis;

  cellVoltage[0] = readCellVoltage(CELL1);
  cellVoltage[1] = readCellVoltage(CELL2);
  cellVoltage[2] = readCellVoltage(CELL3);
  cellVoltage[3] = readCellVoltage(CELL4);
  // Sensor Disconnection Detection
bool sensorFault = false;

for(int i = 0; i < 4; i++) {

  if(cellVoltage[i] <= 0.1) {

    sensorFault = true;
    faultTimestamp = millis();

    Serial.print("Sensor Fault: Cell ");
    Serial.print(i + 1);
    Serial.println(" Disconnected");
  }

  if(cellVoltage[i] > 4.5) {

    sensorFault = true;
    faultTimestamp = millis();

    Serial.print("Invalid Reading: Cell ");
    Serial.print(i + 1);
    Serial.println(" Voltage Too High");
  }

  if(cellVoltage[i] == previousVoltage[i]) {

    freezeCount[i]++;

    if(freezeCount[i] > 5) {

      sensorFault = true;
      faultTimestamp = millis();

      Serial.print("Frozen ADC Fault: Cell ");
      Serial.println(i + 1);
    }

  } else {

    freezeCount[i] = 0;
  }

  previousVoltage[i] = cellVoltage[i];



}

  float packVoltage = 0;

  float maxCell = cellVoltage[0];
  float minCell = cellVoltage[0];

  int maxIndex = 0;
  int minIndex = 0;

  for(int i=0;i<4;i++) {

    packVoltage += cellVoltage[i];

    if(cellVoltage[i] > maxCell){
      maxCell = cellVoltage[i];
      maxIndex = i;
    }

    if(cellVoltage[i] < minCell){
      minCell = cellVoltage[i];
      minIndex = i;
    }
  }

  float average = packVoltage / 4;

  float imbalance = ((maxCell - minCell) / average) * 100;
  // Safety Protection Logic
  // Runtime System Mode Logic

if (sensorFault) {

  currentMode = SHUTDOWN;
  faultTimestamp = millis();

}

else if (minCell <= 0.1) {

  currentMode = SHUTDOWN;
  faultTimestamp = millis();

}

else if (imbalance > 10 || maxCell > 4.2 || minCell < 3.2) {

  currentMode = FAILSAFE;
  faultTimestamp = millis();

}

else if (imbalance > 5) {

  currentMode = DEGRADED;

}

else {

  currentMode = NORMAL;

}
bool batteryFault = (sensorFault || minCell < 3.2 || maxCell > 4.2 || imbalance > 10);


if (batteryFault) {

  faultActive = true;
  faultClearTime = millis();

  relayState = false;

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("FAULT: Protection Active");

}

else {

  if (faultActive && millis() - faultClearTime < recoveryDelay) {

    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);

    Serial.println("Recovery Waiting...");

  }

  else {

    faultActive = false;
    relayState = true;

    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    Serial.println("SYSTEM NORMAL");

  }
}

  String health;

  if(imbalance < 2)
    health = "Healthy";

  else if(imbalance < 5)
    health = "Minor Imbalance";

  else if(imbalance < 10)
    health = "Critical Imbalance";

  else
    health = "Pack Failure";


  Serial.println("---- Battery Report ----");

  for(int i=0;i<4;i++){
    Serial.print("Cell ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print(cellVoltage[i]);
    Serial.println(" V");
  }

  Serial.print("Pack Voltage: ");
  Serial.print(packVoltage);
  Serial.println(" V");

  Serial.print("Average Voltage: ");
  Serial.print(average);
  Serial.println(" V");

  Serial.print("Imbalance: ");
  Serial.print(imbalance);
  Serial.println("%");

  Serial.print("Weakest Cell: Cell ");
  Serial.println(minIndex+1);

  Serial.print("Strongest Cell: Cell ");
  Serial.println(maxIndex+1);

  Serial.print("Battery Health: ");
  Serial.println(health);
  Serial.print("System Mode: ");

switch(currentMode) {

  case NORMAL:
    Serial.println("NORMAL");
    break;

  case DEGRADED:
    Serial.println("DEGRADED");
    break;

  case FAILSAFE:
    Serial.println("FAILSAFE");
    break;

  case SHUTDOWN:
    Serial.println("SHUTDOWN");
    break;
}

  String modeText;

switch(currentMode) {
  case NORMAL:
    modeText = "NORMAL";
    break;

  case DEGRADED:
    modeText = "DEGRADED";
    break;

  case FAILSAFE:
    modeText = "FAILSAFE";
    break;

  case SHUTDOWN:
    modeText = "SHUTDOWN";
    break;
}
Serial.println();
  Serial.print("Fault Status: ");
Serial.println(faultActive ? "ACTIVE" : "CLEAR");
// Send data to Blynk
Blynk.virtualWrite(V0, cellVoltage[0]);
Blynk.virtualWrite(V1, cellVoltage[1]);
Blynk.virtualWrite(V2, cellVoltage[2]);
Blynk.virtualWrite(V3, cellVoltage[3]);
Blynk.virtualWrite(V4, packVoltage);
Blynk.virtualWrite(V5, health);
Blynk.virtualWrite(V6, faultActive);
Blynk.virtualWrite(V7, average);
Blynk.virtualWrite(V8, imbalance);
Blynk.virtualWrite(V9, "Cell " + String(maxIndex + 1));
Blynk.virtualWrite(V10, "Cell " + String(minIndex + 1));
Blynk.virtualWrite(V11, modeText);
Blynk.virtualWrite(V12, relayState ? "ON" : "OFF");
String recommendation;

if(faultActive)
  recommendation = "Check Battery Fault";
else if(imbalance > 5)
  recommendation = "Balance Cells";
else
  recommendation = "System Normal";

Blynk.virtualWrite(V13, recommendation);

  
if (millis() - lcdPreviousMillis >= lcdInterval) {

  lcdPreviousMillis = millis();

  lcd.clear();

  if(faultActive) {

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("!! FAULT !!");

  lcd.setCursor(0,1);
  lcd.print("Protection ON");

  return;
}
if(lcdPage == 0) {

    lcd.setCursor(0,0);
    lcd.print("C1:");
    lcd.print(cellVoltage[0],1);
    lcd.print(" C2:");
    lcd.print(cellVoltage[1],1);

    lcd.setCursor(0,1);
    lcd.print("C3:");
    lcd.print(cellVoltage[2],1);
    lcd.print(" C4:");
    lcd.print(cellVoltage[3],1);
  }

  else if(lcdPage == 1) {

    lcd.setCursor(0,0);
    lcd.print("Pack:");
    lcd.print(packVoltage,1);
    lcd.print("V");

    lcd.setCursor(0,1);
    lcd.print("Avg:");
    lcd.print(average,1);
    lcd.print("V");
  }

  else if(lcdPage == 2) {

    lcd.setCursor(0,0);
    lcd.print("Health:");

    lcd.setCursor(0,1);
    lcd.print(health);
  }

  else if(lcdPage == 3) {

    lcd.setCursor(0,0);
    lcd.print("Fault:");

    lcd.setCursor(0,1);

    if(faultActive)
      lcd.print("ACTIVE");
    else
      lcd.print("CLEAR");
  }

  lcdPage++;

  if(lcdPage > 3)
    lcdPage = 0;
}
}