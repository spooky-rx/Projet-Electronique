#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire);

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Adresse I2C
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Test OLED OK");
  display.display();
}

void loop() {
  // Rien à répéter
}

