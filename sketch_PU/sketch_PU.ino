#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS1307.h>
#include <rgb_lcd.h>
#include <SPI.h>

#include <SdFat.h>

#define P(x) (__FlashStringHelper*)(x)    // F-Makro für Variablen

SdFat SD;
File dataFile;

const byte buzzerPin = 6;

const byte sensorCount = 5;
const DeviceAddress sensors[sensorCount] = {  // Previously determined
  {0x28, 0xAA, 0x98, 0xDA, 0x53, 0x14, 0x01, 0x08},
  {0x28, 0xAA, 0xE5, 0x3D, 0x54, 0x14, 0x01, 0x3C},
  {0x28, 0xAA, 0x3D, 0x3E, 0x54, 0x14, 0x01, 0xD4},
  {0x28, 0xAA, 0xBF, 0x3E, 0x54, 0x14, 0x01, 0x9C},
  {0x28, 0x9A, 0xA5, 0xCF, 0x5E, 0x14, 0x01, 0x24}
};
const char* sensorNames[sensorCount] = { "w", "b1", "b2", "b3", "b4"};
const byte bottleMinIndex = 1, bottleMaxIndex = 4;
float lastTemp[sensorCount] = {999, 999, 999, 999, 999};
float currTemp[sensorCount] = {999, 999, 999, 999, 999};
float totalPu[sensorCount] = {0, 0, 0, 0, 0}; // totalPu[0] is not used, but simplifies things
bool sensorValid[sensorCount] = {true, true, true, true, true};

// Setup a oneWire instance to communicate with any OneWire devices
#define ONE_WIRE_BUS_PIN 2
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature dallas(&oneWire);

DS1307 clock; // RTC clock object
rgb_lcd lcd;  // Display object

const float startStopAccumulating = 50; // 50 start or stop cycle when passing through this temp
const float bottleMaxDiff = 5; // threshold in degrees C to detect unused sensor
const float sensorThreshold = 1; // threshold in degrees C to enable sensor
const float maxTemp = 80; // alert if exceeded
const float puTarget = 60; //delta PU = 1/60 * 10 ^ ((T-60)/7)
const int waitInterval = 10000; // polling interval while waiting for cycle to start
const float measureInterval = 5000; // polling interval during cycle

float bottleTemp;
float minPu;
long lastMillis;
long currMillis;
long diffMillis;

enum State {initializing, waiting, accumulating, cooling} state = initializing;
bool errorState = false;
char sprintfBuffer[80];
char logFileName[11]; //yymmdd.csv 


// convert temperature float to string
// assume a format like this: 999.99
char conversionBuffer[7];
char* tempToStr(float temp) {
  //dtostrf(floatvar, StringLengthIncDecimalPoint, numVarsAfterDecimal, charbuf);
  dtostrf(temp, 6, 2, conversionBuffer);
  return conversionBuffer;
}

void buzz(int duration, int repeat) {
  for (int i=0; i<=repeat; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(duration);
    digitalWrite(buzzerPin, LOW);
    delay(duration);
  }
}

void buzzError() {
  buzz(80,10);
}

void buzzAlert() {
  buzz(1000,1);
}

void snapToSD() {
  //Date;Time;TotalPU;Bath;Bottle1;Bottle2;Bottle3;Bottle4;PU1;PU2;PU3;PU4
  //2019-11-27;8:55:32;0,00 ;16,00 ;15,00 ;15,00 ;15,00 ;15,00 ;0,10 ;0,20 ;0,30 ;0,40
  //2019-11-27;8:55:33;0,00 ;16,00 ;15,00 ;15,00 ;15,00 ;15,00 ;0,10 ;0,20 ;0,30 ;0,40

  char buffer[90];
  clock.getTime();
  sprintf(buffer, "20%02d-%02d-%02d;%d:%d:%d;", 
    clock.year, clock.month, clock.dayOfMonth,
    clock.hour, clock.minute, clock.second);
    
  strcat(buffer, tempToStr(minPu)); strcat(buffer, ";");

  strcat(buffer, tempToStr(currTemp[0])); strcat(buffer, ";");
  strcat(buffer, tempToStr(currTemp[1])); strcat(buffer, ";");
  strcat(buffer, tempToStr(currTemp[2])); strcat(buffer, ";");
  strcat(buffer, tempToStr(currTemp[3])); strcat(buffer, ";");
  strcat(buffer, tempToStr(currTemp[4])); strcat(buffer, ";");

  strcat(buffer, tempToStr(totalPu[1])); strcat(buffer, ";");
  strcat(buffer, tempToStr(totalPu[2])); strcat(buffer, ";");
  strcat(buffer, tempToStr(totalPu[3])); strcat(buffer, ";");
  strcat(buffer, tempToStr(totalPu[4])); 

  writeToSD(buffer);
}
  
// Delay for measure interval, then get temp and PUs
void snapPu() {
  snapTemp();
  sensorCheck();
  bottleTemp = bottleMinTemp(currTemp);

  // Ensure consistent measure intervals of length measureInterval!
  // Results will be badly wrong, if this is not accurate.
  currMillis = millis();
  diffMillis = currMillis - lastMillis;
  int delayMillis = measureInterval - diffMillis;
  if (delayMillis < 0) {
    delayMillis = 0;
    Serial.print(F("WARNING neg. delay"));
  }
  else {
    //Serial.print(F("Delaying: ")); Serial.println(measureInterval - diffMillis);
    delay(measureInterval - diffMillis);
  }

  minPu = addPu();
  lastMillis = millis(); // timestamp immediately after processing the last interval
  Serial.print(F("Min PU=")); Serial.print(minPu);
  Serial.print(F("   PU: "));
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    Serial.print(totalPu[i]);
    Serial.print(F("  "));
  }
  Serial.println();

  sprintf(sprintfBuffer, "%sC   %d PUs", tempToStr(bottleTemp), (int)minPu);
  lcdPrint2(sprintfBuffer);
  snapToSD();
}

void snapTemp() {
  for (int i = 0; i < sensorCount; i++) {
    lastTemp[i] = currTemp[i];
    currTemp[i] = getTemperature(i);
  }
  Serial.println();
}

// Increment PUs and return the lowest valid total PU value
float addPu() {
  const float minPasteurTemp = 60; // 60
  const float fractionalMin = measureInterval / 60000; // measure interval in minutes
  
  float minPu = 999;
  snapTemp();
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    if (sensorValid[i]) {
      float avgTemp = (currTemp[i] + lastTemp[i]) / 2;
      float deltaPu = fractionalMin * pow(10, ((avgTemp - minPasteurTemp) / 7));
      totalPu[i] += deltaPu;
      if (totalPu[i] < minPu) {
        minPu = totalPu[i];
      }
    }
  }
  return minPu;
}

float bottleMaxTemp(float tempArray[]) {
  float max = -999;
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    if (sensorValid[i] && tempArray[i] > max) {
      max = tempArray[i];
    }
  }
  return max;
}

float bottleMinTemp(float tempArray[]) {
  return findMin(tempArray, bottleMinIndex, bottleMaxIndex);
}

float findMin(float array[], int start, int end) {
  float min = 999;
  for (int i = start; i <= end; i++) {
    if (sensorValid[i] && array[i] < min) {
      min = array[i];
    }
  }
  return min;
}

// Sensor sanity check:
// - ignore sensors not being used
// - handle delayed immersion into bath
// - but never enable a sensor during a running PU cycle
void sensorCheck() {
  float min = bottleMinTemp(currTemp);
  float max = bottleMaxTemp(currTemp);
  if (max - min > bottleMaxDiff) {
    Serial.println(F("High discrepancy - seeking unused sensors"));
    Serial.print(max - min);
    Serial.println(F("C"));

    for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
      if (sensorValid[i]) {
        if (currTemp[i] - min < sensorThreshold) {
          // stuck at low temp - assuming not in use
          sensorValid[i] = false;
          Serial.print(F("Invalidated stuck sensor "));
          printTemp(i, currTemp[i]);
          Serial.println();
        }
      }
    }
  }

  else if (waiting == state) {
    // No discrepancy - recheck for delayed immersion and re-enable sensor
    // But only if we have not already started tracking PUs!
    for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
      if (!sensorValid[i] && (max - currTemp[i] < sensorThreshold)) {
        sensorValid[i] = true;
        Serial.print(F("Re-enabling sensor "));
        printTemp(i, currTemp[i]);
        Serial.println();
      }
    }
  }
}

float getTemperature(int index) {
  dallas.requestTemperaturesByAddress(sensors[index]);
  float tempC = dallas.getTempC(sensors[index]);
  printTemp(index, tempC);

  if (-127.00 == tempC) {
    Serial.print(F("ERROR reading ")); Serial.println(sensorNames[index]);
    sensorValid[index] = false;
    Serial.println(F("Invalidated disconnected sensor"));
  }
  return tempC;
}

void printTemp (int index, float temp) {
  Serial.print(sensorNames[index]);
  Serial.print(F(": "));
  Serial.print(temp);
  Serial.print(F("C   "));
}

void lcdPrint(const __FlashStringHelper* line1, const char* line2) {
  lcdPrint1(line1);
  lcdPrint2(line2);
}

void lcdPrint1(const __FlashStringHelper* line1) {
  lcd.clear();
  lcd.setCursor(0, 0); // Cursor im Display auf den Anfang der ersten Zeile setzen
  lcd.print(line1);
}

void lcdPrint2(const char* line2) {
  lcd.setCursor(0, 1); // Cursor im Display auf den Anfang der zweiten Zeile setzen
  lcd.print(line2);
}

bool writeToSD(const char* data) {
  //Serial.println(data);
  if (1 > dataFile.println(data)) {
    Serial.print(F("Error writing to SD: ")); Serial.println(logFileName);
    buzzError();
    return false;
  }      
  dataFile.sync();    
  return true;
}

bool initSD() {
   if (!SD.begin(4)) // selected chip = 4
    return false;
    
  clock.begin();
  clock.getTime();
  sprintf(logFileName, "%02d%02d%02d.csv", clock.year, clock.month, clock.dayOfMonth); 
  dataFile = SD.open(logFileName, FILE_WRITE);
  if (!dataFile) {
    Serial.println(F("Error opening SD datafile"));
    return false;
  }

  if (!writeToSD("Date;Time;TotalPU;Bath;B1;B2;B3;B4;PU1;PU2;PU3;PU4")) 
    return false;
    
  return true;
}

// Abort execution
void halt(const __FlashStringHelper* msg) {
  buzzError();
  dataFile.close();
  Serial.println(msg);

  lcdPrint1(msg);
  lcd.setRGB(255,0,0); //RED

  delay(999999);
}

int nValidSensors() {
  int nValid = 0;
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    if (sensorValid[i]) {
      nValid++;
    }
  }
  Serial.print(F("Valid sensors=")); Serial.println(nValid);

  if (nValid < 1) {
    halt(F("No sensors"));
  }
  else if (nValid < 2) {
    // Need at least 2 valid sensors for high confidence results
    Serial.println(F("WARNING less than 2 valid bottle sensors!"));
  }
  
  return nValid;
}

void setup() {  
  pinMode(buzzerPin, OUTPUT);

  lcd.begin(16, 2); // Display initialisieren - 2 Zeilen mit jeweils 16 Zeichen
  Serial.begin(9600);

  // Temperature sensors
  dallas.begin();
  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  for (int i = 0 ; i < sensorCount; i++) {
    dallas.setResolution(sensors[i], 10);
  }
}

void loop() {
  if (initSD()) {
    Serial.println(F("SD-Card initialized."));
  }
  else {
    halt(F("SD-Card error"));
  }
  
  Serial.println(F("*** Cycle Reset"));
  buzzAlert();
  if (!errorState) {
    // Don't erase prior error color
    lcd.setRGB(64,64,255);
  }
  state = waiting;
  snapTemp();
  bottleTemp = bottleMinTemp(currTemp);

  while (bottleTemp < startStopAccumulating) {
    Serial.print(bottleTemp);
    Serial.println(F("C: Waiting to warm up ..."));
    lcdPrint1(F("Warming up"));
    sprintf(sprintfBuffer, "%sC", tempToStr(bottleTemp));
    lcdPrint2(sprintfBuffer);

    delay(waitInterval);
    snapTemp();
    sensorCheck();
    bottleTemp = bottleMinTemp(currTemp);
  }

  Serial.println(F("*** Cycle start: accumulating"));
  errorState = false;
  lcd.setRGB(255,255,255);
  if (nValidSensors() < 2) {
    buzzError();
    Serial.println(F("ERROR - insufficient sensors!"));
    lcd.setRGB(255,0,0);
    errorState = true;
  }
  else {
    buzzAlert();
  }
  
  lastMillis = millis();
  currMillis = lastMillis;
  diffMillis = 0;
  minPu = 0;
  state = accumulating;
  while (accumulating == state) {
    snapPu();

    lcdPrint1(F("Pasteurizing"));
    sprintf(sprintfBuffer, "%sC   %d PUs", tempToStr(bottleTemp), (int)minPu);
    lcdPrint2(sprintfBuffer);

    if (minPu >= puTarget) {
      state = cooling;
      Serial.println(F(" *** Cycle end: target PU reached"));
      if (!errorState) {
        lcd.setRGB(0,255,0);
        buzzAlert();
      }
      lcdPrint1(F("Done - cool it!"));
    }

    else if (bottleTemp < startStopAccumulating) {
      state = waiting;
      Serial.println(F(" *** Cycle interrupted prematurely"));
      buzzError();
      lcd.setRGB(255,0,0);
      errorState = true;
      lcdPrint(F("Aborted"), "prematurely");
      delay(10000);
    }
  }

  // Continue adding PUs during cooldown phase
  while (cooling == state) {
    snapPu();
    if (bottleTemp < startStopAccumulating) {
      state = waiting;
    }
  }
  dataFile.close();
}
