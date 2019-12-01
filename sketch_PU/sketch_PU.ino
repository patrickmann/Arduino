#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS1307.h>
#include <rgb_lcd.h>

const int sensorCount = 5;
const DeviceAddress sensors[sensorCount] = {  // Previously determined
  {0x28, 0xAA, 0x98, 0xDA, 0x53, 0x14, 0x01, 0x08},
  {0x28, 0xAA, 0xE5, 0x3D, 0x54, 0x14, 0x01, 0x3C},
  {0x28, 0xAA, 0x3D, 0x3E, 0x54, 0x14, 0x01, 0xD4},
  {0x28, 0xAA, 0xBF, 0x3E, 0x54, 0x14, 0x01, 0x9C},
  {0x28, 0x9A, 0xA5, 0xCF, 0x5E, 0x14, 0x01, 0x24}
};
const char* sensorNames[sensorCount] = { "bath", "bottle 1", "bottle 2", "bottle 3", "bottle 4"};
const int bottleMinIndex = 1, bottleMaxIndex = 4;
float lastTemp[sensorCount] = {999, 999, 999, 999, 999};
float currTemp[sensorCount] = {999, 999, 999, 999, 999};
float diffTemp[sensorCount] = {0, 0, 0, 0, 0};
float totalPu[sensorCount] = {0, 0, 0, 0, 0};
bool sensorValid[sensorCount] = {true, true, true, true, true};
int sensorFlagged[sensorCount] = {0, 0, 0, 0, 0};
const int maxStrikes = 3; // invalidate sensor after maxStrikes problems

// Setup a oneWire instance to communicate with any OneWire devices
#define ONE_WIRE_BUS_PIN 2
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature dallas(&oneWire);

DS1307 clock; // RTC clock object
rgb_lcd lcd;  // Display object

const float minPasteurTemp = 40; // 60
const float startStopAccumulating = 35; // 50 start or stop cycle when passing through this temp
const float bottleMaxDiff = 5; // threshold to detect unused sensor
const float sensorThreshold = 1; // threshold to enable sensor
const float maxTemp = 80; // alert if exceeded
const float puTarget = 60; //delta PU = 1/60 * 10 ^ ((T-60)/7)
const int waitInterval = 10000; // polling interval while waiting for cycle to start
const float measureInterval = 5000; // polling interval during cycle
const float fractionalMin = measureInterval / 60000; // measure interval in minutes

float bottleTemp;
float minPu;
long lastMillis;
long currMillis;
long diffMillis;

enum State {initializing, waiting, accumulating, cooling} state = initializing;
bool errorState = false;
char sprintfBuffer[80];

void setup() {
  clock.begin();
  lcd.begin(16, 2); // Display initialisieren - 2 Zeilen mit jeweils 16 Zeichen

  Serial.begin(9600);
  Serial.print("Initializing Temperature Control Library Version ");
  Serial.println(DALLASTEMPLIBVERSION);
  dallas.begin();

  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  for (int i = 0 ; i < sensorCount; i++) {
    dallas.setResolution(sensors[i], 10);
  }
}

void loop() {
  Serial.println(" *********** Cycle Reset ************");
  if (!errorState) {
    // Don't erase prior error color
    lcd.setRGB(128, 128, 128);
  }
  state = waiting;
  snapTemp();
  bottleTemp = bottleMinTemp(currTemp);

  while (bottleTemp < startStopAccumulating) {
    Serial.print(bottleTemp);
    Serial.println("C: Waiting to warm up ...");
    lcdPrint1("Warming up");
    sprintf(sprintfBuffer, "%sC", tempToStr(bottleTemp));
    lcdPrint2(sprintfBuffer);

    delay(waitInterval);
    snapTemp();
    snapDiff();
    sensorCheck();
    bottleTemp = bottleMinTemp(currTemp);
  }
  
  Serial.println("************ Cycle start: accumulating PUs **************");
  errorState = false;
  lcd.setColor(BLUE);
  if (nValidSensors() < 2) {
    Serial.println("ERROR - starting pasteurization with insufficient sensors!");
    lcd.setColor(RED);
    errorState = true;
    delay(10000);
  }

  lastMillis = millis();
  currMillis = lastMillis;
  diffMillis = 0;
  minPu = 0;
  state = accumulating;
  while (accumulating == state) {
    snapPu();

    lcdPrint1("Pasteurizing");
    sprintf(sprintfBuffer, "%sC   %d PUs", tempToStr(bottleTemp), (int)minPu);
    lcdPrint2(sprintfBuffer);

    if (minPu >= puTarget) {
      state = cooling;
      Serial.println(" *************** Cycle end: target PU reached *****************");
      if (!errorState) {
        lcd.setColor(GREEN);
      }
      lcdPrint1("Done - cool it!");
    }

    else if (bottleTemp < startStopAccumulating) {
      state = waiting;
      Serial.println(" ********* Cycle interrupted prematurely ************");
      lcd.setColor(RED);
      errorState = true;
      lcdPrint("Aborted", "prematurely");
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
}

// Delay for measure interval, then get temp and PUs
void snapPu() {
  snapTemp();
  snapDiff();
  sensorCheck();
  bottleTemp = bottleMinTemp(currTemp);

  // Ensure consistent measure intervals of length measureInterval!
  // Results will be badly wrong, if this is not accurate.
  currMillis = millis();
  diffMillis = currMillis - lastMillis;
  int delayMillis = measureInterval - diffMillis;
  if (delayMillis < 0) {
    delayMillis = 0;
    Serial.print("WARNING measure interval exceeded - neg. delay");
  }
  else {
    Serial.print("Delaying: "); Serial.println(measureInterval - diffMillis);
    delay(measureInterval - diffMillis);
  }

  minPu = addPu();
  lastMillis = millis(); // timestamp immediately after processing the last interval
  Serial.print("Min PU="); Serial.print(minPu);
  Serial.print("   PU: ");
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    Serial.print(totalPu[i]);
    Serial.print("  ");
  }
  Serial.println();

  sprintf(sprintfBuffer, "%sC   %d PUs", tempToStr(bottleTemp), (int)minPu);
  lcdPrint2(sprintfBuffer);
}

void snapTemp() {
  for (int i = 0; i < sensorCount; i++) {
    lastTemp[i] = currTemp[i];
    currTemp[i] = getTemperature(i);
  }
  Serial.println();
}

void snapDiff() {
  for (int i = 0; i < sensorCount; i++) {
    diffTemp[i] = currTemp[i] - lastTemp[i];
    if (diffTemp[i] < 0) {
      diffTemp[i] *= -1;
    }
  }
}

// Increment PUs and return the lowest valid total PU value
float addPu() {
  float minPu = 999;
  snapTemp();
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    if (sensorValid[i]) {
      float deltaPu = fractionalMin * pow(10, ((currTemp[i] - minPasteurTemp) / 7));
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
    Serial.println("High discrepancy - seeking unused sensors");
    Serial.print(max - min);
    Serial.println("C");

    for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
      if (sensorValid[i]) {
        if (currTemp[i] - min < sensorThreshold) {
          // stuck at low temp - assuming not in use
          sensorValid[i] = false;
          Serial.print("Invalidated stuck sensor ");
          printTemp(i, currTemp[i]);
          Serial.println("*****");
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
        Serial.print("Re-enabling sensor ");
        printTemp(i, currTemp[i]);
        Serial.println("*****");
      }
    }
  }
}

int nValidSensors() {
  int nValid = 0;
  for (int i = bottleMinIndex; i <= bottleMaxIndex; i++) {
    if (sensorValid[i]) {
      nValid++;
    }
  }
  Serial.print("Valid sensors="); Serial.println(nValid);
  
  // Need at least 2 valid sensors for high confidence results
  if (nValid < 2) {
    Serial.println("WARNING less than 2 valid bottle sensors!");
  }
  return nValid; 
}

float getTemperature(int index) {
  dallas.requestTemperaturesByAddress(sensors[index]);
  float tempC = dallas.getTempC(sensors[index]);
  printTemp(index, tempC);

  if (-127.00 == tempC) {
    Serial.print("ERROR reading ");
    Serial.println(sensorNames[index]);
    sensorFlagged[index]++;
    if (sensorFlagged[index] > maxStrikes) {
      sensorValid[index] = false;
      Serial.print("Invalidated disconnected sensor ");
      printTemp(index, tempC);
      Serial.println("*****");
    }
  }
  return tempC;
}

// convert temperature float to string
// assume a format like this: 999.99
char conversionBuffer[6];
char* tempToStr(float temp) {
  //dtostrf(floatvar, StringLengthIncDecimalPoint, numVarsAfterDecimal, charbuf);
  dtostrf(temp, 6, 2, conversionBuffer);
  return conversionBuffer;
}

void printTemp (int index, float temp) {
  Serial.print(sensorNames[index]);
  Serial.print(": ");
  Serial.print(temp);
  Serial.print("C   ");
}

void lcdPrint(const char* line1, const char* line2) {
  lcdPrint1(line1);
  lcdPrint2(line2);
}

void lcdPrint1(const char* line1) {
  lcd.clear();
  lcd.setCursor(0, 0); // Cursor im Display auf den Anfang der ersten Zeile setzen
  lcd.print(line1);
}

void lcdPrint2(const char* line2) {
  lcd.setCursor(0, 1); // Cursor im Display auf den Anfang der zweiten Zeile setzen
  lcd.print(line2);
}

//Date;Time;TotalPU;Bath;Bottle1;Bottle2;Bottle3;Bottle4;PU1;PU2;PU3;PU4
//2019-11-27;8:55:32;0,00 ;16,00 ;15,00 ;15,00 ;15,00 ;15,00 ;0,10 ;0,20 ;0,30 ;0,40
//2019-11-27;8:55:33;0,00 ;16,00 ;15,00 ;15,00 ;15,00 ;15,00 ;0,10 ;0,20 ;0,30 ;0,40
