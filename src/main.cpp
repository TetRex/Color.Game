#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define I2C_SDA 5
#define I2C_SCL 6
#define BUZZER_PIN 9
#define BUTTON_PIN 1
#define RGB_LED_R 2
#define RGB_LED_G 4
#define RGB_LED_B 3


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct Color {
  float r, g, b;
  const char* name;
};

Color targets[] = {
  {1, 0, 0, "Red"},
  {0, 1, 0, "Green"},
  {0, 0, 1, "Blue"},
  {1, 1, 0, "Yellow"},
  {0.5, 0, 1, "Purple"}
};

// All colors including recognition mode colors
Color allColors[] = {
  {1, 0, 0, "Red"},
  {0, 1, 0, "Green"},
  {0, 0, 1, "Blue"},
  {1, 1, 0, "Yellow"},
  {1, 0.5, 0, "Orange"},
  {0.5, 0, 1, "Purple"},
  {0, 0.5, 1, "Navy"},
  {1, 0, 1, "Magenta"},
  {0.3, 0.3, 0, "Brown"},
  {1, 1, 1, "White"}
};

int numTargets = sizeof(targets)/sizeof(targets[0]);
int numAllColors = sizeof(allColors)/sizeof(allColors[0]);
Color currentTarget;
unsigned long startTime;
int lastIndex = -1;

#define LED_BRIGHTNESS 200

// Game Mode Tolerances
#define GAME_COLOR_TOLERANCE 0.8  // Default tolerance for game mode
#define GAME_RED_TOLERANCE 0.8    // Red in game mode
#define GAME_GREEN_TOLERANCE 0.8  // Green in game mode
#define GAME_YELLOW_TOLERANCE 0.7 // Yellow in game mode
#define GAME_PURPLE_TOLERANCE 0.8 // Purple in game mode
#define GAME_BLUE_TOLERANCE 1.0   // Blue in game mode (most forgiving)

// Recognition Mode Tolerances
#define REC_COLOR_TOLERANCE 0.8  // Default tolerance for recognition mode
#define REC_RED_TOLERANCE 0.9    // Red in recognition mode
#define REC_GREEN_TOLERANCE 0.7  // Green in recognition mode
#define REC_BLUE_TOLERANCE 1.0   // Blue in recognition mode (most forgiving)
#define REC_YELLOW_TOLERANCE 0.8 // Yellow in recognition mode
#define REC_ORANGE_TOLERANCE 0.7 // Orange in recognition mode
#define REC_PURPLE_TOLERANCE 0.7 // Purple in recognition mode
#define REC_NAVY_TOLERANCE 0.7   // Navy in recognition mode
#define REC_MAGENTA_TOLERANCE 0.7 // Magenta in recognition mode
#define REC_BROWN_TOLERANCE 0.8  // Brown in recognition mode
#define REC_WHITE_TOLERANCE 0.9  // White in recognition mode

// Button double-click variables
int buttonClickCount = 0;
unsigned long lastButtonPressTime = 0;
unsigned long lastButtonReleaseTime = 0;
bool buttonWasPressed = false;
#define DOUBLE_CLICK_TIMEOUT 600  // Time window for double-click in milliseconds
#define LONG_PRESS_DURATION 2000  // Duration for long press in milliseconds

// Color recognition mode
bool colorRecognitionMode = false;
bool longPressDetected = false;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

void buzzerBeep(int duration = 200, int frequency = 1000) {
  tone(BUZZER_PIN, frequency);
  delay(duration);
  noTone(BUZZER_PIN);
}

float getColorTolerance(const char* colorName, bool isRecognitionMode) {
  if (isRecognitionMode) {
    // Recognition mode tolerances
    if (strcmp(colorName, "Red") == 0) {
      return REC_RED_TOLERANCE;
    } else if (strcmp(colorName, "Green") == 0) {
      return REC_GREEN_TOLERANCE;
    } else if (strcmp(colorName, "Blue") == 0) {
      return REC_BLUE_TOLERANCE;
    } else if (strcmp(colorName, "Yellow") == 0) {
      return REC_YELLOW_TOLERANCE;
    } else if (strcmp(colorName, "Orange") == 0) {
      return REC_ORANGE_TOLERANCE;
    } else if (strcmp(colorName, "Purple") == 0) {
      return REC_PURPLE_TOLERANCE;
    } else if (strcmp(colorName, "Navy") == 0) {
      return REC_NAVY_TOLERANCE;
    } else if (strcmp(colorName, "Magenta") == 0) {
      return REC_MAGENTA_TOLERANCE;
    } else if (strcmp(colorName, "Brown") == 0) {
      return REC_BROWN_TOLERANCE;
    } else if (strcmp(colorName, "White") == 0) {
      return REC_WHITE_TOLERANCE;
    }
    return REC_COLOR_TOLERANCE;
  } else {
    // Game mode tolerances
    if (strcmp(colorName, "Red") == 0) {
      return GAME_RED_TOLERANCE;
    } else if (strcmp(colorName, "Green") == 0) {
      return GAME_GREEN_TOLERANCE;
    } else if (strcmp(colorName, "Yellow") == 0) {
      return GAME_YELLOW_TOLERANCE;
    } else if (strcmp(colorName, "Purple") == 0) {
      return GAME_PURPLE_TOLERANCE;
    } else if (strcmp(colorName, "Blue") == 0) {
      return GAME_BLUE_TOLERANCE;
    }
    return GAME_COLOR_TOLERANCE;
  }
}

const char* detectColor(float dr, float dg, float db) {
  float minDistance = 999.0;
  const char* closestColor = "Unknown";
  
  for (int i = 0; i < numAllColors; i++) {
    float distance = sqrt(pow(dr - allColors[i].r, 2) + pow(dg - allColors[i].g, 2) + pow(db - allColors[i].b, 2));
    if (distance < minDistance) {
      minDistance = distance;
      closestColor = allColors[i].name;
    }
  }
  
  return closestColor;
}

void changeColor() {
  int randomIndex;
  do {
    randomIndex = random(numTargets);
  } while (randomIndex == lastIndex && numTargets > 1);
  lastIndex = randomIndex;
  currentTarget = targets[randomIndex];
  startTime = millis();
  
  digitalWrite(RGB_LED_R, currentTarget.r > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_G, currentTarget.g > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_B, currentTarget.b > 0 ? HIGH : LOW);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Find %s object\n", currentTarget.name);
  display.display();
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!tcs.begin()) {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  randomSeed(analogRead(0));
  int randomIndex = random(numTargets);
  lastIndex = randomIndex;
  currentTarget = targets[randomIndex];
  
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

  // Check if button is pressed
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  
  if (buttonPressed && !buttonWasPressed) {
    // Button just pressed (transition from release to press)
    unsigned long currentTime = millis();
    
    // Check if this is part of a double-click
    if (currentTime - lastButtonReleaseTime < DOUBLE_CLICK_TIMEOUT) {
      buttonClickCount++;
    } else {
      buttonClickCount = 1;
    }
    
    lastButtonPressTime = currentTime;
    buttonWasPressed = true;
    longPressDetected = false;
    
    // If double-click detected, toggle color recognition mode
    if (buttonClickCount == 2) {
      if (colorRecognitionMode) {
        // Exit color recognition mode
        colorRecognitionMode = false;
        buzzerBeep(200, 1000);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Game Mode\nResumed\n");
        display.display();
        delay(1000);
      } else {
        // This is a normal double-click in game mode - change color
        buzzerBeep(300, 1200);
        delay(200);
        buzzerBeep(300, 1200);
        changeColor();
      }
      buttonClickCount = 0;
    } else {
      buzzerBeep(200, 1000);
    }
  } else if (buttonPressed && buttonWasPressed) {
    // Button is being held
    unsigned long pressedDuration = millis() - lastButtonPressTime;
    
    if (colorRecognitionMode) {
      // In color recognition mode - exit on long press
      if (pressedDuration > LONG_PRESS_DURATION && !longPressDetected) {
        longPressDetected = true;
        colorRecognitionMode = false;
        buzzerBeep(200, 1000);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Game Mode\nResumed\n");
        display.display();
        delay(1000);
      }
    } else {
      // In game mode - enter color recognition mode on long press
      if (pressedDuration > LONG_PRESS_DURATION && !longPressDetected) {
        longPressDetected = true;
        colorRecognitionMode = true;
        buzzerBeep(200, 1400);
        delay(100);
        buzzerBeep(200, 1400);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("Color Recognition\nMode ON\n");
        display.display();
        delay(1000);
      }
    }
  } else if (!buttonPressed && buttonWasPressed) {
    // Button just released - do nothing, stay in current mode
    lastButtonReleaseTime = millis();
    buttonWasPressed = false;
  }

  tcs.setInterrupt(false);
  delay(60);
  tcs.getRawData(&red, &green, &blue, &clear);
  tcs.setInterrupt(true);

  if (colorRecognitionMode) {
    // Color recognition mode - display detected color name with debug info
    if (clear > 0) {
      float dr = (float)red / clear;
      float dg = (float)green / clear;
      float db = (float)blue / clear;
      
      // Normalize to 0-1 range based on max value
      float maxVal = max({dr, dg, db});
      if (maxVal > 0) {
        dr /= maxVal;
        dg /= maxVal;
        db /= maxVal;
      }
      
      const char* detectedColor = detectColor(dr, dg, db);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.printf("Color: %s\n", detectedColor);
      display.printf("R:%.2f G:%.2f\n", dr, dg);
      display.printf("B:%.2f C:%d\n", db, clear);
      display.display();
      
      Serial.printf("Raw - R:%d G:%d B:%d C:%d | Norm - R:%.3f G:%.3f B:%.3f | Detected: %s\n", 
                     red, green, blue, clear, dr, dg, db, detectedColor);
    }
  } else {
    // Normal game mode
    if (clear > 0) {
      float dr = (float)red / clear;
      float dg = (float)green / clear;
      float db = (float)blue / clear;
      
      // Normalize to 0-1 range based on max value
      float maxVal = max({dr, dg, db});
      if (maxVal > 0) {
        dr /= maxVal;
        dg /= maxVal;
        db /= maxVal;
      }
      
      float distance = sqrt(pow(dr - currentTarget.r, 2) + pow(dg - currentTarget.g, 2) + pow(db - currentTarget.b, 2));

      display.clearDisplay();
      display.setCursor(0, 0);
      display.printf("Target: %s\n", currentTarget.name);
      display.printf("Detected:\nR: %.2f G: %.2f B: %.2f\n", dr, dg, db);
      float tolerance = getColorTolerance(currentTarget.name, false);
      if (distance < tolerance) {
        display.printf("MATCH!\n");
        unsigned long elapsed = millis() - startTime;
        display.printf("Time: %lu ms\n", elapsed);
        buzzerBeep(500, 1500);
        delay(1000);

        changeColor();
      }
      display.display();
    }
  }
  Serial.printf("R: %d, G: %d, B: %d, C: %d\n", red, green, blue, clear);

  delay(500);
}