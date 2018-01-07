#include "I2C.h"
#include "Arduino.h"
#include <Wire.h>

namespace I2C {
  // Call this in setup
  void init () {
    Wire.begin();
  }
  
  // Reads num bytes starting from address register on device in to buff array
  void readFrom(char deviceAddr, char regAddr, int numBytes, unsigned char outVals[]) {
  
    // send start
    Wire.beginTransmission(deviceAddr);
    // send register address
    Wire.write(regAddr);
    // send end
    Wire.endTransmission();
  
    delayMicroseconds (100); // might not be necessary in all cases, but seemed to work for me
  
    Wire.requestFrom(deviceAddr, numBytes);
  
    int i = 0;
    while (Wire.available())      // device may send less than requested
    {
      // read a byte
      outVals[i] = Wire.read();
      i++;
    }
  }
  
  void writeTo(char deviceAddr, char regAddr, unsigned char val) {
    // send start
    Wire.beginTransmission(deviceAddr);
    // send register address
    Wire.write(regAddr);
    // write value
    Wire.write(val);
    // send end
    Wire.endTransmission();
  }
  
  // Write multiple consecutive values beginning at the specified address
  void writeToMulti(char deviceAddr, char regAddr, int numBytes, unsigned char values []) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(regAddr);
  
    for (int i = 0; i < numBytes; ++i) {
      delayMicroseconds (100);
      Wire.write(values[i]);
    }
    Wire.endTransmission();
  
  }
}
