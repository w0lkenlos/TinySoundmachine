#ifndef I2C_h
#define I2C_h

namespace I2C{
	// Call this in setup
	void init ();
	// Reads multiple consecutive bytes starting from regAddr
	void readFrom(char deviceAddr, char regAddr, int numBytes, unsigned char outVals[]);
	// Write one value to the specified register
	void writeTo(char deviceAddr, char regAddr, unsigned char val);
	// Write num consecutive values beginning at the specified address 
	void writeToMulti(char deviceAddr, char regAddr, int numBytes, unsigned char values []);
}
#endif
