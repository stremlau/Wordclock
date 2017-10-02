Wordclock
=========

Arduino nano code for a WS2812B/Neopixel Wordclock.

HC-05 Bluetooth configuration 
-----------------------------
Connect Arduino Nano and HC-05 Module:  
VCC -- 5V  
GND -- GND  
TXD -- TX1  
RXD -- RX0  

Upload Sketch with empty setup() and loop(). Open Serial Monitor with "NL and CR" and 38400 Baud rate. Type the following:

* AT+CLASS=1 // hold HC-05 Button while pressing Enter
* AT+ROLE=0
* AT+NAME=Wordclock-\<NAME\>
* AT+PSWD=0000


