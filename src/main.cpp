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
unsigned long roundStartTime;
int lastIndex = -1;

int score = 0;
bool roundActive = false;
#define ROUND_DURATION 30000
#define POINTS_PER_COLOR 100
#define LED_BRIGHTNESS 200

#define GAME_COLOR_TOLERANCE 0.8
#define GAME_RED_TOLERANCE 0.8
#define GAME_GREEN_TOLERANCE 0.8
#define GAME_YELLOW_TOLERANCE 0.6
#define GAME_PURPLE_TOLERANCE 0.8
#define GAME_BLUE_TOLERANCE 1.0

#define REC_COLOR_TOLERANCE 0.8
#define REC_RED_TOLERANCE 0.9
#define REC_GREEN_TOLERANCE 0.7
#define REC_BLUE_TOLERANCE 1.0
#define REC_YELLOW_TOLERANCE 0.8
#define REC_ORANGE_TOLERANCE 0.5
#define REC_PURPLE_TOLERANCE 0.7
#define REC_NAVY_TOLERANCE 0.7
#define REC_MAGENTA_TOLERANCE 0.7
#define REC_BROWN_TOLERANCE 0.8
#define REC_WHITE_TOLERANCE 0.9

int buttonClickCount = 0;
unsigned long lastButtonPressTime = 0;
unsigned long lastButtonReleaseTime = 0;
unsigned long lastClickTime = 0;
bool buttonWasPressed = false;
#define DOUBLE_CLICK_TIMEOUT 1500
#define LONG_PRESS_DURATION 2000

unsigned long lastScreenUpdate = 0;
#define SCREEN_UPDATE_INTERVAL 100

bool colorRecognitionMode = false;
bool longPressDetected = false;
bool startupScreenDisplayed = false;
int selectedMode = 0;  // 0 = Game, 1 = Detection

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

void setCursorCentered(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
}

void buzzerBeep(int duration = 200, int frequency = 1000) {
  tone(BUZZER_PIN, frequency);
  delay(duration);
  noTone(BUZZER_PIN);
}

float getColorTolerance(const char* colorName, bool isRecognitionMode) {
  if (isRecognitionMode) {
    if (strcmp(colorName, "Red") == 0) return REC_RED_TOLERANCE;
    else if (strcmp(colorName, "Green") == 0) return REC_GREEN_TOLERANCE;
    else if (strcmp(colorName, "Blue") == 0) return REC_BLUE_TOLERANCE;
    else if (strcmp(colorName, "Yellow") == 0) return REC_YELLOW_TOLERANCE;
    else if (strcmp(colorName, "Orange") == 0) return REC_ORANGE_TOLERANCE;
    else if (strcmp(colorName, "Purple") == 0) return REC_PURPLE_TOLERANCE;
    else if (strcmp(colorName, "Navy") == 0) return REC_NAVY_TOLERANCE;
    else if (strcmp(colorName, "Magenta") == 0) return REC_MAGENTA_TOLERANCE;
    else if (strcmp(colorName, "Brown") == 0) return REC_BROWN_TOLERANCE;
    else if (strcmp(colorName, "White") == 0) return REC_WHITE_TOLERANCE;
    return REC_COLOR_TOLERANCE;
  } else {
    if (strcmp(colorName, "Red") == 0) return GAME_RED_TOLERANCE;
    else if (strcmp(colorName, "Green") == 0) return GAME_GREEN_TOLERANCE;
    else if (strcmp(colorName, "Yellow") == 0) return GAME_YELLOW_TOLERANCE;
    else if (strcmp(colorName, "Purple") == 0) return GAME_PURPLE_TOLERANCE;
    else if (strcmp(colorName, "Blue") == 0) return GAME_BLUE_TOLERANCE;
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
  display.setTextSize(2);
  char buffer[50];
  sprintf(buffer, "Find %s", currentTarget.name);
  setCursorCentered(buffer);
  display.printf("%s", buffer);
  display.display();
  delay(2000);
}

void startNewRound() {
  // 3-second countdown timer
  for (int i = 3; i > 0; i--) {
    display.clearDisplay();
    display.setTextSize(3);
    char buffer[10];
    sprintf(buffer, "%d", i);
    setCursorCentered(buffer);
    display.printf("%s", buffer);
    display.display();
    delay(1000);
  }
  
  score = 0;
  roundStartTime = millis();
  changeColor();
}

void endRound() {
  roundActive = false;
  startupScreenDisplayed = false;
  
  // Turn off LED
  digitalWrite(RGB_LED_R, LOW);
  digitalWrite(RGB_LED_G, LOW);
  digitalWrite(RGB_LED_B, LOW);
  
  display.clearDisplay();
  char buffer[50];
  display.setTextSize(2);
  sprintf(buffer, "Score: %d", score);
  setCursorCentered(buffer);
  display.printf("%s", buffer);
  display.setTextSize(2);
  display.display();
  buzzerBeep(500, 1200);
  delay(500);
  buzzerBeep(500, 1200);
  delay(2000);
}

void showStartScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Calculate centered positions for each line
  int16_t x1, y1;
  uint16_t w1, h1;
  display.getTextBounds("Press twice", 0, 0, &x1, &y1, &w1, &h1);
  int16_t x_pos1 = (SCREEN_WIDTH - w1) / 2;
  
  uint16_t w2, h2;
  display.getTextBounds("to start game!", 0, 0, &x1, &y1, &w2, &h2);
  int16_t x_pos2 = (SCREEN_WIDTH - w2) / 2;
  
  // Draw text with vertical spacing
  display.setCursor(x_pos1, 20);
  display.printf("Press twice");
  display.setCursor(x_pos2, 40);
  display.printf("to start game!");
  display.display();
}

void showColorGameScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  setCursorCentered("Color Game");
  display.printf("Color Game");
  display.display();
}

void showModeMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Display title
  display.setCursor(40, 5);
  display.printf("Select Mode");
  
  // Display Game option
  display.setCursor(10, 25);
  if (selectedMode == 0) {
    display.printf(">> Game");
  } else {
    display.printf("   Game");
  }
  
  // Display Detection option
  display.setCursor(10, 45);
  if (selectedMode == 1) {
    display.printf(">> Detection");
  } else {
    display.printf("   Detection");
  }
  
  display.display();
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
  
  showColorGameScreen();
  delay(1000);
  showModeMenu();
}

void loop() {
  if (!roundActive && !colorRecognitionMode) {
    if (!startupScreenDisplayed) {
      showModeMenu();
      startupScreenDisplayed = true;
    }
  }
  
  uint16_t clear, red, green, blue;
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long currentTime = millis();
  
  // Simple button state tracking
  if (buttonPressed && !buttonWasPressed) {
    // Button just pressed
    buttonWasPressed = true;
    lastButtonPressTime = currentTime;
    longPressDetected = false;
  } 
  else if (!buttonPressed && buttonWasPressed) {
    // Button just released
    unsigned long pressDuration = currentTime - lastButtonPressTime;
    
    if (pressDuration >= LONG_PRESS_DURATION) {
      // Long press detected
      if (roundActive || colorRecognitionMode) {
        // Exit Game or Detection mode and return to menu
        colorRecognitionMode = false;
        roundActive = false;
        startupScreenDisplayed = false;
        
        // Turn off LED
        digitalWrite(RGB_LED_R, LOW);
        digitalWrite(RGB_LED_G, LOW);
        digitalWrite(RGB_LED_B, LOW);
        
        buzzerBeep(200, 1000);
        showModeMenu();
      } else {
        // In menu mode - start selected mode
        if (selectedMode == 0) {
          // Start Game mode
          colorRecognitionMode = false;
          roundActive = true;
          startupScreenDisplayed = false;
          buzzerBeep(200, 1000);
          startNewRound();
        } else {
          // Start Detection mode
          colorRecognitionMode = true;
          roundActive = false;
          startupScreenDisplayed = false;
          buzzerBeep(200, 1400);
          delay(100);
          buzzerBeep(200, 1400);
          display.clearDisplay();
          setCursorCentered("Detection");
          display.printf("Detection");
          display.display();
          delay(1000);
        }
      }
    } else {
      // Short press - toggle selected mode (only in menu)
      if (!roundActive && !colorRecognitionMode) {
        selectedMode = 1 - selectedMode;  // Toggle between 0 and 1
        showModeMenu();
        buzzerBeep(200, 800);
      }
    }
    
    buttonWasPressed = false;
  } 
  else if (buttonPressed && buttonWasPressed) {
    // Button still pressed - check for long press
    unsigned long pressDuration = currentTime - lastButtonPressTime;
    if (pressDuration >= LONG_PRESS_DURATION && !longPressDetected) {
      longPressDetected = true;
    }
  }

  tcs.setInterrupt(false);
  delay(60);
  tcs.getRawData(&red, &green, &blue, &clear);
  tcs.setInterrupt(true);

  if (colorRecognitionMode) {
    if (clear > 0) {
      float dr = (float)red / clear;
      float dg = (float)green / clear;
      float db = (float)blue / clear;
      
      float maxVal = max({dr, dg, db});
      if (maxVal > 0) {
        dr /= maxVal;
        dg /= maxVal;
        db /= maxVal;
      }
      
      const char* detectedColor = detectColor(dr, dg, db);

      display.clearDisplay();
      display.setTextSize(2);
      setCursorCentered(detectedColor);
      display.printf("%s", detectedColor);
      display.setTextSize(2);
      display.display();
      
      Serial.printf("Raw - R:%d G:%d B:%d C:%d | Norm - R:%.3f G:%.3f B:%.3f | Detected: %s\n", 
                     red, green, blue, clear, dr, dg, db, detectedColor);
    }
  } else if (roundActive) {
    if (clear > 0) {
      float dr = (float)red / clear;
      float dg = (float)green / clear;
      float db = (float)blue / clear;
      
      float maxVal = max({dr, dg, db});
      if (maxVal > 0) {
        dr /= maxVal;
        dg /= maxVal;
        db /= maxVal;
      }
      
      float distance = sqrt(pow(dr - currentTarget.r, 2) + pow(dg - currentTarget.g, 2) + pow(db - currentTarget.b, 2));
      
      unsigned long elapsedTime = millis() - roundStartTime;
      unsigned long remainingTime = (elapsedTime < ROUND_DURATION) ? (ROUND_DURATION - elapsedTime) / 1000 : 0;

      display.clearDisplay();
      display.setTextSize(2);
      char buffer[50];
      
      sprintf(buffer, "Find: %s", currentTarget.name);
      setCursorCentered(buffer);
      display.printf("%s", buffer);
      
      float tolerance = getColorTolerance(currentTarget.name, false);
      if (distance < tolerance) {
        score += POINTS_PER_COLOR;
        display.display();
        delay(200);
        
        display.clearDisplay();
        display.setTextSize(2);
        setCursorCentered("MATCH!");
        display.printf("MATCH!");
        display.setTextSize(2);
        display.display();
        buzzerBeep(500, 1500);
        delay(1000);
        changeColor();
      }
      
      // Display all game info in one render
      display.clearDisplay();
      display.setTextSize(1);
      
      char buffer1[50];
      sprintf(buffer1, "Find: %s", currentTarget.name);
      display.setCursor(0, 5);
      display.printf("%s", buffer1);
      
      char buffer2[50];
      sprintf(buffer2, "Score: %d", score);
      display.setCursor(0, 25);
      display.printf("%s", buffer2);
      
      char buffer3[50];
      sprintf(buffer3, "Time: %lu s", remainingTime);
      display.setCursor(0, 45);
      display.printf("%s", buffer3);
      
      display.display();
      
      if (elapsedTime >= ROUND_DURATION) {
        endRound();
      }
    }
  }
  Serial.printf("R: %d, G: %d, B: %d, C: %d\n", red, green, blue, clear);

  delay(500);
}