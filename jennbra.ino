#include "LPD8806.h"
// XXX: The library should successfully include this itself.
#include "SPI.h"

////////////////////////////////////////////////////////////////
// DEVICE-SPECIFIC DEFINES
////////////////////////////////////////////////////////////////
#define dataPin 6
#define clockPin 9
/*#define width 13
// -1 indicates no actual pixel at that virtual location.
int8_t nodeLayout[] = {
  -1, -1, 10, -1, -1, -1,  -1,  -1, -1, -1, 17, -1, -1,
  -1,  9, -1, 11, -1, -1,  -1,  -1, -1, 16, -1, 18, -1,
   2, -1,  8, -1, 12, -1,  -1,  -1, 15, -1, 19, -1, 25,
  -1,  3, -1,  7, -1, 13,  -1,  14, -1, 20, -1, 24, -1,
   1, -1,  4, -1,  6, -1,  -1,  -1, 21, -1, 23, -1, 26,
  -1,  0, -1,  5, -1, -1,  -1,  -1, -1, 22, -1, 27, -1,
};*/
#define width 3
int8_t nodeLayout[] = {
  -1,  2, -1,
   1, -1,  3,
  -1,  0, -1,
};


#define numnodes (sizeof(nodeLayout) / sizeof(int8_t))
#define height (numnodes / width)

// First parameter is the number of LEDs in the strand.  The LED strips
// are 32 LEDs per meter but you can extend or cut the strip.  Next two
// parameters are SPI data and clock pins:
LPD8806 strip = LPD8806(numnodes, dataPin, clockPin);

// You can optionally use hardware SPI for faster writes, just leave out
// the data and clock pin parameters.  But this does limit use to very
// specific pins on the Arduino.  For "classic" Arduinos (Uno, Duemilanove,
// etc.), data = pin 11, clock = pin 13.  For Arduino Mega, data = pin 51,
// clock = pin 52.  For 32u4 Breakout Board+ and Teensy, data = pin B2,
// clock = pin B1.  For Leonardo, this can ONLY be done on the ICSP pins.
//LPD8806 strip = LPD8806(numnodes);

void printArray(int8_t* arr, uint8_t len, uint8_t w) {
  for (int i = 0; i < len; i++) {
    if (!(i % w)) {
      Serial.print("\n");
    }
    Serial.print(arr[i]);
    Serial.print(", ");
  }
  Serial.print("\n");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Yello");
  printArray(nodeLayout, numnodes, width);

  int8_t tempLayout[numnodes];
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      tempLayout[height - 1 - j + i * height] = nodeLayout[i + j * width];
    }
  }
  memcpy(nodeLayout, tempLayout, numnodes * sizeof(int8_t));

  printArray(nodeLayout, numnodes, height);

  // Start up the LED strip
  strip.begin();

  // Update the strip, to start they are all 'off'
  strip.show();
}

void set(uint8_t x, uint8_t y, uint32_t color) {
  if (nodeLayout[x * height + y] >= 0) {
/*    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.print(" translates to ");
    Serial.println(nodeLayout[x * height + y]);*/
    strip.setPixelColor(nodeLayout[x * height + y], color);
  }
}

void fillCol(uint8_t col, uint32_t color) {
  for (int j = 0; j < height; j++) {
    set(col, j, color);
  }
}

void loop() {

  for (int i = 0; i < width; i++) {
    fillCol(i, strip.Color(127, 0, 0));
    strip.show();
    delay(100);
    fillCol(i, strip.Color(0, 0, 0));
  }
  for (int i = width - 2; i > 0; i--) {
    fillCol(i, strip.Color(127, 0, 0));
    strip.show();
    delay(100);
    fillCol(i, strip.Color(0, 0, 0));
  }

  // Send a simple pixel chase in...
  /*colorChase(strip.Color(127, 127, 127), 50); // White
  colorChase(strip.Color(127,   0,   0), 50); // Red
  colorChase(strip.Color(127, 127,   0), 50); // Yellow
  colorChase(strip.Color(  0, 127,   0), 50); // Green
  colorChase(strip.Color(  0, 127, 127), 50); // Cyan
  colorChase(strip.Color(  0,   0, 127), 50); // Blue
  colorChase(strip.Color(127,   0, 127), 50); // Violet*/

  // Fill the entire strip with...
  /*colorWipe(strip.Color(127,   0,   0), 50);  // Red
  colorWipe(strip.Color(  0, 127,   0), 50);  // Green
  colorWipe(strip.Color(  0,   0, 127), 50);  // Blue

  //rainbow(10);
  while (1) rainbowCycle(0);  // make it go through the cycle fairly fast*/
}

void rainbow(uint8_t wait) {
  int i, j;

  for (j=0; j < 384; j++) {     // 3 cycles of all 384 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (i + j) % 384));
    }
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Slightly different, this one makes the rainbow wheel equally distributed
// along the chain
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j=0; j < 384 * 5; j++) {     // 5 cycles of all 384 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      // tricky math! we use each pixel as a fraction of the full 384-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 384 is to make the wheel cycle around
      strip.setPixelColor(i, Wheel( ((i * 384 / strip.numPixels()) + j) % 384) );
    }
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Fill the dots progressively along the strip.
void colorWipe(uint32_t c, uint8_t wait) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

// Chase one dot down the full strip.
void colorChase(uint32_t c, uint8_t wait) {
  int i;

  // Start by turning all pixels off:
  for(i=0; i<strip.numPixels(); i++) strip.setPixelColor(i, 0);

  // Then display one pixel at a time:
  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c); // Set new pixel 'on'
    strip.show();              // Refresh LED states
    strip.setPixelColor(i, 0); // Erase pixel, but don't refresh!
    delay(wait);
  }

  strip.show(); // Refresh to turn off last pixel
}

/* Helper functions */

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g -b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  byte r, g, b;
  switch(WheelPos / 128)
  {
    case 0:
      r = 127 - WheelPos % 128;   //Red down
      g = WheelPos % 128;      // Green up
      b = 0;                  //blue off
      break;
    case 1:
      g = 127 - WheelPos % 128;  //green down
      b = WheelPos % 128;      //blue up
      r = 0;                  //red off
      break;
    case 2:
      b = 127 - WheelPos % 128;  //blue down
      r = WheelPos % 128;      //red up
      g = 0;                  //green off
      break;
  }
  return(strip.Color(r,g,b));
}
