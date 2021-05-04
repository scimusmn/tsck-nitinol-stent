// Adapted from Thermistor Example #3 from the Adafruit Learning System guide on Thermistors
// https://learn.adafruit.com/thermistor/overview by Limor Fried, Adafruit Industries
// MIT License - please keep attribution and consider buying parts from Adafruit/
// Written by D.Bailey Science Museum of Minnesota 9/2020

#include <LiquidCrystal.h>
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); // rs, en, d4, d5, d6, d7
#include "FlipflopTimer.h"
FlipflopTimer flipflopTimer1;
FlipflopTimer flipflopTimer2;
#include "averager.h"
averager averageReading(10); //take 10 samples

#define heatSSR 3
#define addHeatPB 4
#define coolDownPB 5
#define fan 6
#define heatOnLED 13
#define THERMISTORPIN A0 // analog input for thermistor
#define thermistorLow 15.0 //min safe thermistor reading in C before triggering out of range fault
#define thermistorHigh 50.0 //max safe thermistor reading in C before triggering out of range fault
#define THERMISTORNOMINAL 10000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT 3988 //TDK  B57863S0103F040 (The beta coefficient of the thermistor is usually 3000-4000)
#define SERIESRESISTOR 10000 // the value of the 'other' resistor
#define lcdRefreshRate 200 //lcd refresh rate in ms, slows down update of lcd to make it more readable
#define watchdogTimer 120000 //reset timer in ms
#define minTemp 30.0 //min temp, affects fan turn-off point
#define midTemp 38.0 //mid temp, affects when flipflopTimer1 starts
#define maxTemp 42.0 //max temp, affects when flipflopTimer2 starts

//variables
bool heating = false;
bool delayThermistorCheck = true;
float tempRead = 0.0;
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
long lagTime = 0;
float average = 0.0;
float steinhart = 0.0;
float displayTemp = 0.0;
float coldOffset = 0.0; //this can be used as an offset to compensate for temp difference at spring ends
float roundedTemp = 0.0; //display temp value on LCD
long startTime = 0;
long timeNow = 0; //current time
long timenow2 = 0; //current time
long lcdRefreshTimer = 0;

void thermistorFault() { //stay here if thermistor is disconnected or reading is out of range
  while (1);
}

void setup(void) {
  // Serial.begin(9600);
  flipflopTimer1.setup([](boolean flipflopValue) {
    digitalWrite(heatSSR, flipflopValue);
    digitalWrite(heatOnLED, flipflopValue);
  }, 100, 75);
  flipflopTimer2.setup([](boolean flipflopValue) {
    digitalWrite(heatSSR, flipflopValue);
    digitalWrite(heatOnLED , flipflopValue);
  }, 100, 400);
  analogReference(EXTERNAL); //set analog reference to 3.3V, using 3.3 reference improves s/n ratio
  pinMode(heatSSR, OUTPUT);
  pinMode(addHeatPB, INPUT_PULLUP);
  pinMode(coolDownPB, INPUT_PULLUP);
  pinMode(fan, OUTPUT); //fan
  pinMode(heatOnLED , OUTPUT);
  digitalWrite(heatSSR, LOW);
  lcd.begin(16, 2);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("Heat OFF");
  startTime = millis();
  digitalWrite(fan, HIGH);
}

void loop(void) {
  averageReading.idle(analogRead(THERMISTORPIN)); //get reading of thermistor add to running average

  //***********calculate temperature using simplifed Steinhart-Hart equation*********
  average = averageReading.ave;
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;
  steinhart = average / THERMISTORNOMINAL;    // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  //*********compensation for thermistor being mounted at cooler end of spring********
  tempRead = mapfloat(steinhart, 23.0, 29.0, 23.0, 35.5); //min and max temp change at thermistor, actual measured min and max temp change at center of spring
  displayTemp = tempRead;
  
  //*********make sure thermistor is reading temp in expected range, fault-out if not************
  if (millis() >= startTime + 2000) delayThermistorCheck = false; //wait 2 seconds after start-up, before checking thermistor
  if ((tempRead < thermistorLow || tempRead  > thermistorHigh ) && !delayThermistorCheck) {
    digitalWrite(heatSSR, LOW);
    lcd.clear();
    lcd.print("thermistor error");
    thermistorFault();
  }

  //*************heat or cool visitor button selection*************
  if (!digitalRead(addHeatPB) && digitalRead(coolDownPB) && !heating) { //add heat
    digitalWrite(heatSSR, HIGH);
    digitalWrite(fan, LOW); //turn off fan
    digitalWrite(heatOnLED , HIGH);
    heating = true;
    timeNow = millis();
    lcd.setCursor(0, 1);
    lcd.print("                ");//clear line 2
    lcd.setCursor(0, 1);
    lcd.print("Heat ON");
  }
  else if (!digitalRead(coolDownPB) && digitalRead(addHeatPB) && heating) { //cool down
    digitalWrite(heatSSR, LOW);
    digitalWrite(fan, HIGH);
    digitalWrite(heatOnLED , LOW);
    heating = false;
    lcd.setCursor(0, 1);
    lcd.print("                "); //clear line 2
    lcd.setCursor(0, 1);
    lcd.print("Heat OFF");
  }

  //*************regulation of spring temperature during heating*************
  if (tempRead < minTemp && heating) {
    digitalWrite(heatSSR, HIGH);
  }
  else if (tempRead  >= minTemp && tempRead  <= midTemp && heating) {
    flipflopTimer1.update();
  }
  else if (tempRead  > midTemp && tempRead <= maxTemp && heating) {
    flipflopTimer2.update();
  }
  else if (tempRead >= maxTemp - coldOffset && heating) {
    digitalWrite(heatSSR, LOW);
    digitalWrite(heatOnLED , LOW);
  }
  if (millis() >= timeNow + watchdogTimer && heating) { // 2 minute watchdog timer
    digitalWrite(heatSSR, LOW);
    digitalWrite(fan, HIGH);
    digitalWrite(heatOnLED , HIGH);
    heating = false;
    lcd.setCursor(0, 1);
    lcd.print("                "); //clear line 2
    lcd.setCursor(0, 1);
    lcd.print("Heat OFF");
  }

  //********update LCD display with current temperature rounded to nearest .5 degree**********
  if (millis() >= lcdRefreshTimer + lcdRefreshRate) { //only update lcd per the defined lcd refresh rate
    roundedTemp = 0.5 * round(2.0 * displayTemp) ;
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(roundedTemp, 1);
    lcd.print(" C        ");
    lcdRefreshTimer = millis();
  }
  if (roundedTemp <= minTemp) digitalWrite(fan, LOW);
}
