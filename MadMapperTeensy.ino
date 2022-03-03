// MadMapperTeensy v 0.1
// Made for Teensy 4.0
// Takes signal from the software "MadMapper" and writes this data to WS2812 5v LED Strips
// Made with help from the MadMapper team & Aidan Lincoln
// https://www.aidanlincoln.com/led-mapping


////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <SPI.h>
#include <FastLED.h>

#define MAD_LED_PACKET_HEADER 0xFF
#define MAD_LED_DATA 0xBE
#define MAD_LED_DETECTION 0xAE
#define MAD_LED_DETECTION_REPLY 0xEA
#define MAD_LED_PROTOCOL_VERSION 0x01

// Uncomment this line to see verbose debug prints in the Seril Monitor
// #define DEBUG_MODE

// Uncomment the line below test LEDs without the need of MadMapper or MadRouter sending data on serial port
// #define JUST_TEST_LEDS

// How many LEDs are in the strip?
#define NUM_LEDS 3

// What data pin to send this out of?
// Put a 330 Ohm resistor between pin and Data In for WS2812
#define DATA_PIN 2

// Fast LED Buffers
CRGB leds[NUM_LEDS];

// MadLED protocol buffer
char dataFrame[NUM_LEDS * 3];
int readingFrameOnLine = -1;
bool gotNewDataFrame = false;

enum State {
  State_WaitingNextPacket,
  State_GotPacketHeader,
  State_WaitingLineNumber,
  State_WaitingChannelCountByte1,
  State_WaitingChannelCountByte2,
  State_ReadingDmxFrame
};

State inputState = State_WaitingNextPacket;
unsigned int channelsLeftToRead = 0;
char* frameWritePtr = dataFrame;

////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(921600);

  for (unsigned int i = 0; i < sizeof(dataFrame); i++) dataFrame[i] = 0;

  // Create LED - type, what pin, color mode, object, number of LEDs
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  Serial.print("Setup done");

} // End Setup

////////////////////////////////////////////////////////////////////////////////////////////////////////

void processByte(unsigned char currentByte) {

#ifdef DEBUG_MODE
  Serial.print("GOT BYTE: "); Serial.print(currentByte, HEX);
#endif

  if (currentByte == MAD_LED_PACKET_HEADER) {
    inputState = State_GotPacketHeader;
#ifdef DEBUG_MODE
    Serial.print("GOT PH ");
#endif
  } else if (inputState == State_WaitingNextPacket) {
    // Just ignore this byte, we're not processing a packet at the moment
    // Wait for next packet start (xFF)
  } else if (inputState == State_GotPacketHeader) {
    if (currentByte == MAD_LED_DETECTION) {
      // Send back detection reply
      Serial.write(MAD_LED_DETECTION_REPLY);
      Serial.write(MAD_LED_PROTOCOL_VERSION);
      inputState = State_WaitingNextPacket;
    } else if (currentByte == MAD_LED_DATA) {
      inputState = State_WaitingLineNumber;
#ifdef DEBUG_MODE
      Serial.print("GOT LD ");
#endif
    } else {
      // Unknown packet start, reset
      inputState = State_WaitingNextPacket;
    }
  } else if (inputState == State_WaitingLineNumber) {
    if (currentByte > 0x7F) {
      // Error, reset
      inputState = State_WaitingNextPacket;
#ifdef DEBUG_MODE
      Serial.print("ErrLineNum: "); Serial.print(currentByte);
#endif
    } else {
      readingFrameOnLine = currentByte;
      inputState = State_WaitingChannelCountByte1;
#ifdef DEBUG_MODE
      Serial.print("GOT LN ");
#endif
    }
  } else if (inputState == State_WaitingChannelCountByte1) {
    if (currentByte > 0x7F) {
      // Error, reset
      inputState = State_WaitingNextPacket;
#ifdef DEBUG_MODE
      Serial.print("ErrChCNT1: "); Serial.print(currentByte);
#endif
    } else {
      channelsLeftToRead = currentByte;
      inputState = State_WaitingChannelCountByte2;
#ifdef DEBUG_MODE
      Serial.print("GOT CHC1 ");
#endif
    }
  } else if (inputState == State_WaitingChannelCountByte2) {
    if (currentByte > 0x7F) {
      // Error, reset
      inputState = State_WaitingNextPacket;
#ifdef DEBUG_MODE
      Serial.print("ErrChCNT2: "); Serial.print(currentByte);
#endif
    } else {
      channelsLeftToRead += (int(currentByte) << 7);
      if (channelsLeftToRead == 0) {
        // Error, reset
        inputState = State_WaitingNextPacket;
#ifdef DEBUG_MODE
        Serial.print("ErrChCNT=0");
#endif
      } else {
        frameWritePtr = dataFrame;
        inputState = State_ReadingDmxFrame;
#ifdef DEBUG_MODE
        Serial.print("GOT CHC2 ");
#endif
      }
    }
  } else if (inputState == State_ReadingDmxFrame) {
    *frameWritePtr++ = currentByte;
    channelsLeftToRead--;
    if (channelsLeftToRead == 0) {
      // Finished reading DMX Frame
      inputState = State_WaitingNextPacket;
      gotNewDataFrame = true;
#ifdef DEBUG_MODE
      Serial.print("GOT DATA ");
#endif
    }
  }
}  // End processByte()

////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  // If in this mode the entire strip will fade black to white
#ifdef JUST_TEST_LEDS
  Serial.println("Currently testing...");
  static int value = 254;
  value = (value + 1) % 254;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(value, value, value);
  }
#else

  // We read a maximum of 30000 bytes before we should call FastLED.show again
  // This is a good setting for the teensy, it depends on CPU speed, so it should be set lower on a slower CPU (ie arduino)
  // This limit (bytesRead<30000) is useless for protocols with a clock
  // But necessary when controlling more than 600 hundred RGB leds with WS2811 / WS2812
  int bytesRead = 0;
  while (Serial.available() > 0 && bytesRead < 30000) {
    processByte(Serial.read());
    bytesRead++;
  }

  if (gotNewDataFrame) {
    gotNewDataFrame = false;
    char* dataPtr = dataFrame;

    // Copy the data frame we received in the correct FastLED buffer
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB(dataPtr[0], dataPtr[1], dataPtr[2]);
      dataPtr += 3;
    }

  }
#endif

  // Actually show the LEDs
  FastLED.show();

} // End Loop

////////////////////////////////////////////////////////////////////////////////////////////////////////
