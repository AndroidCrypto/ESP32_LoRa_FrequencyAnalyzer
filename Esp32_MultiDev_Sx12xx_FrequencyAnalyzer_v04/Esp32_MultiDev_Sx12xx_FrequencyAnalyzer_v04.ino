/**
*
*
**/

/*
Version Management
05.02.2025 V04 Final version for Tutorial
28.01.2025 V03 Using the Non-Blocking DIO reading for receiving, Status: working
27.02.2025 V02 added the real LoRa frequency analyzer with a toggle button
               to switch the frequencies
               Note: variant with 'ESP32 SX1262' does not work, hanging after 'receiveSXBuffer'
26.01.2025 V01 first version, just for displaying the frame on the display, 
               but without any real data
*/

/**
* Please uncomment just one hardware definition file that reflects your hardware combination
* for Heltec WiFi LoRa 32 V2 boards use HELTEC_V2
* for Heltec WiFi LoRa 32 V3 boards use HELTEC_V3
* for ESP32 Development boards with attached LoRa module SX1276 module and OLED use ESP32_SX1276_OLED
* for ESP32 Development boards with attached LoRa module SX1276 module and TFT use ESP32_SX1276_TFT
* for all other boards and hardware combination you should consider to modify an existing one to your needs
*
* Don't forget to change the Board in Arduino:
* for Heltec V2: Heltec WiFi LoRa 32(V2)
* for Heltec V3: Heltec WiFi LoRa 32(V3) / Wireless shell (V3) / ...
* or ESP32 Development Boards: ESP32-WROOM-DA Module
*
* - or in Tools menue:
* for Heltec V2: Tools - Board - esp32 - Heltec WiFi LoRa 32(V2)
* for Heltec V3: Tools - Board - esp32 - Heltec WiFi LoRa 32(V3) / Wireless shell (V3) / ...
* for ESP32 Development Boards: Tools - Board - esp32 - ESP32-WROOM-DA Module
*
*/

#define HELTEC_V2
//#define HELTEC_V3
//#define ESP32_SX1276_OLED
//#define ESP32_SX1276_TFT

// ------------------------------------------------------------------
// include the hardware definition files depending on the uncommenting
#ifdef HELTEC_V2
#include "Heltec_V2_Hardware_Settings.h"
#endif

#ifdef HELTEC_V3
#include "Heltec_V3_Hardware_Settings.h"
#endif

#ifdef ESP32_SX1276_OLED
#include "ESP32_SX1276_OLED_Hardware_Settings.h"
#endif

#ifdef ESP32_SX1276_TFT
#include "ESP32_SX1276_TFT_Hardware_Settings.h"
#endif

// ------------------------------------------------------------------

// when using the (default) OLED display SSD1306 128 * 64 px the maximum length is 26 chars
const String PROGRAM_VERSION = "LoRa Freqncy Analyzr  V04";

// ------------------------------------------------------------------
// internal or external OLED SSD1306 128 * 64 px display

#ifdef IS_OLED
#include "FONT_MONOSPACE_9.h"
// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>
#include "SSD1306.h"  // https://github.com/ThingPulse/esp8266-oled-ssd1306
SSD1306Wire display(OLED_I2C_ADDRESS, OLED_I2C_SDA_PIN, OLED_I2C_SCL_PIN);
#endif

#ifdef IS_TFT
// ------------------------------------------------------------------
// TFT display ST7735 1.8' 128 * 160 RGB
#include "FONT_MONOSPACE_9.h"
#include <SPI.h>
#include <Adafruit_GFX.h>                                        // Core graphics library, https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_ST7735.h>                                     // Hardware-specific library for ST7735, https://github.com/adafruit/Adafruit-ST7735-Library
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);  // hardware SPI
#endif

// vars for displaying line 1 to 5 to display in a loop
String display1 = "";
String display2 = "";
String display3 = "";
String display4 = "";
String display5 = "";
// for TFT only
String display6, display7, display8, display9, display10, display11, display12, display13;

bool showDisplay = false;
bool isDisplayOn = false;  // this bool is needed for switching the display off after timer exceeds

// -----------------------------------------------------------------------
// https://github.com/StuartsProjects/SX12XX-LoRa

#include <SPI.h>

#ifdef HELTEC_V2
#include <SX127XLT.h>  //include the appropriate library
SX127XLT LT;           //create a library class instance called LT
#endif

#ifdef HELTEC_V3
#include <SX126XLT.h>  //include the appropriate library
SX126XLT LT;           //create a library class instance called LT
#endif

#ifdef ESP32_SX1276_OLED
#include <SX127XLT.h>  //include the appropriate library
SX127XLT LT;           //create a library class instance called LT
#endif

#ifdef ESP32_SX1276_TFT
#include <SX127XLT.h>  //include the appropriate library
SX127XLT LT;           //create a library class instance called LT
#endif

#include "LoRa_Settings.h"  // include the setttings file, LoRa frequencies, txpower etc
#include "Node_Settings.h"  // include the node/sketch specific settings

uint8_t RXPacketL;  // stores length of packet received

// vars for counting
uint8_t channelCounter = 1;  // starting with 1 = 1st channel
uint16_t completeRounds = 0;

// the counted packets are stored here
uint16_t cnt[MAXIMUM_NUMBER_OF_CHANNELS];

// -----------------------------------------------------------------------
// change the frequency
uint32_t currentBaseFrequency;
uint32_t currentFrequency;
uint8_t currentNumberOfChannels;
const long measureIntervalMillis = MEASURE_INTERVAL_SECONDS * 1000;
long lastFrequencyChangeMillis = 0;

// -----------------------------------------------------------------------
// PRG/Boot button
// #define BOOT_BUTTON_PIN 0 // see settings or hardware settings
boolean isBootButtonPressed = false;
uint8_t modeCounter = 0;  // just a counter

void IRAM_ATTR bootButtonPressed() {
  modeCounter++;
  isBootButtonPressed = true;
  // deactivate the interrupt to avoid bouncing
  detachInterrupt(BOOT_BUTTON_PIN);
}

void loop() {

  // the BOOT button was pressed
  if (isBootButtonPressed) {
    bootButtonIsPressed();
  }

  // change the frequency to next channel
  if (millis() - lastFrequencyChangeMillis > measureIntervalMillis) {
    changeFrequency();
  }

  // note: this is a non blocking call
  // notified until the timeout is reached, here 1000 ms = 1 second
  RXPacketL = LT.receiveSXBuffer(0, 0, NO_WAIT);  // returns 0 if packet error of some sort

  //while (!digitalRead(DIO0)) {
  while (!readDioRx()) {
    // during this idle time the other elements in loop are called here
    // the BOOT button was pressed
    if (isBootButtonPressed) {
      bootButtonIsPressed();
    }

    // change the frequency to next channel
    if (millis() - lastFrequencyChangeMillis > measureIntervalMillis) {
      changeFrequency();
      // the frequency change resetted the receive request, starting new
      RXPacketL = LT.receiveSXBuffer(0, 0, NO_WAIT);  // returns 0 if packet error of some sort
    }
  }

  // we received some data
  if (RXPacketL > 0) {
    // we received something readable with given parameters
    Serial.print(F("Received a package with length "));
    Serial.println(RXPacketL);
    ledFlash(1, 125);
    // increase the counter and display the data
    cnt[channelCounter - 1]++;
    drawFrame();
  } else {
    // at this point there could be a timeout or received a packet with 'wrong' parameters
    // to distinguish between both we need to read the IRQ status of the device
    uint16_t IRQStatus;
    IRQStatus = LT.readIrqStatus();  //get the IRQ status
    if (IRQStatus & IRQ_RX_TIMEOUT) {
      // this is the regular timeout, do nothing
      Serial.print(F("Timeout, channelCounter: "));
      Serial.print(channelCounter);
      Serial.print(F(" completeRounds: "));
      Serial.println(completeRounds);
    } else {
      // we received an 'unreadable' packet but we count it as there is activity on the frequency
      // increase the counter and display the data
      Serial.println(F("Received a package with unkown length"));
      cnt[channelCounter - 1]++;
      drawFrame();
    }
  }
}

void bootButtonIsPressed() {
  isBootButtonPressed = false;
  if (modeCounter > 1) modeCounter = 0;
  // change the frequency
  if (modeCounter == 0) {
    currentBaseFrequency = FREQUENCY;
    currentNumberOfChannels = NUMBER_OF_CHANNELS_1;
    Serial.print(F("Setting new base frequency: "));
    Serial.print(currentBaseFrequency);
    Serial.println(F(" KHz"));
  } else {
    currentBaseFrequency = FREQUENCY + (NUMBER_OF_CHANNELS_1 * CHANNEL_DISTANCE);
    currentNumberOfChannels = NUMBER_OF_CHANNELS_2;
    Serial.print(F("Setting new base frequency: "));
    Serial.print(currentBaseFrequency);
    Serial.println(F(" KHz"));
  }
  resetCounter();
  drawFrame();
  // activate the interrupt again
  attachInterrupt(BOOT_BUTTON_PIN, bootButtonPressed, RISING);
  lastFrequencyChangeMillis = millis();
}

void changeFrequency() {
  channelCounter++;
  if (channelCounter > currentNumberOfChannels) {
    // start with the next round
    channelCounter = 1;
    completeRounds++;
  }
  currentFrequency = currentBaseFrequency + ((channelCounter - 1) * CHANNEL_DISTANCE);
  Serial.print(F("change frequency to "));
  Serial.println(currentFrequency);
  LT.setupLoRa(currentFrequency, OFFSET, SPREADING_FACTOR, BANDWIDTH, CODE_RATE, OPTIMISATION);
  LT.setHighSensitivity();
  Serial.println();
  LT.printModemSettings();  // reads and prints the configured LoRa settings, useful check
  Serial.println();
  drawFrame();
  lastFrequencyChangeMillis = millis();
}

/**
* If DIO0 (SX1287) or DIO1 (SX1262) are high a signal was received
**/
boolean readDioRx() {
  // this reads DIO0 (SX1276) or DIO1 (SX1262), depending on device
#ifdef HELTEC_V2
  return digitalRead(DIO0);
#endif
#ifdef HELTEC_V3
  return digitalRead(DIO1);
#endif
#ifdef ESP32_SX1276_OLED
  return digitalRead(DIO0);
#endif
#ifdef ESP32_SX1276_TFT
  return digitalRead(DIO0);
#endif
}

void ledFlash(uint16_t flashes, uint16_t delaymS) {
  // run only if a LED is connected
  if (LED_PIN >= 0) {
    uint16_t index;
    for (index = 1; index <= flashes; index++) {
      digitalWrite(LED_PIN, HIGH);
      delay(delaymS);
      digitalWrite(LED_PIN, LOW);
      delay(delaymS);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println(F("ESP32 MD LoRa Frequency Analyzer V04"));

  // if we have a power control for devices put it on
#ifdef IS_VEXT_CONTROL
  setVextControl(true);
#endif

  if (LED_PIN >= 0) {
    pinMode(LED_PIN, OUTPUT);  // setup pin as output for indicator LED
    ledFlash(1, 125);          // two quick LED flashes to indicate program start
  }

  SPI.begin();

  // setup display
#ifdef IS_OLED
  if (OLED_I2C_RST_PIN >= 0) {
    pinMode(OLED_I2C_RST_PIN, OUTPUT);
    digitalWrite(OLED_I2C_RST_PIN, LOW);  // set GPIO16 low to reset OLED
    delay(50);
    digitalWrite(OLED_I2C_RST_PIN, HIGH);
    delay(50);
  }
  clearDisplayData();
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  delay(50);
  display1 = PROGRAM_VERSION;
  //display.drawString(0, 0, PROGRAM_VERSION);
  //display.display();
  displayData();
  delay(500);
#endif

  // init TFT display
#ifdef IS_TFT
  tft.initR(INITR_BLACKTAB);     // den ST7735S Chip initialisieren, schwarz
  tft.fillScreen(ST77XX_BLACK);  // und den Schirm mit Schwarz füllen
  tft.setTextWrap(false);        // automatischen Zeilenumbruch ausschalten
  tft.setRotation(1);            // Landscape
  Serial.println(F("Display init done"));
#endif

  display1 = PROGRAM_VERSION;
  display2 = "Display init done";
  displayData();
  delay(1000);

  // setup the LoRa device
  //SPI.begin();

#ifdef HELTEC_V2
  if (LT.begin(NSS, NRESET, DIO0, LORA_DEVICE)) {
    Serial.println(F("LoRa Device Heltec V2 found"));
    display3 = "LoRa Device HV2 found";
    displayData();
    ledFlash(2, 125);
  } else {
    Serial.println(F("Device error"));
    while (1) {
      Serial.println(F("No device responding"));
      display3 = "No LoRa Device found";
      displayData();
      ledFlash(50, 50);  // long fast speed flash indicates LoRa device error
    }
  }
#endif

#ifdef HELTEC_V3
  if (LT.begin(NSS, NRESET, RFBUSY, DIO1, LORA_DEVICE)) {
    Serial.println(F("LoRa Device Heltec V3 found"));
    display3 = "LoRa Device HV3 found";
    displayData();
    ledFlash(1, 125);
  } else {
    Serial.println(F("Device error"));
    while (1) {
      Serial.println(F("No device responding"));
      display3 = "No LoRa Device found";
      displayData();
      ledFlash(50, 50);  // long fast speed flash indicates LoRa device error
    }
  }
  // The Heltec V3 board uses an unusual crystal voltage. Somme errors came up
  // when using Reliable communication so I'm setting the value here.
  LT.setDIO3AsTCXOCtrl(TCXO_CTRL_1_8V);
#endif

#ifdef ESP32_SX1276_OLED
  if (LT.begin(NSS, NRESET, DIO0, LORA_DEVICE)) {
    Serial.println(F("LoRa Device ESP32/SX1276 found"));
    display3 = "LoRa Dev.ESP32+SX1276";
    displayData();
    ledFlash(2, 125);
  } else {
    Serial.println(F("Device error"));
    while (1) {
      Serial.println(F("No device responding"));
      display3 = "No LoRa Device found";
      displayData();
      ledFlash(50, 50);  // long fast speed flash indicates LoRa device error
    }
  }
#endif

#ifdef ESP32_SX1276_TFT
  if (LT.begin(NSS, NRESET, DIO0, LORA_DEVICE)) {
    Serial.println(F("LoRa Device ESP32/SX1276 found"));
    display3 = "LoRa Dev.ESP32+SX1276";
    displayData();
    ledFlash(2, 125);
  } else {
    Serial.println(F("Device error"));
    while (1) {
      Serial.println(F("No device responding"));
      display3 = "No LoRa Device found";
      displayData();
      ledFlash(50, 50);  // long fast speed flash indicates LoRa device error
    }
  }
#endif

  // just to be for sure - use the default syncword
  LT.setSyncWord(LORA_MAC_PRIVATE_SYNCWORD);  // this is the default value
  // set the high sensitive mode
  // Sets LoRa device for the highest sensitivity at expense of slightly higher LNA current.
  // The alternative is setLowPowerReceive() for lower sensitivity with slightly reduced current.
  LT.setHighSensitivity();
  // start the device with default parameters
  LT.setupLoRa(FREQUENCY, OFFSET, SPREADING_FACTOR, BANDWIDTH, CODE_RATE, OPTIMISATION);
  Serial.println(F("LoRa setup is complete"));

  // debug information
  Serial.println();
  LT.printModemSettings();
  Serial.println();
  LT.printOperatingSettings();
  Serial.println();

  display3 = "LoRa init done";
  display4 = "Press BOOT button";
  display5 = "to toggle Frequency";
  displayData();
  delay(2000);

  // init the mode select button
  pinMode(BOOT_BUTTON_PIN, INPUT);
  attachInterrupt(BOOT_BUTTON_PIN, bootButtonPressed, RISING);

  // reset the counter arrays
  resetCounter();
  currentBaseFrequency = FREQUENCY;
  currentNumberOfChannels = NUMBER_OF_CHANNELS_1;

/*
  // display sample data  
  cnt[2] = 6;
  cnt[7] = 1;
  cnt[8] = 2;
  cnt[9] = 3;
  cnt[10] = 4;
  cnt[11] = 5;
  cnt[19] = 3;
*/

  drawFrame();
}

void drawFrame() {

#ifdef IS_TFT
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setFont(NULL);  // Pass NULL to revert to 'classic' fixed-space bitmap font.

  tft.setCursor(0, 0);
  tft.print("FreqAzr04 " + (String)(currentBaseFrequency / 1000) + "KHz " + (String)channelCounter + "|" + (String)completeRounds);
  tft.setCursor(0, 9);
  tft.print("5");
  tft.setCursor(0, 18);
  tft.print("4");
  tft.setCursor(0, 27);
  tft.print("3");
  tft.setCursor(0, 36);
  tft.print("2");
  tft.setCursor(0, 45);
  tft.print("1");
  if (modeCounter == 0) {
    tft.setCursor(0, 54);
    tft.print(" 12345678901234567890");
  } else {
    tft.setCursor(0, 54);
    tft.print(" 123456789012345");
  }

  // draw value lines
  const uint8_t zeroX = 8;
  const uint8_t zeroY = 51;
  const uint8_t distanceY = 6;
  for (uint8_t i = 0; i < currentNumberOfChannels; i++) {
    if (cnt[i] > 0) {
      if (cnt[i] < 6) {
        tft.drawLine(zeroX + (i * distanceY), zeroY, zeroX + (i * distanceY), zeroY - (cnt[i] * 9 - 4), ST77XX_GREEN);          // use line x 12 + x 13 for line
        tft.drawLine(zeroX + 1 + (i * distanceY), zeroY, zeroX + 1 + (i * distanceY), zeroY - (cnt[i] * 9 - 4), ST77XX_GREEN);  // use line x 12 + x 13 for line
      } else {
        // draw the line just up to '4'
        tft.drawLine(zeroX + (i * distanceY), zeroY, zeroX + (i * distanceY), 18, ST77XX_RED);          // use line x 12 + x 13 for line
        tft.drawLine(zeroX + 1 + (i * distanceY), zeroY, zeroX + 1 + (i * distanceY), 18, ST77XX_RED);  // use line x 12 + x 13 for line
        // and print a '+' in value 5
        tft.setCursor((i + 1) * distanceY, 8);
        tft.print("+");
      }
    }
  }
#endif

#ifdef IS_OLED
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Monospaced_plain_9);
  // maximum 7 lines with 'Monospaced_plain_9' font
  display.drawString(0, 0, "FreqAzr04 " + (String)(currentBaseFrequency / 1000) + "KHz " + (String)channelCounter + "|" + (String)completeRounds);
  display.drawString(0, 9, "5");
  display.drawString(0, 18, "4");
  display.drawString(0, 27, "3");
  display.drawString(0, 36, "2");
  display.drawString(0, 45, "1");
  if (modeCounter == 0) {
    display.drawString(0, 54, " 12345678901234567890");
  } else {
    display.drawString(0, 54, " 123456789012345");
  }

  // draw value lines
  for (uint8_t i = 0; i < currentNumberOfChannels; i++) {
    if (cnt[i] > 0) {
      if (cnt[i] < 6) {
        display.drawLine(7 + (i * 5), 54, 7 + (i * 5), 54 - (cnt[i] * 9 - 2));  // use line x 12 + x 13 for line
        display.drawLine(8 + (i * 5), 54, 8 + (i * 5), 54 - (cnt[i] * 9 - 2));  // use line x 12 + x 13 for line
      } else {
        // draw the line just up to '4'
        display.drawLine(7 + (i * 5), 54, 7 + (i * 5), 20);  // use line x 12 + x 13 for line
        display.drawLine(8 + (i * 5), 54, 8 + (i * 5), 20);  // use line x 12 + x 13 for line
        // and print a '+' in value 5
        display.drawString((i + 1) * 5, 9, "+");
      }
    }
  }
  display.display();
#endif
}

void resetCounter() {
  // init array
  for (uint8_t i = 0; i < MAXIMUM_NUMBER_OF_CHANNELS; i++) {
    cnt[i] = 0;
  }
  channelCounter = 1;
  completeRounds = 0;
}

void drawTestFrame() {

#ifdef IS_OLED
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Monospaced_plain_9);
  // maximum 7 lines
  display.drawString(0, 0, "+");
  display.drawString(0, 9, "9");
  display.drawString(0, 18, "18");
  display.drawString(0, 27, "27");
  display.drawString(0, 36, "36");
  display.drawString(0, 45, "45");
  display.drawString(0, 54, "0123456789X123456789Y12345");  // a part of the last 5 is shown

  // draw lines
  display.drawLine(10, 56, 10, 44);  // x 10 line is between char 2 and 3
  display.drawLine(11, 54, 11, 39);
  display.drawLine(12, 54, 12, 34);  // use line x 12 + x 13 for line
  display.drawLine(13, 54, 13, 29);  // use line x 12 + x 13 for line
  display.drawLine(14, 54, 14, 24);
  display.drawLine(15, 56, 15, 19);  // x 15 line is between char 3 and 4
  display.drawLine(16, 54, 16, 14);
  display.drawLine(17, 54, 17, 9);
  display.drawLine(18, 54, 18, 4);
  // we have 24 positions = channels, starting x = 7 from y = 54 to 0 (top) or y = 10 (one line on top for description)
  display.display();
#endif
}

void displayData() {
#ifdef IS_TFT
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setFont(NULL);  // Pass NULL to revert to 'classic' fixed-space bitmap font.
  tft.setCursor(0, 0);
  tft.print(display1);
  tft.setCursor(0, 10);
  tft.print(display2);
  tft.setCursor(0, 20);
  tft.print(display3);
  tft.setCursor(0, 30);
  tft.print(display4);
  tft.setCursor(0, 40);
  tft.print(display5);
  tft.setCursor(0, 50);
  tft.print(display6);
  tft.setCursor(0, 60);
  tft.print(display7);
  tft.setCursor(0, 70);
  tft.print(display8);
  tft.setCursor(0, 80);
  tft.print(display9);
  tft.setCursor(0, 90);
  tft.print(display10);
  tft.setCursor(0, 100);
  tft.print(display11);
  tft.setCursor(0, 110);
  tft.print(display12);
  tft.setCursor(0, 120);
  tft.print(display13);
#endif

#ifdef IS_OLED
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.setFont(ArialMT_Plain_10);  // create more fonts at http://oleddisplay.squix.ch/
  display.setFont(Monospaced_plain_9);
  display.drawString(0, 0, display1);
  display.drawString(0, 12, display2);
  display.drawString(0, 24, display3);
  display.drawString(0, 36, display4);
  display.drawString(0, 48, display5);
  display.display();
#endif
}

void clearDisplayData() {
  display1 = "";
  display2 = "";
  display3 = "";
  display4 = "";
  display5 = "";
  display6 = "";
  display7 = "";
  display8 = "";
  display9 = "";
  display10 = "";
  display11 = "";
  display12 = "";
  display13 = "";
}

void setVextControl(boolean trueIsOn) {
#ifdef IS_VEXT_CONTROL
  if (trueIsOn) {
    pinMode(VEXT_POWER_CONTROL_PIN, OUTPUT);
    digitalWrite(VEXT_POWER_CONTROL_PIN, LOW);
  } else {
    // pulled up, no need to drive it
    pinMode(VEXT_POWER_CONTROL_PIN, INPUT);
  }
#endif
}
