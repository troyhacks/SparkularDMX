/*

*/
#include <Arduino.h>
#include <esp_dmx.h>

/*
  - potentiometer
    center pin of the potentiometer to the analog input 0
    one side pin (either one) to ground
    the other side pin to +5V
*/
int sensorPin = A0;   // select the input pin for the potentiometer

int trigger1Pin = 10;

/* First, lets define the hardware pins that we are using with our ESP32. We
  need to define which pin is transmitting data and which pin is receiving data.
  DMX circuits also often need to be told when we are transmitting and when we
  are receiving data. We can do this by defining an enable pin. */
int transmitPin = 17;
int receivePin = 16;
int enablePin = 21;
/* Make sure to double-check that these pins are compatible with your ESP32!
  Some ESP32s, such as the ESP32-WROVER series, do not allow you to read or
  write data on pins 16 or 17, so it's always good to read the manuals. */

/* Next, lets decide which DMX port to use. The ESP32 has either 2 or 3 ports.
  Port 0 is typically used to transmit serial data back to your Serial Monitor,
  so we shouldn't use that port. Lets use port 1! */
dmx_port_t dmxPort = 1;

/* Now we want somewhere to store our DMX data. Since a single packet of DMX
  data can be up to 513 bytes long, we want our array to be at least that long.
  This library knows that the max DMX packet size is 513, so we can fill in the
  array size with `DMX_PACKET_SIZE`. */
byte data[DMX_PACKET_SIZE];


void setup() {
  /* Start the serial connection back to the computer so that we can log
    messages to the Serial Monitor. Lets set the baud rate to 115200. */
  Serial.begin(115200);

  /* Set the DMX hardware pins to the pins that we want to use. */
  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  /* Now we can install the DMX driver! We'll tell it which DMX port to use and
    which interrupt priority it should have. If you aren't sure which interrupt
    priority to use, you can use the macro `DMX_DEFAULT_INTR_FLAG` to set the
    interrupt to its default settings.*/
  dmx_driver_install(dmxPort, DMX_DEFAULT_INTR_FLAGS);
}

void loop() {

  int height = map(analogRead(sensorPin), 1023, 0, 255);
  bool trigger1 = digitalRead(trigger1Pin);
  if(trigger1 == HIGH) {
    Serial.println("Fire!");
    data[2] = height;
  }
  else {
    data[2] = 0;
  }

  bool preHeat = digitalRead(preheatPin);
  bool eStop = false; // digitalRead(eStopPin);
  bool clean = false;
  if(eStop) {
    data[1] = 100;
  }
  elseif(clean) {
    data[1] = 240;
  }
  elseif(preHeat) {
    data[1] = 255;
  }
  else {
    data[1] = 0;
  }
  

  /* Now we can transmit the DMX packet! */
  dmx_send(dmxPort, DMX_PACKET_SIZE);

  /* We can do some other work here if we want. */

  /* If we have no more work to do, we will wait until we are done sending our
    DMX packet. */
  dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
}
