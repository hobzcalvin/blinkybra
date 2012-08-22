#include "LPD8806.h"
// XXX: The LPD8806 library should successfully include this itself.
#include "SPI.h"

#include "PinChangeInt.h"
#include "TimerOne.h"
#include "fix_fft.h"

////////////////////////////////////////////////////////////////
// DEVICE-SPECIFIC DEFINES
////////////////////////////////////////////////////////////////
#define dataPin 10
#define clockPin 9
#define ledPin 13
#define b0pin 2
#define b1pin 3
#define b2pin 4
#define b3pin 5
#define b4pin 6
#define bGroundPin 7
#define soundGroundPin A2
#define soundVPin A0
#define soundPin A1

#define WIDTH 12
// -1 indicates no actual pixel at that virtual location.
int8_t nodeLayout[] = {
  -1,  1, -1, -1, -1, -1,   -1, -1, -1, -1, 27, -1,
   0, -1, 10, -1, -1, -1,   -1, -1, -1, 17, -1, 26,
  -1,  9, -1, 11, -1, -1,   -1, -1, 16, -1, 18, -1,
   2, -1,  8, -1, 12, -1,   -1, 15, -1, 19, -1, 25,
  -1,  3, -1,  7, -1, 13,   14, -1, 20, -1, 24, -1,
  -1, -1,  4, -1,  6, -1,   -1, 21, -1, 23, -1, -1,
  -1, -1, -1,  5, -1, -1,   -1, -1, 22, -1, -1, -1,
};
/*int8_t nodeLayout[] = {
  -1,  1, -1, -1, -1, -1,  -1,  -1, -1, -1, -1, 27, -1,
   0, -1, 10, -1, -1, -1,  -1,  -1, -1, -1, 17, -1, 26,
  -1,  9, -1, 11, -1, -1,  -1,  -1, -1, 16, -1, 18, -1,
   2, -1,  8, -1, 12, -1,  -1,  -1, 15, -1, 19, -1, 25,
  -1,  3, -1,  7, -1, 13,  -1,  14, -1, 20, -1, 24, -1,
  -1, -1,  4, -1,  6, -1,  -1,  -1, 21, -1, 23, -1, -1,
  -1, -1, -1,  5, -1, -1,  -1,  -1, -1, 22, -1, -1, -1,
};*/


#define numNodes (sizeof(nodeLayout) / sizeof(int8_t))
#define HEIGHT (numNodes / WIDTH)

typedef void (*mode_func)(void);

volatile bool mode_running = true;
volatile bool next_mode = false;
volatile byte speed = 0;
#define NUM_SPEEDS 6
volatile byte timerDoubler = 0;

#define FULL 127

LPD8806 strip = NULL;
uint8_t realNumNodes = 0;

// PROTOTYPES
byte gamma(byte x);
long hsv2rgb(long h, byte s, byte v);

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

void fill(uint32_t color) {
  for (int i = 0; i < realNumNodes; i++) {
    strip.setPixelColor(i, color);
  }
}

void clear() {
  fill(0);
}

void showFor(uint32_t ms) {
  strip.show();
  uint32_t target = millis() + ms;
  while (mode_running && millis() < target) { }
}

#define BOUNCE_DURATION 50
volatile unsigned long bounceTime = 0;
#define outsideBounce() (((millis() - bounceTime) > BOUNCE_DURATION) && (bounceTime = millis()))

// Right
void b0interrupt() {
  if (outsideBounce()) {
    mode_running = false;
    next_mode = true;
  }
}
// Up
void b1interrupt() {
  if (outsideBounce()) {
  }
}
// Left
void b2interrupt() {
  if (outsideBounce()) {
    speed = (speed + 1) % NUM_SPEEDS;
    mode_running = false;
  }
}
// Center
void b3interrupt() {
  if (outsideBounce()) {
    strip.downBrightness();
  }
}
// Down
void b4interrupt() {
  if (outsideBounce()) {
  }
}
void handleRandomTimer() {
  if (++timerDoubler % 2) {
    mode_running = false;
  }
}


void setup() {
  pinMode(ledPin, OUTPUT);

  // For convenience, waste a pin to give GROUND to the buttons.
  pinMode(bGroundPin, OUTPUT);
  digitalWrite(bGroundPin, LOW);
  // And waste 2 analog inputs.
  pinMode(soundGroundPin, OUTPUT);
  pinMode(soundVPin, OUTPUT);
  digitalWrite(soundGroundPin, LOW);
  digitalWrite(soundVPin, HIGH);

  pinMode(soundPin, INPUT);
  randomSeed(analogRead(soundPin));

  pinMode(b0pin, INPUT);
  pinMode(b1pin, INPUT);
  pinMode(b2pin, INPUT);
  pinMode(b3pin, INPUT);
  pinMode(b4pin, INPUT);

  digitalWrite(b0pin, HIGH);
  digitalWrite(b1pin, HIGH);
  digitalWrite(b2pin, HIGH);
  digitalWrite(b3pin, HIGH);
  digitalWrite(b4pin, HIGH);

  realNumNodes = 0;
  for (int i = 0; i < numNodes; i++) {
    if (nodeLayout[i] >= 0) {
      realNumNodes++;
    }
  }
  // First parameter is the number of LEDs in the strand.  The LED strips
  // are 32 LEDs per meter but you can extend or cut the strip.  Next two
  // parameters are SPI data and clock pins:
  strip = LPD8806(realNumNodes, dataPin, clockPin);
  Serial.begin(9600);

  int8_t tempLayout[numNodes];
  for (int i = 0; i < WIDTH; i++) {
    for (int j = 0; j < HEIGHT; j++) {
      tempLayout[HEIGHT - 1 - j + i * HEIGHT] = nodeLayout[i + j * WIDTH];
    }
  }
  memcpy(nodeLayout, tempLayout, numNodes * sizeof(int8_t));

  // Start up the LED strip
  strip.begin();

  // Update the strip, to start they are all 'off'
  strip.show();

  fill(strip.Color(127, 0, 0));
  digitalWrite(ledPin, HIGH);
  showFor(200);
  fill(strip.Color(0, 127, 0));
  digitalWrite(ledPin, LOW);
  showFor(200);
  fill(strip.Color(0, 0, 127));
  digitalWrite(ledPin, HIGH);
  showFor(200);
  fill(strip.Color(0, 0, 0));
  digitalWrite(ledPin, LOW);
  showFor(200);
  digitalWrite(ledPin, HIGH);

//  attachInterrupt(0, b0interrupt, FALLING);
//  attachInterrupt(1, b1interrupt, FALLING);
  PCintPort::attachInterrupt(b0pin, b0interrupt, FALLING);
  PCintPort::attachInterrupt(b1pin, b1interrupt, FALLING);
  PCintPort::attachInterrupt(b2pin, b2interrupt, FALLING);
  PCintPort::attachInterrupt(b3pin, b3interrupt, FALLING);
  PCintPort::attachInterrupt(b4pin, b4interrupt, FALLING);


  // XXX For now, start dimmer for powersave.
  strip.downBrightness();
  strip.downBrightness();

}

void set(uint8_t x, uint8_t y, uint32_t color) {
  if (nodeLayout[x * HEIGHT + y] >= 0) {
/*    Serial.print(x);
    Serial.print(",");
    Serial.print(y);
    Serial.print(" translates to ");
    Serial.println(nodeLayout[x * HEIGHT + y]);*/
    strip.setPixelColor(nodeLayout[x * HEIGHT + y], color);
  }
}

void fillCol(uint8_t col, uint32_t color) {
  for (int j = 0; j < HEIGHT; j++) {
    set(col, j, color);
  }
}

mode_func modes[] = {
  &spectrum,
  &randDots,
  &wheelPlus,
  &nightRide,
};
#define num_modes (sizeof(modes) / sizeof(mode_func))
#define WHILE_MODE(rand_cycles) while (mode_running)

void loop() {
  /*  START RANDOM
  // handleRandomTimer only acts every 2 times it's called, so this time value is doubled.
  Timer1.attachInterrupt(handleRandomTimer, 6000000);
  next_mode = false;
  while (!next_mode) {
    mode_running = true;
    clear();
    speed = random(NUM_SPEEDS);
    (*modes[random(num_modes)])();
  }
  Timer1.detachInterrupt();
  END RANDOM */
  for (byte i = 0; i < num_modes; ++i) {
    speed = 0;
    next_mode = false;
    while (!next_mode) {
      mode_running = true;
      clear();
      (*modes[i])();
    }
  }
}

int8_t spectrumLayout[] = {
   5,  4,  3,  2, -1,
   6,  7,  8,  9,  0, -1,
  13, 12, 11, 10,  1, -1,
  14, 15, 16, 17, 27, -1,
  21, 20, 19, 18, 26, -1,
  22, 23, 24, 25, -1,
};
#define BANDS 6
byte bandSizes[BANDS] = {
 1, 2, 2, 2, 2, 2
};
void spectrum() {
  char im[128];
  char data[128];
  int i, curBand;
  int val;
  double bands[BANDS];
  byte bandHeights[BANDS] = { 0 };

  double avg = 0;

  // Count the height of each band so we know how many LEDs to divide between.
  for (int curBand = 0, i = 0;
       curBand < BANDS && i < (sizeof(spectrumLayout) * sizeof(int8_t));
       i++) {
    if (spectrumLayout[i] < 0) {
      curBand++;
    } else {
      bandHeights[curBand]++;
    }
  }

  WHILE_MODE(30) {

    // Sample the sounds coming in and do the transform.
    for (i=0; i < 128; i++) {
      val = analogRead(soundPin);
      data[i] = val / 4 -128;
      im[i] = 0;
    };
    fix_fft(data,im,7,0);

    /*for (i = 0; i < 30; i++) {
      Serial.print((int)sqrt(data[i] * data[i] + im[i] * im[i]));
      Serial.print(",");
    }
    Serial.println("");*/
    /*int max = 0, maxband = 0;
    for (i = 0; i < 60; i++) {
      int hi = sqrt(data[i] * data[i] + im[i] * im[i]);
      if (hi > max) {
        max = hi;
        maxband = i;
      }
    }
    Serial.println(maxband);
    continue;*/

    // Separate the result into bands.
    for (curBand = 0; curBand < BANDS; curBand++) {
      bands[curBand] = 0;
    }
    double bandMax = 0;
    /*Serial.print("d0 ");
    Serial.print(sqrt(data[0] * data[0] + im[0] * im[0]));
    Serial.print(" d1 ");
    Serial.print(sqrt(data[1] * data[1] + im[1] * im[1]));
    Serial.print(" d2 ");
    Serial.print(sqrt(data[2] * data[2] + im[2] * im[2]));*/

    /*for (curBand = 0; curBand < BANDS; curBand++) {
      bands[curBand] = sqrt(data[curBand + 1] * data[curBand + 1] + im[curBand + 1] * im[curBand + 1]);
      if (bands[curBand] > bandMax) {
        bandMax = bands[curBand];
      }
    }*/

    // Start at 1; the first data point is crap.
    for (curBand = 0, i = 1;
         curBand < BANDS && i < 128;
         curBand++) {
      for (int target = i + bandSizes[curBand]; i < target && i < 128; i++) {
        bands[curBand] += sqrt(data[i] * data[i] + im[i] * im[i]);
      }
      bands[curBand] /= bandSizes[curBand];

      if (bands[curBand] > bandMax) {
        bandMax = bands[curBand];
      }
    }

/*    for (curBand = 0, i = 1;
         curBand < BANDS && i < 128;
         i++) {
      bands[curBand] += sqrt(data[i] * data[i] + im[i] * im[i]);

      // Weird math to figure out if we've found (1 << curBand) values for this band, or we're out of values.
      if (i >= (1 << (curBand + 1)) - 2 || i == 127) {
        bands[curBand] /= (double)(1 << curBand);
        //Serial.print("   band ");
        //Serial.print(curBand);
        //Serial.print(" ");
        //Serial.print(bands[curBand]);
        if (bands[curBand] > bandMax) {
          bandMax = bands[curBand];
        }
        curBand++;
      }
    }*/
    /*for (i=0; i < 42; i++) {       // In the current design, 60Hz and noise
      // XXX
      bands[i / 6] += sqrt(data[i] * data[i] + im[i] * im[i]);//in general are a problem. Future designs
      // START HERE: Lots of frequencies stuck at the bottom band. Divide that one more??
    }

    // Find the maximum band value.
    uint16_t bandMax = 0;
    for (curBand = 0; curBand < BANDS; curBand++) {
      if (bands[curBand] > bandMax) {
        bandMax = bands[curBand];
      }
    }*/

    if (!avg) {
      // If we're just starting out, assume this is the average loudness.
      avg = bandMax;
    } else {
      // Otherwise, converge the average towards this, so the loudness always appears "medium".
      avg += (bandMax - avg) / 60.0;
    }

    /*Serial.print("  max");
    Serial.print(bandMax);
    Serial.print("  avg");
    Serial.print(avg);*/

    // Render each band in LEDs, shading as necessary.
    for (curBand = 0, i = 0;
         curBand < BANDS && i < (sizeof(spectrumLayout) * sizeof(int8_t));
         // Increment i again when we move to the next band, to account for the -1 at the end.
         curBand++, i++) {
      // TODO: Make the values linger a bit to make it less stroby.
      long height = (bands[curBand] / avg) * 0.8 * ((double)bandHeights[curBand] * 255.0);//XXX? * .8;
      /*Serial.print("  band");
      Serial.print(curBand);
      Serial.print(":");
      Serial.print(bands[curBand]);
      Serial.print(":");
      Serial.print(height);*/
      for (int j = 0; j < bandHeights[curBand]; j++) {
        byte value = gamma(min(height, 0xFF));
        // Increment i for the next node the next time around.
        strip.set(spectrumLayout[i++],
          (curBand <= 1 || curBand == 5) ? value : 0,
          (curBand >= 1 && curBand <= 3) ? value : 0,
          (curBand >= 3) ? value : 0);
        height -= 0xFF;
        if (height < 0) {
          height = 0;
        }
      }
    }
//    Serial.println("");
    strip.show();
  }
}

void randDots() {

  WHILE_MODE(30) {
    strip.set(random(numNodes), random(FULL + 1), random(FULL + 1), random(FULL + 1));
    showFor(1);
  }
}

#define HUE_DIVIDER ((float)FULL / (float)(numNodes))
void wheelPlus() {
/*  byte baseHue = 100;
  WHILE_MODE(30) {
    for (byte i = 0; i < numNodes; i++) {
      strip.set(i, HSVtoRGBfull(((word)((float)i * HUE_DIVIDER) + baseHue) % 0xff, speed % 2 ? FULL / 2 : FULL, speed % 2 ? FULL / 2 : FULL));
    }
    baseHue += (3 - speed % 3) * 3;
    showFor(60 * (speed % 3));
  }*/
  int j = 0;
  WHILE_MODE(30) {
    for (int i = 0; i < WIDTH; i++) {
      fillCol(i, Wheel( ((i * 384 / WIDTH + j) % 384)));
    }
    showFor(5);
    j++;
  }
}


void nightRide() {
  uint32_t mycolor;
  switch (speed) {
  case 0:
    mycolor = strip.Color(127, 0, 0);
    break;
  case 1:
    mycolor = strip.Color(0, 127, 0);
    break;
  case 2:
    mycolor = strip.Color(0, 0, 127);
    break;
  case 3:
    mycolor = strip.Color(127, 127, 0);
    break;
  case 4:
    mycolor = strip.Color(127, 0, 127);
    break;
  case 5:
    mycolor = strip.Color(127, 127, 127);
    break;
  }


  WHILE_MODE(5) {
    for (int i = 0; i < WIDTH; i++) {
      fillCol(i, mycolor);
      showFor(50);
      fillCol(i, strip.Color(0, 0, 0));
    }
    for (int i = WIDTH - 2; i > 0; i--) {
      fillCol(i, mycolor);
      showFor(50);
      fillCol(i, strip.Color(0, 0, 0));
    }
  }
}

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


PROGMEM prog_uchar gammaTable[]  = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
    4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  7,  7,
    7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10, 11,
   11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16,
   16, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22,
   23, 23, 24, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30,
   30, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 37, 38, 38, 39,
   40, 40, 41, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48, 49, 50,
   50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60, 61, 62,
   62, 63, 64, 65, 66, 67, 67, 68, 69, 70, 71, 72, 73, 74, 74, 75,
   76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
   92, 93, 94, 95, 96, 97, 98, 99,100,101,102,104,105,106,107,108,
  109,110,111,113,114,115,116,117,118,120,121,122,123,125,126,127
};

// This function (which actually gets 'inlined' anywhere it's called)
// exists so that ammaTable can reside out of the way down here in the
// utility code...didn't want that huge table distracting or intimidating
// folks before even getting into the real substance of the program, and
// the compiler permits forward references to functions but not data.
inline byte gamma(byte x) {
  return pgm_read_byte(&gammaTable[x]);
}


// Fixed-point colorspace conversion: HSV (hue-saturation-value) to RGB.
// This is a bit like the 'Wheel' function from the original strandtest
// code on steroids.  The angular units for the hue parameter may seem a
// bit odd: there are 1536 increments around the full color wheel here --
// not degrees, radians, gradians or any other conventional unit I'm
// aware of.  These units make the conversion code simpler/faster, because
// the wheel can be divided into six sections of 256 values each, very
// easy to handle on an 8-bit microcontroller.  Math is math, and the
// rendering code elsehwere in this file was written to be aware of these
// units.  Saturation and value (brightness) range from 0 to 255.
long hsv2rgb(long h, byte s, byte v) {
  byte r, g, b, lo;
  int  s1;
  long v1;

  // Hue
  h %= 1536;           // -1535 to +1535
  if(h < 0) h += 1536; //     0 to +1535
  lo = h & 255;        // Low byte  = primary/secondary color mix
  switch(h >> 8) {     // High byte = sextant of colorwheel
    case 0 : r = 255     ; g =  lo     ; b =   0     ; break; // R to Y
    case 1 : r = 255 - lo; g = 255     ; b =   0     ; break; // Y to G
    case 2 : r =   0     ; g = 255     ; b =  lo     ; break; // G to C
    case 3 : r =   0     ; g = 255 - lo; b = 255     ; break; // C to B
    case 4 : r =  lo     ; g =   0     ; b = 255     ; break; // B to M
    default: r = 255     ; g =   0     ; b = 255 - lo; break; // M to R
  }

  // Saturation: add 1 so range is 1 to 256, allowig a quick shift operation
  // on the result rather than a costly divide, while the type upgrade to int
  // avoids repeated type conversions in both directions.
  s1 = s + 1;
  r = 255 - (((255 - r) * s1) >> 8);
  g = 255 - (((255 - g) * s1) >> 8);
  b = 255 - (((255 - b) * s1) >> 8);

  // Value (brightness) and 24-bit color concat merged: similar to above, add
  // 1 to allow shifts, and upgrade to long makes other conversions implicit.
  v1 = v + 1;
  return (((r * v1) & 0xff00) << 8) |
          ((g * v1) & 0xff00)       |
         ( (b * v1)           >> 8);
}


/*
long HSVtoRGBfull(byte h, byte s, byte v) {
  return HSVtoRGB( (float)h / (float)FULL * 6.0, (float)s / (float)FULL, (float)v / (float)FULL);
}

long HSVtoRGB( float h, float s, float v ) {

  //   modified from Alvy Ray Smith's site:
//http://www.alvyray.com/Papers/hsv2rgb.htm
//H is given on [0, 6]. S and V are given on [0, 1].
//RGB is returned as a 24-bit long #rrggbb

  int i;
  float m, n, f;

  // not very elegant way of dealing with out of range: return black
  if ((s<0.0) || (s>1.0) || (v<0.0) || (v>1.0)) {
    return 0L;
  }

  if ((h < 0.0) || (h > 6.0)) {
    return long( v * 255 ) + long( v * 255 ) * 256 + long( v * 255 ) * 65536;
  }
  i = floor(h);
  f = h - i;
  if ( !(i&1) ) {
    f = 1 - f; // if i is even
  }
  m = v * (1 - s);
  n = v * (1 - s * f);
  switch (i) {
    case 6:
    case 0: // RETURN_RGB(v, n, m)
      return long(v * 255 ) * 65536 + long( n * 255 ) * 256 + long( m * 255);
    case 1: // RETURN_RGB(n, v, m)
      return long(n * 255 ) * 65536 + long( v * 255 ) * 256 + long( m * 255);
    case 2:  // RETURN_RGB(m, v, n)
      return long(m * 255 ) * 65536 + long( v * 255 ) * 256 + long( n * 255);
    case 3:  // RETURN_RGB(m, n, v)
      return long(m * 255 ) * 65536 + long( n * 255 ) * 256 + long( v * 255);
    case 4:  // RETURN_RGB(n, m, v)
      return long(n * 255 ) * 65536 + long( m * 255 ) * 256 + long( v * 255);
    case 5:  // RETURN_RGB(v, m, n)
      return long(v * 255 ) * 65536 + long( m * 255 ) * 256 + long( n * 255);
  }
}

*/
