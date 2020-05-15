// Display Library
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
// Temperature Libarary
#include "DHT.h"
#include <AceButton.h>
#include "ESPFlash.h"

using namespace ace_button;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1    // 4 // Reset pin # (or -1 if sharing Arduino reset pin)

// What temperature range can the relay be set to turn on or off
#define DEFAULT_TEMPERATURE 80
#define MIN_TEMPERATURE 75
#define MAX_TEMPERATURE 95
#define BOUNCE_TIME 120 // Fan will run for 2 min AFTER temperature drops to keep it from cutting in and out.
#define DOWN_PIN 12
#define UP_PIN 13

#define DHTPIN 14     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define TEMP_SETTING_FILE "/temp"
#define FORMAT_SPIFFS_IF_FAILED true

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// What temperature does the fan turn on
uint8_t setpoint = 75;
ButtonConfig downConfig;
AceButton downButton(&downConfig);//(DOWN_PIN);

ButtonConfig upConfig;
AceButton upButton(&upConfig);//(UP_PIN);

void handleDownEvent(AceButton*, uint8_t, uint8_t);
void handleUpEvent(AceButton*, uint8_t, uint8_t);

uint16_t getPersistantTemperature();
void setPersistantTemperature(uint16_t temp);

#define SETUP_DELAY 3000 // How long to press the button to enter setup mode.
#define SETUP_TIME 10000 // How long to stay in setup mode.
uint16_t setupMode = 0; // How much time of setup mode is left

uint32_t lastReading = 0;
uint32_t last = 0;

void setup() {
  Serial.begin(9600);

  Serial.println("Getting Persistant Temperature (maybe?)");
  setpoint = getPersistantTemperature();

  if(setpoint > MAX_TEMPERATURE || setpoint < MIN_TEMPERATURE){
    setPersistantTemperature(DEFAULT_TEMPERATURE);
    setpoint = getPersistantTemperature();  
  }
    
  Serial.print("Persistant Temperature: ");
  Serial.println(setpoint);

  
  pinMode(DOWN_PIN, INPUT_PULLUP); // INPUT OR INPUT_PULLUP or INPUT_PULLDOWN
  pinMode(UP_PIN, INPUT_PULLUP);

  downConfig.setEventHandler(handleDownEvent);
  //downConfig.setFeature(ButtonConfig::kFeatureClick);
  downConfig.setFeature(ButtonConfig::kFeatureLongPress);
  downConfig.setLongPressDelay(SETUP_DELAY);

  upButton.setEventHandler(handleUpEvent);
  //ubc->setFeature(ButtonConfig::kFeatureClick);

  downButton.init(DOWN_PIN);
  upButton.init(UP_PIN);

  Serial.println(F("DHT22 test!"));

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) // Address 0x3C for 128x32
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.display();
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(1, 0);
  display.println("Fan v1.0");
  display.display();
  Serial.println("Setup Complete");

  delay(1500);
}

void loop() {
  unsigned long current = millis();

  downButton.check();
  upButton.check();

  if (setupMode > 0) {
    Serial.print("SetupMode Value ");
    Serial.println(setupMode);

    display.clearDisplay();
    display.setFont(&FreeSans9pt7b);
    display.setCursor(0, 16);
    display.setTextSize(1); // Draw 3X-scale text
    display.setTextColor(WHITE);
    display.print("temp ");
    display.print(setpoint);
    display.print((char)247);
    display.println("F");
    display.display();

    uint16_t elapsed = current - last;
    if (setupMode - elapsed > 0) {
      setupMode = setupMode - elapsed;
    }
    else {
      setupMode = 0;      
      // Only write the persistant value after exiting settings mode.
      setPersistantTemperature(setpoint);
    }
  } else {
    // Wait a few seconds between measurements.
    if (lastReading + 2000 < current) {
      Serial.println("Taking Reading");
      lastReading = current;
      float h = dht.readHumidity();

      // Read temperature as Fahrenheit (isFahrenheit = true)
      float f = dht.readTemperature(true);

      // Check if any reads failed and exit early (to try again).
      if (isnan(h) || isnan(f)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
      }
      Serial.print(F("Humidity: "));
      Serial.print(h);
      Serial.print(F("%  Temperature: "));
      Serial.print(f);
      Serial.println(F("°F"));

      display.clearDisplay();
      display.setFont(&FreeSans9pt7b);
      display.setCursor(0, 16);
      display.setTextSize(1); // Draw 3X-scale text
      display.setTextColor(WHITE);
      display.print("temp ");
      display.print(f);
      display.print((char)247);
      display.println("F");
      display.display();
    }
  }
  last = current;
}
    

  /**
   * AceButton EventTypes: 
   * kEventPressed = 0; 
   * kEventReleased = 1;
   * kEventClicked = 2;
   * kEventDoubleClicked = 3;
   * kEventLongPressed = 4;
   * kEventRepeatPressed = 5;
   * kButtonStateUnknown = 127;
   */
void handleUpEvent(AceButton* /*button*/, uint8_t eventType, uint8_t /*buttonState*/) {
  if (setupMode > 0 && eventType == AceButton::kEventPressed){
    if(setpoint + 1 < MAX_TEMPERATURE)
      setpoint++;// = setpoint + 1;
    setupMode = SETUP_TIME;
  }
}

void handleDownEvent(AceButton* /*button*/, uint8_t eventType, uint8_t /*buttonState*/) {
  if(setupMode > 0 && eventType == AceButton::kEventPressed) {    
    if(setpoint - 1 > MIN_TEMPERATURE)
      setpoint--;// = setpoint - 1;
    setupMode = SETUP_TIME;
  } else if (eventType == AceButton::kEventLongPressed) {
      setupMode = SETUP_TIME;
      display.clearDisplay();
      display.setFont(&FreeSans9pt7b);
      display.setCursor(0, 16);
      display.setTextSize(1); // Draw 3X-scale text
      display.setTextColor(WHITE);
      display.println("SETUP");
      display.display();
      delay(2000);    
  } 
}

uint16_t getPersistantTemperature() {  
  ESPFlash<uint16_t> file(TEMP_SETTING_FILE);
  uint16_t val = file.get();  
  return val;
}

void setPersistantTemperature(uint16_t temp) {
  ESPFlash<uint16_t> file(TEMP_SETTING_FILE);
  file.set(temp);
}