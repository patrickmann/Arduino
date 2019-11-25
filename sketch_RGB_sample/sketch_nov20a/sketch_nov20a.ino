#include <Wire.h>
 #include <rgb_lcd.h>


 rgb_lcd lcd; // Instanz der Display-Schnittstelle erstellen
 
 byte HEART_SYMBOL = 0; // Sprechender Name für das Symbol
 byte heart[8] = { // Die Belegung der einzelnen Pixel
    0b00000,
    0b01010,
    0b11111,
    0b11111,
    0b11111,
    0b01110,
    0b00100,
    0b00000
 };
 
 void setup(){
  lcd.begin(16, 2); // Display initialisieren - 2 Zeilen mit jeweils 16 Zeichen
  lcd.createChar(HEART_SYMBOL, heart); // Herz-Symbol registrieren
 }
 
 void loop() {
  int red = 255;
  int green = 0;
  int blue = 0;
  lcd.setRGB(red, green, blue); 
  lcd.setCursor(0, 0); // Cursor im Display auf den Anfang der ersten Zeile setzen
  lcd.print("I ");
  lcd.write(HEART_SYMBOL);
  lcd.print(" Arduino");
  lcd.setCursor(0, 1); // Cursor im Display auf den Anfang der zweiten Zeile setzen
  lcd.print("===========");
 }
 
