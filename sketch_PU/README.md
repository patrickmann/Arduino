# PU Meter
Sketch to power a device to measure Pasteurization Units in a water bath pasteurizer. 
The thresholds and pasteurization targets are based on hard cider; but you can easily modify them for other applications.

## How it works
After startup the PU meter waits for temperature to increase to measuring threshold. It then starts adding up PUs and alerts
when the target threshold is reached. We track PU accummulation both as temperature rises and falls; i.e. user may remove
material from the bath before hitting the target PU, taking advantage of PU accummulation during cooldown.

Smart detection of sensors allows user to insert sensors one at a time. As long as the setup is complete before hitting the 
threshold for PU accummulation, all sensors will be used for PU measurement.

## Features
* Compute PUs from temperature readings
* Compare readings across multiple sensors and check for consistency - require at least 2 sensors for sanity checking
* Identify and ignore unused or disconnected sensors
* Output CSV data log with date and time stamps to SD card for import to Excel
* Output data points to serial
* Output user prompts and data to LCD
* Alert user via buzzer to important state transitions or errors

## Some detected error states
* Not enough sensors - need at least 2
* Pasteurization cycle aborted prematurely 
* SD card not working
* Excessive difference between sensors
* Water too hot

## Hardware
* Arduino Uno Rev. 3, SMD-Variante, ATmega328, USB
* Arduino Shield - Base Shield V2
* Arduino Display Grove LCD mit RGB-Backlight
* Arduino Shield SD Card
* Arduino Grove RTC, DS1307
* 5 DS18B20 temperature sensors in water proof package
* Grove Buzzer
* Miscellaneous: Connectors, pull-up resistor for sensors, power supply, etc.
