#include <Wire.h>
#include <DS1307.h>

DS1307 clock; // ein Uhr-Objekt anlegen

void setup() {
 Serial.begin(9600);

 // Die Verbindung zur Uhr aufbauen
 clock.begin();
 // Die Zeit einstellen
 clock.fillByYMD(2019, 11, 30);
 clock.fillByHMS(16, 50, 00);
 clock.fillDayOfWeek(SAT);
 clock.setTime();
}

void loop() {
 printTime();
 delay(1000);
}
void printTime() {
 clock.getTime(); // Zeit vom Chip abfragen
 Serial.print(clock.hour);
 Serial.print(":");
 Serial.print(clock.minute);
 Serial.print(":");
 Serial.print(clock.second);
 Serial.print(" | ");
 Serial.print(clock.dayOfMonth);
 Serial.print(".");
 Serial.print(clock.month);
 Serial.print(".");
 Serial.print(clock.year);
 Serial.println();
}
