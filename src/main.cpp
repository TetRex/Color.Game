#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// Pin definitions
#define I2C_SDA 5
#define I2C_SCL 6
#define BUZZER_PIN 9
#define BUTTON_PIN 1
#define RGB_LED_R 2
#define RGB_LED_G 4
#define RGB_LED_B 3

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct Color {
  uint8_t r, g, b;
  const char* name;
};

Color targets[] = {
  {1, 0, 0, "Red"},
  {0, 1, 0, "Green"},
  {0, 0, 1, "Blue"}
};

int numTargets = sizeof(targets)/sizeof(targets[0]);
Color currentTarget;
unsigned long startTime;
int lastIndex = -1;

// LED brightness values (0-255)
#define LED_BRIGHTNESS 200

// Color sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize color sensor
  if (!tcs.begin()) {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Set initial target
  randomSeed(analogRead(0)); // for random
  int randomIndex = random(numTargets);
  lastIndex = randomIndex;
  currentTarget = targets[randomIndex];
  
  // Set LED color based on target
  digitalWrite(RGB_LED_R, currentTarget.r > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_G, currentTarget.g > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_B, currentTarget.b > 0 ? HIGH : LOW);
  
  startTime = millis();
  display.setCursor(0, 0);
  display.printf("Find %s object\n", currentTarget.name);
  display.display();
  delay(2000);
}

void loop() {
  uint16_t clear, red, green, blue;

  tcs.setInterrupt(false);  // Turn on LED
  delay(60);  // Takes 60ms to read
  tcs.getRawData(&red, &green, &blue, &clear);
  tcs.setInterrupt(true);  // Turn off LED

  if (clear > 0) {
    float dr = (float)red / clear * 255;
    float dg = (float)green / clear * 255;
    float db = (float)blue / clear * 255;
    float distance = sqrt(pow(dr - currentTarget.r, 2) + pow(dg - currentTarget.g, 2) + pow(db - currentTarget.b, 2));

    display.clearDisplay();
    display.setCursor(0, 0);
    display.printf("Target: %s\n", currentTarget.name);
    display.printf("Detected:\nR: %.0f G: %.0f B: %.0f\n", dr, dg, db);
    if (distance < 150) {
      display.printf("MATCH!\n");
      unsigned long elapsed = millis() - startTime;
      display.printf("Time: %lu ms\n", elapsed);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(500);
      digitalWrite(BUZZER_PIN, LOW);
      delay(1000);
      // New target - ensure it's different from the last one
      int randomIndex;
      do {
        randomIndex = random(numTargets);
      } while (randomIndex == lastIndex && numTargets > 1);
      lastIndex = randomIndex;
      currentTarget = targets[randomIndex];
      startTime = millis();
      
      // Set LED color based on target
      digitalWrite(RGB_LED_R, currentTarget.r > 0 ? HIGH : LOW);
      digitalWrite(RGB_LED_G, currentTarget.g > 0 ? HIGH : LOW);
      digitalWrite(RGB_LED_B, currentTarget.b > 0 ? HIGH : LOW);
      
      display.clearDisplay();
      display.setCursor(0, 0);
      display.printf("Find %s object\n", currentTarget.name);
      display.display();
      delay(2000); // wait before starting detection
    }
    display.display();
  }

  // Print to Serial for debugging
  Serial.printf("R: %d, G: %d, B: %d, C: %d\n", red, green, blue, clear);

  delay(500);
}