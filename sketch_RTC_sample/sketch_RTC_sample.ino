#include <Wire.h>
#include <DS1307.h>

DS1307 clock; // ein Uhr-Objekt anlegen

void setup() {
 Serial.begin(9600);

 // Die Verbindung zur Uhr aufbauen
 clock.begin();
 // Die Zeit einstellen
 clock.fillByYMD(2017, 9, 24);
 clock.fillByHMS(18, 53, 15);
 clock.fillDayOfWeek(SAT);
 clock.setTime();
}

void loop() {
 printTime();
 delay(1000);
}
void printTime() {
 clock.getTime(); // Zeit vom Chip abfragen
 Serial.print(clock.hour, DEC);
 Serial.print(":");
 Serial.print(clock.minute, DEC);
 Serial.print(":");
 Serial.print(clock.second, DEC);
 Serial.print(" | ");
 Serial.print(clock.dayOfMonth, DEC);
 Serial.print(".");
 Serial.print(clock.month, DEC);
 Serial.print(".");
 Serial.print(clock.year, DEC);
 Serial.println();
}
