/*

	Cold Spark Machine DMX Control

	Could easily be modified for "whatever" DMX control

	Extra safety checks added becasue we don't want these going off accidentially or for too long

*/

#include <Arduino.h>
#include <esp_dmx.h>
#include <ezButton.h>
#include <ezLED.h>

#define DEBOUNCE_TIME 50

/*
	First, lets define the hardware pins that we are using with our ESP32. We
	need to define which pin is transmitting data and which pin is receiving data.
	DMX circuits also often need to be told when we are transmitting and when we
	are receiving data. We can do this by defining an enable pin.

	Make sure to double-check that these pins are compatible with your ESP32!
	Some ESP32s, such as the ESP32-WROVER series, do not allow you to read or
	write data on pins 16 or 17 (PSRAM), so it's always good to read the manuals. 
*/

int transmitPin = 25; 	// Goes to DI on the MAX485
int receivePin = 26;	// Goes to RO on the MAX485
int enablePin = 27;		// Goes to both RE & DE on the MAX485
						// MAX485 VCC = Goes to ESP32 5V pin or 5v rail
						// MAX485 GND = Goes to Pin 1 on the XLR and to the ESP32 GND or GND rail
						// MAX485   A = Goes to Pin 3 on the XLR
						// MAX485   B = Goes to Pin 2 on the XLR

/*
If you see PINS in your XLR socket:  1   2 (pins facing you)
                                       3

If you see HOLES in your XLR socket: 2   1 (holes facing you - holes are often marked with numbers)
                                       3
*/

// Pins for various things, status lights, etc.
//
int sensorPin = A0;   		// select the input pin for the potentiometer. Currently unused.
ezButton fire1(21);			// Trigger momentary button
ezButton preheat1(13);		// Preheat latching button
ezLED standby1_light(23);	// Light for "powered on but not preheating"
ezLED preheat1_light(19);	// Light for "preheating"
ezLED fire1_light(18);		// Light for "fire1" button

bool fire1enabled = false;
unsigned long fire1timeout = 0;
int fire1timelimit = 2000;

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

	Serial.begin(115200);
	delay(100);

	Serial.println("Spark Machine Contol Startup");

	// Set the DMX hardware pins to the pins that we want to use.
	//
	dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

	/* Now we can install the DMX driver! We'll tell it which DMX port to use and
	which interrupt priority it should have. If you aren't sure which interrupt
	priority to use, you can use the macro `DMX_DEFAULT_INTR_FLAG` to set the
	interrupt to its default settings.*/

	dmx_driver_install(dmxPort, DMX_DEFAULT_INTR_FLAGS);

	// Set the indicator lights to default to indicate logical state regardless of the physical button stste
	//
	fire1_light.turnOFF();
	preheat1_light.turnOFF();
	
	// slowly blink the standby so we know it's something important.
	// It'll keep blinking from boot until we intentionally get to "preheat"
	//
	standby1_light.blink(500,50);

}

void loop() {

	preheat1.loop();
	fire1.loop();

	fire1_light.loop();
	preheat1_light.loop();
	standby1_light.loop();

	// This is currently ignored but left for later use.
	//
	int height = map(analogRead(sensorPin), 0, 1023, 0, 255);

	if (fire1.isPressed()) {

		if (fire1enabled) { // don't run unless we're specifically armed.

			fire1_light.turnON(); 			// turn on the fire1 light so we know it's done something.
			preheat1_light.blink(75,75); 	// rapidly blink the preheat light to give visual feedback on the channel.

			if (data[2] == 0) Serial.println("Fire 1 ON");
			data[2] = 15;	// My machine has LOW/MED/HIGH aka F1/F2/F3. My values are 15-94, 95-174, 175-255. 
							// Send any number in the range. 15 is the same as 94, etc.

			fire1timeout = millis() + fire1timelimit;

			dmx_write(dmxPort, data, DMX_PACKET_SIZE);

		}

	}

	if (fire1.isReleased() || (fire1timeout < millis())) {

		if (data[2] != 0 && fire1timeout < millis()) {
			
			Serial.println("Fire 1 OFF (timeout)");

		} else if (data[2] != 0) {

			Serial.println("Fire 1 OFF");

		}

		fire1_light.turnOFF(); // turn off the fire1 light feedback.
		preheat1_light.cancel(); // turn off the preheat blinking feedback during firing.
		if (preheat1.getState() == LOW && fire1enabled) preheat1_light.turnON(); // turn the preheat light back on if we're in an enabled state.

		data[2] = 0;

		dmx_write(dmxPort, data, DMX_PACKET_SIZE);

	}

	// 0..10 = Preheat Off
	// 11..239 = Dead zone. Preheat stays on, but won't START to preheat here. Often machines have something in this range like "Clean" or "E-Stop", mine does not.
	// 240..255 = Start Preheat
	// Send any number in the range. 240 is the same as 255, etc.

	if (preheat1.isPressed()) {

		// Only turn on preheat by event. This stops prehating from triggering at boot if the button was left on at bootup. User needs to cycle if off and back on for safety.

		if (data[1] == 0) {
			
			Serial.println("Preheat 1 ON");
		
			preheat1_light.turnON();
			standby1_light.turnOFF();

		}

		data[1] = 255;

		fire1enabled = true; // enable fire1 only after we get the first button event to enable preheat. Stops machine firing before we turn on preheat.

		dmx_write(dmxPort, data, DMX_PACKET_SIZE);

	} 

	if (preheat1.isReleased()) {

		if (data[1] == 255) {
			
			Serial.println("Preheat 1 OFF");

			preheat1_light.turnOFF();
			standby1_light.turnON();

		}

		data[1] = 0;

		fire1enabled = false; // turn off fire1 for safety.

		dmx_write(dmxPort, data, DMX_PACKET_SIZE);

	}

	// Now we can transmit the DMX packet!
	dmx_send(dmxPort, DMX_PACKET_SIZE);

	// We can do some other work here if we want.

	// If we have no more work to do, we will wait until we are done sending our DMX packet.
	dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);

}
