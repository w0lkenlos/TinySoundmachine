
/**
 * author: Sandra Kemmerling
 * created: 02.01.2018
 * brief: Software for a small hardware device to play music. Compatible with Arduino Uno and Teensy 3.5. Uses APA102 LEDs and custom made sliders
**/

#include <SPI.h>
#include <Wire.h>

#if DAC
#include <TimerOne.h>
#endif

#include "pitches.h"
#include "I2C.h"

//-------------------------------------
// globals & defines
//-------------------------------------
#define LED_FRAME_START_BITS (7<<5) // every data frame starts with 111
#define SLIDER1_ADDR (0x0D) // 7 bit address slider 1
#define SLIDER2_ADDR (0x6B) // 7 bit address slider 2 
#define SPEAKER_OUT 10

#define MATH_PI 3.141592653f
#define MIN_FREQ 30.0f // min possible frequency 30Hz
#define SAMPLING_RATE 50 //microseconds
#define NUM_SAMPLES 666 // num of samples needed for one sine period
#define PERIOD 3.333f // time a sine period needs to complete in ms
#define STEP_SIZE 0.01

#define DAC 0 //DAC can only be used with Teensy (3.5)

#define MAX_OCTAVE 6
#define NUM_NOTES 8
#define NUM_MELODIES 4
#define MAX_NOTE_COUNT 25 // 24 notes + stop

#define MASK_BTN_C                ((unsigned char)(1 << 0))
#define MASK_BTN_D                ((unsigned char)(1 << 1))
#define MASK_BTN_E                ((unsigned char)(1 << 2))
#define MASK_BTN_F                ((unsigned char)(1 << 3))
#define MASK_BTN_G                ((unsigned char)(1 << 4))
#define MASK_BTN_H                ((unsigned char)(1 << 5))
#define MASK_BTN_A                ((unsigned char)(1 << 6))
#define MASK_BTN_C_UPPER          ((unsigned char)(1 << 7))

#define MASK_BTN_OCTAVE_DOWN      ((unsigned char)(1 << 0))
#define MASK_BTN_OCTAVE_UP        ((unsigned char)(1 << 1))
#define MASK_BTN_BEAT             ((unsigned char)(1 << 2))
#define MASK_BTN_MELODY           ((unsigned char)(1 << 3))

int _numLEDs = 2;

//+++ slider & buttons +++//
unsigned char _keyStatus [5];
unsigned char _changePin = 2;
volatile boolean _sliderValueChanged = false;

//+++ DAC vars +++//
#if DAC
int _currentNote = 0;
int _currentNoteDuration = 0;
volatile float _currentMultiplier;
volatile double _dacOut; // value for DAC output
volatile int _currentPeriodCount = 0;
volatile bool _isNotePlaying = false;
volatile bool _test = false;
#endif


//+++ melody and timing +++//
int melody[NUM_MELODIES][MAX_NOTE_COUNT] = {
  // Mr. Wobble
  {
    NOTE_E5,
    NOTE_E3,
    NOTE_E5,
    NOTE_E3,
    NOTE_E5,
    NOTE_E3,
    NOTE_D5,
    NOTE_D3,
    NOTE_G5,
    NOTE_D3,
    NOTE_G5,

    NOTE_G5,
    NOTE_C3,
    NOTE_G5,
    NOTE_C3,
    NOTE_G5,
    NOTE_C3,
    NOTE_D5,
    NOTE_D3,
    NOTE_E5,
    NOTE_D3,
    NOTE_G5,
    NOTE_D3,

    NOTE_STOP
  },
  // Eponas Song
  {
    NOTE_D5, NOTE_B4, NOTE_A4, NOTE_D5, NOTE_B4, NOTE_A4, NOTE_D5, NOTE_B4, NOTE_A4, NOTE_B4, NOTE_A4,
    NOTE_STOP
  },
  //Song of Time
  {
    NOTE_A4, NOTE_D4, NOTE_F4, NOTE_A4, NOTE_D4, NOTE_F4,
    NOTE_A4, NOTE_C5, NOTE_B4, NOTE_G4, NOTE_F4, NOTE_G4,
    NOTE_A4, NOTE_D4, NOTE_C4, NOTE_E4, NOTE_D4,

    NOTE_STOP
  },
  //Song of Storms
  {
    NOTE_D4, NOTE_F4, NOTE_D5, NOTE_D4, NOTE_F4, NOTE_D5,
    NOTE_E5, NOTE_F5, NOTE_E5, NOTE_F5, NOTE_E5, NOTE_C5, NOTE_A4,

    NOTE_STOP
  }
};

int beats[NUM_MELODIES][MAX_NOTE_COUNT]  = {
  { 8, 8, 16,  16, 16, 16, 16, 16, 8, 8, 8,
    8, 8, 16, 16,  16, 16, 16, 16, 16, 16, 8, 8
  },
  {
    8, 8, 2, 8, 8, 2, 8, 8, 4, 4, 2
  },
  {
    4, 2, 4, 4, 2, 4,
    8, 8, 4, 4, 8, 8,
    4, 4, 8, 8, 4
  },
  {
    8, 8, 4, 8, 8, 4,
    8, 8, 8, 8, 8 , 8, 4
  }
};

// Set overall tempo (for 100 bpm)
long _tempo = 2400;

// define notes matrix for keyboard
// 7 octaves with 8 notes each
int _notes [MAX_OCTAVE][NUM_NOTES] = {
  {NOTE_C1, NOTE_D1, NOTE_E1, NOTE_F1, NOTE_G1, NOTE_A1, NOTE_B1, NOTE_C2},
  {NOTE_C2, NOTE_D2, NOTE_E2, NOTE_F2, NOTE_G2, NOTE_A2, NOTE_B2, NOTE_C3},
  {NOTE_C3, NOTE_D3, NOTE_E3, NOTE_F3, NOTE_G3, NOTE_A3, NOTE_B3, NOTE_C4},
  {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5},
  {NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5, NOTE_A5, NOTE_B5, NOTE_C6},
  {NOTE_C6, NOTE_D6, NOTE_E6, NOTE_F6, NOTE_G6, NOTE_A6, NOTE_B6, NOTE_C7}
  //{NOTE_C7, NOTE_D7, NOTE_E7, NOTE_F7, NOTE_G7, NOTE_A7, NOTE_B7, NOTE_C8}
};

unsigned char _octave = 3;
char _keyboardBeat = 4;
char _melodyIndex = 0;

// one color per note, brightness, r, g, b
unsigned char _colors [NUM_NOTES][4] = {
  {15, 125, 0, 0},      // red
  {15, 125, 70, 0},     // orange
  {15, 125, 125, 0},    // yellow
  {15, 0, 125, 0},      // green
  {15, 0, 0, 125},      // blue
  {15, 125, 0, 125},    // magenta
  {15, 125, 0, 70},     // pink
  {15, 125, 125, 125}   // white
};

//-------------------------------------
// APA 102 via SPI
//-------------------------------------
// sends a color value over SPI (only data frame no start or end)
void sendColorSPI (int br, int r, int g, int b) {

  //send data frame:: 111 | 5 bit brightness | 8 bit blue | 8 bit green | 8 bit red
  SPI.transfer(LED_FRAME_START_BITS | br); // MAX brightness: 31
  SPI.transfer(b);
  SPI.transfer(g);
  SPI.transfer(r);
}

//-------------------------------------

// sends a color value with brightness to all connected LEDs (same for all)
void sendColorToAll(int br, int r, int g, int b) {
  // send start frame 0000 0000 0000 0000
  for (int i = 0; i < 4; ++i) {
    SPI.transfer(0);
  }
  for (int i = 0; i < _numLEDs; ++i) {

    sendColorSPI (br, r, g, b);
  }
  // send end frame 1111 1111 1111 1111
  for (int i = 0; i < 4; ++i) {
    SPI.transfer(0xff);
  }
}

//-------------------------------------

// sends a color value to the LED at the provided index, turns of all other LEDs
void sendColorToLEDAtIndex (int ledIndex, int br, int r, int g, int b) {
  // send start frame 0000 0000 0000 0000
  for (int i = 0; i < 4; ++i) {
    SPI.transfer(0);
  }
  for (int i = 0; i < _numLEDs; ++i) {

    if (i == ledIndex) {
      sendColorSPI (br, r, g, b);
    }
    else {
      sendColorSPI (0, 0, 0, 0);
    }
  }
  // send end frame 1111 1111 1111 1111
  for (int i = 0; i < 4; ++i) {
    SPI.transfer(0xff);
  }
}

//-------------------------------------

void sendColorForFrequency (int freq) {
  unsigned long temp = freq * 3757; // map range to 24 bit value
  char r =  (temp >> 16);
  char g = (temp >> 8) & 0xff;
  char b = (temp & 0xff);

  sendColorToAll (15, r, g, b);
}

//-------------------------------------
// Sound
//-------------------------------------

void playTone(int note, int beat, bool pause) {
  int duration = 0;
  if (beat < 0) {
    tone (SPEAKER_OUT, note);
  }
  else {
    duration = _tempo / beat;
    tone (SPEAKER_OUT, note, duration / 1.3);
  }

#if DAC
  _dacOut = 0;
  _currentNote = note;
  _currentNoteDuration = duration;
  _currentMultiplier = _currentNote / MIN_FREQ;

  _isNotePlaying = true;
  Timer1.start();
#else

  tone (SPEAKER_OUT, note, duration / 1.3);
  // to distinguish the notes, set a minimum time between them.
  // the note's duration + 30% seems to work well:
  if (pause) {
    int pauseBetweenNotes = duration;
    delay(pauseBetweenNotes);
    sendColorToAll(0, 0, 0, 0);
    noTone(SPEAKER_OUT);
  }

#endif

}

//-------------------------------------

void playToneAtIndex(int noteIndex, int beat) {
  int note = _notes[_octave][noteIndex];
  //sendColorToAll(_colors[noteIndex][0], _colors[noteIndex][1], _colors[noteIndex][2], _colors[noteIndex][3]);
  sendColorForFrequency (note);
  playTone (note, beat, true);
}

//-------------------------------------

void playMelody (int index) {

  uint8_t noteCount = sizeof(melody[index]) / sizeof(int);
  for (int i = 0; i < noteCount; i++) {

    int note = melody[index][i];
    if (note == NOTE_STOP) {
      Serial.print("stop");
      break;
    }

    int beat = beats[index][i];
    sendColorForFrequency (note);
    playTone (note, beat, true);
  }

  sendColorToAll(0, 0, 0, 0);
}

//-------------------------------------

void holdToneAtIndex(int noteIndex) {
  int note = _notes[_octave][noteIndex];
  //sendColorToAll(_colors[noteIndex][0], _colors[noteIndex][1], _colors[noteIndex][2], _colors[noteIndex][3]);
  sendColorForFrequency (note);
  playTone (note, -1, false);
}

//-------------------------------------

#if DAC
void checkTone () {
  noInterrupts();
  float elapsedTime = _currentPeriodCount * PERIOD;
  interrupts();

  Serial.println (elapsedTime);
  if (elapsedTime >= _currentNoteDuration) {
    Timer1.stop();

    analogWrite(A21, 0);
    sendColorToAll (15, 0, 0, 0);
    _isNotePlaying = false;

    // to distinguish the notes, set a minimum time between them.
    int pauseBetweenNotes = _currentNoteDuration;
    delay(pauseBetweenNotes);

    //reset all values
    _currentPeriodCount = 0;
    _currentNoteDuration = 0;
    _currentNote = 0;
    _dacOut = 0;
  }
}

void initTimer () {
  Timer1.initialize(SAMPLING_RATE);
  Timer1.attachInterrupt(onToneTimer);
  Timer1.stop();
}

//-------------------------------------

void onToneTimer() {
  volatile double temp = sin (_dacOut * MATH_PI * _currentMultiplier) + 1;
  temp = (temp * 2048);
  volatile uint16_t outVal = temp;
  analogWrite(A21, outVal);
  _dacOut += STEP_SIZE;
  if (_dacOut > 2) {
    _dacOut = 0;
    ++_currentPeriodCount;
  }

  _test = !_test;
  if (_test) {
    digitalWrite(SPEAKER_OUT, 255);
  }
  else {
    digitalWrite(SPEAKER_OUT, 0);
  }
}
#endif

//-------------------------------------
// slider
//-------------------------------------
void configureSlider () {
  // 0x14 slider control (hyst + num buttons)
  // 0x15 slider options (resolution)
  // 0x16 0 - 11 key control (AKS groups)

  //+++ slider 1+++//
  unsigned char configData [14] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
  I2C::writeToMulti(SLIDER1_ADDR, 0x14, 14, configData);

  unsigned char burstLength [12];
  for (int i = 0; i < 12; ++i) {
    burstLength [i] = 8;
  }
  I2C::writeToMulti(SLIDER1_ADDR, 0x36, 12, burstLength);

  //+++ slider 2+++//
  /*unsigned char configData2 [14]  = {((2 << 4) | 8), 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
    I2C::writeToMulti(SLIDER2_ADDR, 0x14, 14, configData2);*/
}

//-------------------------------------

void pollSliderInput () {

  // 0x02 general status, last bit: is slider touched
  // 0x03 key Status k0 - k7
  // 0x04 key status k8 - k15
  // 0x05 last slider value
  delayMicroseconds(5);
  I2C::readFrom(SLIDER1_ADDR, 0x02, 5, _keyStatus);

  /*Serial.print (_keyStatus[1] & (1 << 7) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 6) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 5) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 4) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 3) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 2) ? "1" : "0");
    Serial.print (_keyStatus[1] & (1 << 1) ? "1" : "0");
    Serial.println (_keyStatus[1] & (1 << 0) ? "1" : "0");

    Serial.print (_keyStatus[2] & (1 << 7) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 6) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 5) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 4) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 3) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 2) ? "1" : "0");
    Serial.print (_keyStatus[2] & (1 << 1) ? "1" : "0");
    Serial.println (_keyStatus[2] & (1 << 0) ? "1" : "0");*/

  /*Serial.println ("-");
    Serial.println (_keyStatus [0] & 1);
    Serial.println (_keyStatus[3]);
    Serial.println ("---");*/

  int currentNote = 0;

  if (_keyStatus[1] & MASK_BTN_C) {
    //playToneAtIndex(0, _keyboardBeat);
    holdToneAtIndex(0);
  }
  else if (_keyStatus[1] & MASK_BTN_D) {
    //playToneAtIndex(1, _keyboardBeat);
    holdToneAtIndex(1);

  }
  else if (_keyStatus[1] & MASK_BTN_E) {
    //playToneAtIndex(2, _keyboardBeat);
    holdToneAtIndex(2);
  }
  else if (_keyStatus[1] & MASK_BTN_F) {
    //playToneAtIndex(3, _keyboardBeat);
    holdToneAtIndex(3);
  }
  else if (_keyStatus[1] & MASK_BTN_G) {
    //playToneAtIndex(4, _keyboardBeat);
    holdToneAtIndex(4);
  }
  else if (_keyStatus[1] & MASK_BTN_H) {
    // playToneAtIndex(5, _keyboardBeat);
    holdToneAtIndex(5);

  }
  else if (_keyStatus[1] & MASK_BTN_A) {
    // playToneAtIndex(6, _keyboardBeat);
    holdToneAtIndex(6);

  }
  else if (_keyStatus[1] & MASK_BTN_C_UPPER) {
    // playToneAtIndex(7, _keyboardBeat);
    holdToneAtIndex(7);

  }
  else {
    noTone(SPEAKER_OUT);
    sendColorToAll(0, 0, 0, 0);
  }

  if (_keyStatus[2] & MASK_BTN_OCTAVE_UP) {
    _octave = _octave < (MAX_OCTAVE - 1) ? _octave + 1 : (MAX_OCTAVE - 1);
    currentNote = _notes[_octave][0];
    playTone(currentNote, _keyboardBeat, false);
    Serial.print ("octave: ");
    Serial.println (_octave);
  }
  else if (_keyStatus[2] & MASK_BTN_OCTAVE_DOWN) {
    _octave = _octave > 0 ? _octave - 1 : 0;
    currentNote = _notes[_octave][0];
    playTone(currentNote, _keyboardBeat, false);
    Serial.print ("octave: ");
    Serial.println (_octave);
  }
  else if (_keyStatus[2] & MASK_BTN_BEAT) {
    _melodyIndex = (_melodyIndex + 1) % NUM_MELODIES; 
    playTone(NOTE_C4, _keyboardBeat, false);
  }
  else if (_keyStatus[2] & MASK_BTN_MELODY) {
    playMelody(_melodyIndex);
  }
  I2C::readFrom(SLIDER1_ADDR, 0x02, 5, _keyStatus);
}

//-------------------------------------

void onSliderChange () {
  Serial.println ("change");
  _sliderValueChanged = true;
}

//-------------------------------------
// setup and loop
//-------------------------------------

void setup() {
  // put your setup code here, to run once:
  SPI.begin();
  sendColorToAll (15, 0, 0, 0);
  //sendColorToAll(15, 125, 125, 125);

  //I2C::init();
  Wire.begin();
  Serial.begin(9600);

  //+++ speaker +++//
  pinMode(SPEAKER_OUT, OUTPUT);

  //+++ slider +++//
  configureSlider();
  delayMicroseconds(5);
  pinMode(_changePin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(_changePin), onSliderChange, FALLING);
  
  delayMicroseconds(5);
  I2C::readFrom(SLIDER1_ADDR, 0x02, 5, _keyStatus);

#if DAC
  initTimer();
#endif
}

//-------------------------------------

void loop() {
  
  delayMicroseconds (5);
#if DAC
  if (_isNotePlaying) {
    checkTone();
  }
#endif

  if (_sliderValueChanged) {
    pollSliderInput();
    _sliderValueChanged = false;
  }
}
