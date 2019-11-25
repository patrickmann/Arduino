#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS1307.h>

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
float lastTemp[sensorCount] = { -99, -99, -99, -99, -99};

// Setup a oneWire instance to communicate with any OneWire devices
#define ONE_WIRE_BUS_PIN 2
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature dallas(&oneWire);

DS1307 clock; // ein Uhr-Objekt anlegen

enum State {initializing, waiting, accumulating} state = initializing;
const float minPasteurTemp = 30; // 60
const float startStopAccumulating = minPasteurTemp; // start or stop cycle when passing through this temp
const float maxBottleDiff = 3; // max allowable difference between bottle temps
const float maxTemp = 80; // alert if exceeded
const float puTarget = 60; //delta PU = 1/60 * 10 ^ ((T-60)/7)
const int waitInterval = 10000; // polling interval while waiting for cycle to start
const float measureInterval = 3000; // polling interval during cycle
const float fractionalMin = measureInterval / 60000; // measure interval in minutes

void setup() {
  // Die Verbindung zur Uhr aufbauen
  clock.begin();

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
  state = waiting;
  float bottleTemp = getMinBottleTemp();
  if (bottleTemp > startStopAccumulating) {
    Serial.println("ERROR: inital bottle temp too high");
  }

  while (bottleTemp < startStopAccumulating) {
    Serial.println("Waiting to start accumulating ...");
    delay(waitInterval);
    bottleTemp = getMinBottleTemp();
  }

  long lastMillis = millis();
  Serial.println(" ********************** Cycle start: accumulating PUs *************************");
  state = accumulating;
  float totalPu = 0;
  float deltaPu = 0;
  float lastBottleTemp = bottleTemp;
  while (accumulating == state) {
    bottleTemp = getMinBottleTemp();

    // Ensure consistent measure intervals!
    long currMillis = millis();
    long diffMillis = currMillis - lastMillis;
    Serial.print("Delaying: "); Serial.println(measureInterval - diffMillis);
    delay(measureInterval - diffMillis);

    deltaPu = fractionalMin * pow(10, ((bottleTemp - minPasteurTemp) / 7));
    totalPu += deltaPu;
    lastMillis = millis();

    Serial.print(lastMillis);
    Serial.print("     Total PU: ");
    Serial.print(totalPu);
    Serial.print("     Delta PU: ");
    Serial.println(deltaPu);

    if (totalPu >= puTarget) {
      state = waiting;
      Serial.println(" ********************** Cycle end: target PU reached *************************");
    }

    if (bottleTemp < startStopAccumulating) {
      state = waiting;
      Serial.println(" ************************* Cycle interrupted ******************************");
    }
  }
}

float getMinBottleTemp() {
  float minTemp = getTemperature(bottleMinIndex);
  float lastTemp = minTemp;

  for (int i = bottleMinIndex + 1; i <= bottleMaxIndex; i++) {
    float currTemp = getTemperature(i);
    float diff = currTemp - lastTemp;
    if (diff < 0) diff *= -1;
    if (diff > maxBottleDiff) {
      Serial.println("ERROR: excessive temperature discrepancy!!!");
      printTemp(i, currTemp);
      printTemp(i - 1, lastTemp);
      Serial.println();
    }

    if (currTemp < minTemp) {
      minTemp = currTemp;
    }
    lastTemp = currTemp;
  }
  Serial.println();
  Serial.print("Min bottle temp: ");
  Serial.println(minTemp);
  return minTemp;
}

float getTemperature(int index) {
  dallas.requestTemperaturesByAddress(sensors[index]); // Send the command to get temperatures
  float tempC = dallas.getTempC(sensors[index]);
  //printTemp(index, tempC);

  if (tempC == -127.00) {
    Serial.print("ERROR: error getting temperature for ");
    Serial.println(sensorNames[index]);
  }
  return tempC;
}

void printTemp (int index, float temp) {
  Serial.print(sensorNames[index]);
  Serial.print(": ");
  Serial.print(temp);
  Serial.print("   ");
}
