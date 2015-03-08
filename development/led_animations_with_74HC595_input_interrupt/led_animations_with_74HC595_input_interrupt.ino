/*
 *  LED Animations With 74HC595 & Input Interrupt.
 *
 *  Copyright (C) 2010 Efstathios Chatzikyriakidis (contact@efxa.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// include for portable "non-local" jumps.
#include <setjmp.h>

const int buttonPin = 2;   // the pin number of the button element.
const int buttonIRQ = 0;   // the IRQ number of the button pin.
const long delayTime = 50; // delay time in millis for the leds.

const int BOUNCE_DURATION = 200; // define a bounce time for the button.

volatile int buttonState = 0; // the default animation to start showing.
volatile long bounceTime = 0; // variable to hold ms count to debounce a pressed switch.

// number of leds to control.
const int NUM_LEDS = 8;

// used for single LED manipulation.
int ledState = LOW;

// information to restore calling environment.
jmp_buf buf;

// define some new type name definitions.
typedef void (*animFunc) (void);

// array of animation functions' pointers.
const animFunc ledAnims[] = { fr_one_at_a_time,
                              blink_one_at_a_time,
                              bk_one_at_a_time,
                              blink_all_together,
                              fb_one_at_a_time,
                              loading_effect,
                              fb_all_together,
                              all_anims_together /* this should be the last one */ };

// calculate the number of the animations in the array.
const int NUM_ANIMS = (int) (sizeof (ledAnims) / sizeof (const animFunc));

// set 74HC595 8-bit shift register control pin numbers.
const int data = 7;
const int clock = 3; // PWM pin.
const int latch = 4;

// these are used in the bitwise math that we use to change individual LEDs.

// each specific bit set to 1.
int bits[] = { B00000001, B00000010,
               B00000100, B00001000,
               B00010000, B00100000,
               B01000000, B10000000 };

// each specific bit set to 0.
int masks[] = { B11111110, B11111101,
                B11111011, B11110111,
                B11101111, B11011111,
                B10111111, B01111111 };

// startup point entry (runs once).
void setup() {
  // set button element as an input.
  pinMode(buttonPin, INPUT);

  // set 74HC595 control pins as output.
  pinMode (data, OUTPUT);
  pinMode (clock, OUTPUT);  
  pinMode (latch, OUTPUT);  

  // attach an ISR for the IRQ (for button presses).
  attachInterrupt(buttonIRQ, buttonISR, RISING);
}

// loop the main sketch.
void loop()
{
  // save the environment of the calling function.
  setjmp(buf);

  // dark all the leds before any animation start.
  for (int i = 0; i < NUM_LEDS; i++)
    changeLED (i, LOW);

  // run led animation according to button state.
  ledAnims[buttonState]();
}

// ISR for the button IRQ (is called on button presses).
void buttonISR () {
  // it ignores presses intervals less than the bounce time.
  if (abs(millis() - bounceTime) > BOUNCE_DURATION) {
    // go to next state.
    buttonState++;

    // check bounds for the state of the button (repeat).
    if (buttonState > NUM_ANIMS-1)
      buttonState = 0;

    // set whatever bounce time in ms is appropriate.
    bounceTime = millis(); 

    // go to the main loop and start the next animation.
    longjmp(buf, 0);
  }
}

// animation #1.
void fr_one_at_a_time () {
  for (int i = 0; i < NUM_LEDS; i++) {
    changeLED (i, HIGH);
    delay (delayTime);
    changeLED (i, LOW);
  }
}

// animation #2.
void bk_one_at_a_time () {
  for (int i = NUM_LEDS-1; i >= 0; i--) {
    changeLED (i, HIGH);
    delay(delayTime);
    changeLED (i, LOW);
  }
}

// animation #3.
void fb_one_at_a_time () {
  for (int i = 0; i < NUM_LEDS; i++) {
    changeLED (i, HIGH);
    delay(delayTime);
    changeLED (i, LOW);
  }

  for (int i = NUM_LEDS-1; i >= 0 ; i--) {
    changeLED (i, HIGH);
    delay(delayTime);
    changeLED (i, LOW);
  }
}

// animation #4.
void fb_all_together () {
  for (int i = 0; i < NUM_LEDS; i++) {
    changeLED (i, HIGH);
    delay(delayTime);
  }

  for (int i = NUM_LEDS-1; i >= 0 ; i--) {
    changeLED (i, LOW);
    delay(delayTime);
  }
}

// animation #5.
void blink_one_at_a_time () {
  for (int i = 0; i < NUM_LEDS; i++) {
    for (int j = 0; j < 10; j++) {
      changeLED (i, HIGH);
      delay(delayTime);
      changeLED (i, LOW);
      delay(delayTime);
    }
  }
}

// animation #6.
void blink_all_together () {
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < NUM_LEDS; i++)
      changeLED (i, HIGH);

    delay(delayTime);

    for (int i = 0; i < NUM_LEDS; i++)
      changeLED (i, LOW);

    delay(delayTime);
  }
}

// animation #7.
void loading_effect() {
  for (int i = 0; i < NUM_LEDS; i++) {
    changeLED (i, HIGH);
    delay(delayTime);
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    changeLED (i, LOW);
    delay(delayTime);
  }
}

// all animations.
void all_anims_together () {
  for (int i = 0; i < NUM_ANIMS-1; i++)
    ledAnims[i]();
}

// sends the LED states set in ledStates to the 74HC595 sequence.
void updateLEDs (int value) {
  // pulls the chips latch low.
  digitalWrite (latch, LOW);

  // shifts out the 8 bits to the shift register.
  shiftOut (data, clock, MSBFIRST, value);

  // pulls the latch high displaying the data.
  digitalWrite (latch, HIGH);
}

// changes an individual LED.
void changeLED (int led, int state) {
  // clears ledState of the bit we are addressing.
  ledState = ledState & masks[led];

  // if the bit is on we will add it to ledState.
  if (state == HIGH) {
    ledState = ledState | bits[led];
  }

  // send the new LED state to the shift register.
  updateLEDs(ledState);
}
