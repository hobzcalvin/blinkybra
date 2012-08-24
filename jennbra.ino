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
#define nodeAt(x, y) (nodes[(x) * HEIGHT + (y)])
#define imaginaryNode(x, y) (nodeLayout[(x) * HEIGHT + (y)] == -1)

uint32_t nodes[numNodes] = { 0 };

typedef void (*mode_func)(void);

volatile bool mode_running = true;
volatile int8_t curMode;
volatile byte speed = 0;
volatile byte timerDoubler;

#define FULL 255
#define MAXHUE 1535

enum colors {
  red = 0,
  yellow,
  green,
  cyan,
  blue,
  magenta,
  white,
  purple,
  numColors,
};
uint32_t COLORS[] = {
  0xFF0000,
  0xFFFF00,
  0x00FF00,
  0x00FFFF,
  0x0000FF,
  0xFF00FF,
  0xFFFFFF,
  0xC000FF,
};

LPD8806 strip = NULL;

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
  for (int i = 0; i < numNodes; i++) {
    nodes[i] = color;
  }
}

void clear() {
  fill(0);
}

void showFor(uint32_t ms, byte doGamma = true) {
  if (!mode_running) return;
  for (int i = 0; i < numNodes && mode_running; i++) {
    if (nodeLayout[i] >= 0) {
      if (doGamma) {
        strip.setPixelColor(nodeLayout[i], gamma(nodes[i] >> 16), gamma(nodes[i] >> 8), gamma(nodes[i]));
      } else {
        strip.setPixelColor(nodeLayout[i], nodes[i] >> 17, nodes[i] >> 9, nodes[i] >> 1);
      }
    }
  }
  strip.show();
  uint32_t target = millis() + ms;
  while (mode_running && millis() < target) { }
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

  byte realNumNodes = 0;
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

//  attachInterrupt(0, b0interrupt, FALLING);
//  attachInterrupt(1, b1interrupt, FALLING);
  PCintPort::attachInterrupt(b0pin, b0interrupt, FALLING);
  PCintPort::attachInterrupt(b1pin, b1interrupt, FALLING);
  PCintPort::attachInterrupt(b2pin, b2interrupt, FALLING);
  PCintPort::attachInterrupt(b3pin, b3interrupt, FALLING);
  PCintPort::attachInterrupt(b4pin, b4interrupt, FALLING);


  Timer1.initialize();
  // XXX For now, start dimmer for powersave.
  strip.downBrightness();
  strip.downBrightness();
  curMode = 3;
}

void checkPattern() {
  fill(0xFF0000);
  digitalWrite(ledPin, HIGH);
  showFor(200);
  fill(0x00FF00);
  digitalWrite(ledPin, LOW);
  showFor(200);
  fill(0x0000FF);
  digitalWrite(ledPin, HIGH);
  showFor(200);
  fill(0);
  digitalWrite(ledPin, LOW);
  showFor(200);
  digitalWrite(ledPin, HIGH);
}

void shiftLeft() {
  memmove(nodes, nodes + HEIGHT, HEIGHT * (WIDTH - 1) * sizeof(uint32_t));
  fillCol(WIDTH - 1, 0);
}

void fillCol(uint8_t col, uint32_t color) {
  for (int j = 0; j < HEIGHT; j++) {
    nodeAt(col, j) = color;
  }
}

mode_func modes[] = {
  &colorMix,
  &wheelPlus,
  &blueSound,
  &spectrum,
  &nightRide,
  &randDots,
};
#define num_modes (sizeof(modes) / sizeof(mode_func))
mode_func specialModes[] = {
  &headlights,
  &plainColors,
};
#define num_special_modes (sizeof(specialModes) / sizeof(mode_func))

void loop() {
  // Do whatever curMode defines, over and over.
  if (curMode == 0) {
    // Random mode. Go through all normal modes.
    // Need to set mode_running now so it doesn't think it's still time to change modes.
    mode_running = true;
    checkPattern();
    // handleRandomTimer only acts every 2 times it's called, so this time value is doubled.
    Timer1.attachInterrupt(handleRandomTimer, 6000000);
    while (curMode == 0) {
      mode_running = true;
      clear();
      speed = random(256);
      timerDoubler = 0;
      Timer1.restart();
      (*modes[random(num_modes)])();
    }
    Timer1.detachInterrupt();
  } else {
    mode_running = true;
    clear();
    if (curMode > 0) {
      // Remove 1-indexing.
      (*modes[curMode - 1])();
    } else {
      // Remove 1-indexing.
      (*specialModes[-1 * curMode - 1])();
    }
  }
}

// Positions should really be bytes, but we're about to do arithmetic with them that wants negative results.
float distance(float pos1, float pos2, float dimensionSize) {
  float distance = abs(pos1 - pos2);
  while (distance > dimensionSize) distance -= dimensionSize;
  if (distance > dimensionSize / 2) {
    distance = dimensionSize - distance;
  }
  return distance;
}

#define RED 0
#define GREEN 1
#define BLUE 2
void colorMix() {
  // Cut speed to a range we care about.
  speed %= 12;

  float xPos[] = { WIDTH / 2.0, WIDTH / 2.0, WIDTH / 2.0 };
  float yPos[] = { HEIGHT / 2.0, HEIGHT / 2.0, HEIGHT / 2.0 };
  float xIncrement[3];
  float yIncrement[3];
  int steps[] = { 0, 0, 0 };

  float size = 4.0 + (speed % 2);

  byte increment_max = 4 * (speed % 3 + 1);
  /*switch (speed % 3) {
  case 0:
    increment_max = 3;
    break;
  case 1:
    increment_max = 6;
    break;
  case 2:
    increment_max = 15;
    break;
  }*/

  while (mode_running) {
    // Render the spots.
    for (byte x = 0; x < WIDTH; x++) {
      for (byte y = 0; y < HEIGHT; y++) {
        // Don't render imaginary nodes; they take time.
        if (imaginaryNode(x, y)) continue;
        uint32_t color = 0;
        for (byte c = RED; c <= BLUE; c++) {
          float xDist = distance(x, xPos[c], WIDTH);
          float yDist = distance(y, yPos[c], HEIGHT);
          float dist = sqrt(xDist * xDist + yDist * yDist);
          color |= (uint32_t)(255.0 * (size - min(size, dist)) / size) << (BLUE - c) * 8;
        }
        nodeAt(x, y) = (speed > 5) ? ~color : color;
      }
    }
    showFor(1);

    // Move the spots.
    for (byte c = RED; c <= BLUE; c++) {
      if (!steps[c]) {
        xIncrement[c] = ((float)random(0, increment_max * 2) - increment_max) / 20.0;
        yIncrement[c] = ((float)random(0, increment_max * 2) - increment_max) / 20.0;
        steps[c] = random(10, 100);
      }
      xPos[c] += xIncrement[c];
      yPos[c] += yIncrement[c];
      while (xPos[c] > WIDTH) xPos[c] -= WIDTH;
      while (yPos[c] > HEIGHT) yPos[c] -= HEIGHT;
      while (xPos[c] < 0) xPos[c] += WIDTH;
      while (yPos[c] < 0) yPos[c] += HEIGHT;
      steps[c]--;
    }
  }
}


void headlights() {
  int x, y, i, j;
  while (mode_running) {
    for (x = 2, y = 2; x < WIDTH; x += 5) {
      for (i = 0; i < 3 && x + i < WIDTH; i++) {
        for (j = 0; j < 3 && y + j < HEIGHT; j++) {
          // Start with white
          nodeAt(x + i, y + j) = COLORS[(speed + 6) % numColors];
        }
      }
    }
    showFor(1000);
  }
}

void plainColors() {
  while (mode_running) {
    // Start with white
    fill(COLORS[(speed + 6) % numColors]);
    showFor(1000);
  }
}


void blueSound() {
  uint32_t base = speed % 2 ? 0 : COLORS[blue];
  byte shift = (speed % 3) * 8;
  if (base == COLORS[blue] && shift == 0) {
    // Avoid blue on blue; change base to red.
    base = COLORS[red];
  }
  while (mode_running) {
    fillCol(WIDTH - 1, ((uint32_t)(analogRead(soundPin) >> 2) << shift) | base);
    showFor(50);
    shiftLeft();
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
byte bandHeights[BANDS] = { 0 };
void spectrum() {
  char im[128];
  char data[128];
  int i, curBand;
  int val;
  double bands[BANDS];
  long curHeights[BANDS] = { 0 };

  double avg = 0;

  // Just do this the first time this function runs.
  if (!bandHeights[0]) {
    // Count the height of each band so we know how many LEDs to divide between.
    for (int curBand = 0, i = 0;
         curBand < BANDS && i < (sizeof(spectrumLayout) * sizeof(int8_t));
         i++) {
      if (spectrumLayout[i] < 0) {
        curBand++;
      } else {
        bandHeights[curBand]++;
        // spectrumLayout is in terms of strip-numbered nodes, but we need it in terms of nodeLayout, so do the reverse translation. O(n^2); I suck.
        for (int8_t j = 0; j < numNodes; j++) {
          if (spectrumLayout[i] == nodeLayout[j]) {
            spectrumLayout[i] = j;
            break;
          }
        }
      }
    }
  }

  while (mode_running) {

    // Sample the sounds coming in and do the transform.
    for (i=0; i < 128; i++) {
      val = analogRead(soundPin);
      data[i] = val / 4 -128;
      im[i] = 0;
    };
    fix_fft(data,im,7,0);

    // Separate the result into bands.
    for (curBand = 0; curBand < BANDS; curBand++) {
      bands[curBand] = 0;
    }
    double bandMax = 0;

    // Start at 1; the first data point is crap.
    for (curBand = 0, i = 1;
         curBand < BANDS && i < 128;
         curBand++) {
      for (int target = i + bandSizes[curBand]; i < target && i < 128; i++) {
        bands[curBand] += sqrt(data[i] * data[i] + im[i] * im[i]);
      }
      bands[curBand] /= bandSizes[curBand];

      // Find the maximum band value.
      if (bands[curBand] > bandMax) {
        bandMax = bands[curBand];
      }
    }

    if (!avg) {
      // If we're just starting out, assume this is half the average loudness.
      avg = bandMax / 2;
    } else {
      // Otherwise, converge the average towards this, so the loudness always appears "medium".
      avg += (bandMax - avg) / 60.0;
    }

    // Render each band in LEDs, shading as necessary.
    for (curBand = 0, i = 0;
         curBand < BANDS && i < (sizeof(spectrumLayout) * sizeof(int8_t));
         // Increment i again when we move to the next band, to account for the -1 at the end.
         curBand++, i++) {
      // TODO: Make the values linger a bit to make it less stroby.
      long height = (bands[curBand] / avg) * 0.5 * ((double)bandHeights[curBand] * 255.0);
      if (height < curHeights[curBand]) {
        curHeights[curBand] = (height + curHeights[curBand]) / 2;
      } else {
        curHeights[curBand] = height;
      }
      height = curHeights[curBand];
      for (int j = 0; j < bandHeights[curBand]; j++) {
        byte value = min(height, FULL);
        uint32_t color = 0;
        if (curBand <= 1 || curBand == 5) color |= ((uint32_t)value) << 16;
        if (curBand >= 1 && curBand <= 3) color |= ((uint32_t)value) << 8;
        if (curBand >= 3) color |= value;
        // Increment i for the next node the next time around.
        nodes[spectrumLayout[i++]] = color;
        /*nodes[spectrumLayout[i++]] = color((curBand <= 1 || curBand == 5) ? value : 0,
          (curBand >= 1 && curBand <= 3) ? value : 0,
          (curBand >= 3) ? value : 0);*/
        height -= FULL;
        if (height < 0) {
          height = 0;
        }
      }
    }
    showFor(1);
  }
}

void randDots() {

  while (mode_running) {
//    nodes[random(numNodes)] = color(random(FULL), random(FULL), random(FULL));
    nodes[random(numNodes)] = random(FULL) << 16 | random(FULL) << 8 | random(FULL);
    showFor(1);
  }
}

void wheelPlus() {
  while (mode_running) {
    for (int i = 0; i <= MAXHUE; i+=2) {
      for (int j = 0; j < WIDTH; j++) {
        fillCol(j, hsv2rgb(i + j * (MAXHUE / WIDTH), FULL, FULL));
      }
      showFor(1);
    }
  }
}


void nightRide() {
  while (mode_running) {
    for (int i = 0; i < WIDTH; i++) {
      fillCol(i, COLORS[speed % numColors]);
      showFor(50);
      fillCol(i, 0);
    }
    for (int i = WIDTH - 2; i > 0; i--) {
      fillCol(i, COLORS[speed % numColors]);
      showFor(50);
      fillCol(i, 0);
    }
  }
}

#define BOUNCE_DURATION 50
volatile unsigned long bounceTime = 0;
#define outsideBounce() (((millis() - bounceTime) > BOUNCE_DURATION) && (bounceTime = millis()))
// Right
void b0interrupt() {
  if (outsideBounce()) {
    speed++;
    mode_running = false;
  }
}
// Up
void b1interrupt() {
  if (outsideBounce()) {
    // If in a special mode, switch to first normal mode.
    if (curMode < 0) {
      curMode = 1;
    } else if (curMode >= num_modes) {
      // Last mode; switch to random.
      curMode = 0;
    } else {
      // Otherwise, next normal mode.
      curMode++;
    }
    mode_running = false;
    speed = 0;
  }
}
// Left
void b2interrupt() {
  if (outsideBounce()) {
    // If in a normal or random mode, switch to first special mode. Also handle wrapping case.
    if (curMode >= 0 || curMode * -1 >= num_special_modes) {
      curMode = -1;
    } else {
      // Otherwise, next special mode.
      curMode--;
    }
    mode_running = false;
    speed = 0;
  }
}
// Center
void b3interrupt() {
  if (outsideBounce()) {
    strip.downBrightness();
  }
}
// Down = LESS USED.
void b4interrupt() {
  if (outsideBounce()) {
    curMode = 0;
    mode_running = false;
  }
}
void handleRandomTimer() {
  if (++timerDoubler >= 2) {
    mode_running = false;
  }
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
