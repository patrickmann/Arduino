# PU Meter
Sketch to power a device to measure Pasteurization Units in a water bath pasteurizer. 
The thresholds and pasteurization targets are based on hard cider; but you can easily modify them for other applications.
You will also need to replace the current sensor addresses with your own. 

PUs are calculated according to this formula:
>delta PU per second = 1/60 * 10 ^ ((T-60)/7)

**Disclaimer: use entirely at your own risk. Accuracy and validity of results is NOT guaranteed.**

## How it works
After startup the PU meter waits for temperature to increase to measuring threshold. It then starts adding up PUs and alerts
when the target threshold is reached. We track PU accummulation both as temperature rises and falls. I.e. product may be removed
from the bath before hitting the target PU, taking advantage of PU accummulation during cooldown, while still obtaining accurate PU totals.

A core feature is support for multiple sensors to achieve high confidence results. All sensors must agree closely with each other. 
This protects against cold spots in the water bath, where product is receiving fewer PUs. The code is also resilient to broken sensors or sensors that are not in use (i.e. not immersed). In particular, a pasteurization cycle will still be completed successfully if a sensor fails during the run - as long as at least 1 working sensor remains. 

Smart detection of sensors allows user to insert sensors one at a time. As long as the setup is complete before hitting the 
threshold for PU accummulation, all sensors will be used for PU measurement. Sensors inserted after PU accummulation has started will be ignored to avoid erroneous results. If a sensor fails during pasteurizing, we also ignore the associated PU data. Thresholds tests are based on the lowest valid temperature and PU values across all sensors - this keeps us on the safe side when there are cool spots in the bath.

## Features
* Compute PUs from temperature readings
* Compare readings across multiple sensors and check for consistency - require at least 2 sensors for sanity checking
* Support arbitrary number of sensors (subject to memory limitations)
* Identify and ignore unused (active but not immersed) and disconnected sensors
* Output CSV data log with date and time stamps to SD card for import to Excel
* Output data points to serial
* Output user prompts and data to LCD
* Alert user via buzzer to important state transitions or errors

## Some flagged error states
* Not enough sensors for high confidence result - need at least 2 product sensors
* Pasteurization cycle aborted prematurely 
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
