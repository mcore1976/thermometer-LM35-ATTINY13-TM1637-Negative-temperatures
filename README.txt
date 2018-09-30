
Digital LED thermometer on ATTINY13 and TM1637 4-digit LED Arduino module. 
Uses analog temperature sensor LM35 (for -50C - 100C degrees) with pullup diode 1N4007 to
enable negative temperature measurements 

How to connect

LM35 Vout to ATTINY13 PB4 pin
LM35 GND (from pullup diode) to ATTINY13 PB3 pin 
TM1637 DIO_PIN to ATTINY13 PB0 pin
TM1637 CLK_PIN to ATTINY13 PB1 pin

VCC (5V from USB or LM7805 stabilizer) and GND must be connected to both ATTINY13 and TM1637 module

5V from VCC is used as a reference for LM35/TM1637 measurement.

 

