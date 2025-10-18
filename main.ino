#include <HID-Project.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== OLED SCREEN SETTINGS =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== BUTTON PINS (9 buttons) =====
const int buttonPins[9] = {4, 5, 6, 7, 8, 9, 10, 15, A1};
bool buttonStates[9] = {false};
bool lastButtonStates[9] = {false};

// ===== ROTARY ENCODER PINS =====
#define ENCODER_CLK 14
#define ENCODER_DT 16
#define ENCODER_SW A0

// Encoder state tracking
volatile int encoderPos = 0;
int lastEncoderPos = 0;
uint8_t encoderState = 0;
bool lastEncoderButtonState = HIGH;
unsigned long lastModeChangeFromEncoder = 0;
const int modeChangeDebounce = 200; // 200ms delay for mode changes

// Encoder state table for reliable direction detection
// This uses Gray Code for proper quadrature decoding
const int8_t encoderStates[] = {
  0, -1, 1, 0,
  1, 0, 0, -1,
  -1, 0, 0, 1,
  0, 1, -1, 0
};

// ===== MODE SYSTEM =====
#define TOTAL_MODES 5
int currentMode = 1;
unsigned long lastModeChangeTime = 0;
const int modeDisplayDuration = 2000;
bool showingModeChange = false;

// ===== ENCODER MODE SYSTEM =====
bool encoderVolumeMode = false;
unsigned long lastEncoderButtonPress = 0;
const int encoderButtonDebounce = 300;

// ===== ANIMATION VARIABLES =====
int animFrame = 0;
unsigned long lastAnimTime = 0;
const int animDelay = 100;

// ===== DINO GAME VARIABLES =====
bool gameRunning = false;
int dinoY = 40;
int dinoVelocity = 0;
bool dinoJumping = false;
const int gravity = 2;
const int jumpPower = -12;
const int groundLevel = 40;
int obstacleX = 128;
int obstacleSpeed = 4;
int gameScore = 0;
bool gameOver = false;
unsigned long lastGameUpdate = 0;
const int gameUpdateDelay = 50;

// ===== SETUP FUNCTION =====
void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  Consumer.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while(1);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 15);
  display.println(F("KLAVYE"));
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println(F("Multi-Mode"));
  display.display();
  delay(1500);

  for(int i = 0; i < 9; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  // Initialize encoder state
  encoderState = (digitalRead(ENCODER_CLK) << 1) | digitalRead(ENCODER_DT);

  showModeChange();
}

// ===== MAIN LOOP =====
void loop() {
  handleEncoder();
  handleButtons();
  updateDisplay();
}

// ===== ENCODER HANDLER =====
void handleEncoder() {
  // Read both pins
  uint8_t clk = digitalRead(ENCODER_CLK);
  uint8_t dt = digitalRead(ENCODER_DT);

  // Create new state from current pins
  uint8_t newState = (clk << 1) | dt;

  // Combine with old state to create index
  uint8_t index = (encoderState << 2) | newState;

  // Look up direction from state table
  int8_t direction = encoderStates[index];

  // Update encoder position
  encoderPos += direction;

  // Store new state
  encoderState = newState;

  // Check if position changed (complete detent/step)
  if(encoderPos != lastEncoderPos) {
    int diff = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;

    if(encoderVolumeMode) {
      // VOLUME CONTROL MODE
      if(diff > 0) {
        // Counter-Clockwise = Volume UP
        Consumer.write(MEDIA_VOLUME_UP);
      } else if(diff < 0) {
        // Clockwise = Volume DOWN
        Consumer.write(MEDIA_VOLUME_DOWN);
      }
      delay(2);
    } else {
      // MODE CHANGE MODE
      if(diff > 0) {
        // Counter-Clockwise = Next mode
        currentMode++;
        if(currentMode > TOTAL_MODES) currentMode = 1;
      } else if(diff < 0) {
        // Clockwise = Previous mode
        currentMode--;
        if(currentMode < 1) currentMode = TOTAL_MODES;
      }
      showModeChange();

      if(gameRunning && currentMode != 5) {
        gameRunning = false;
      }
    }
  }

  bool currentButtonState = digitalRead(ENCODER_SW);

  if(currentButtonState == LOW && lastEncoderButtonState == HIGH) {
    if(millis() - lastEncoderButtonPress > encoderButtonDebounce) {
      encoderVolumeMode = !encoderVolumeMode;

      if(encoderVolumeMode) {
        showMessage(F("VOL MODE"), 1000);
      } else {
        showMessage(F("MODE SEL"), 1000);
      }

      lastEncoderButtonPress = millis();
    }
  }

  lastEncoderButtonState = currentButtonState;
}

// ===== BUTTON HANDLER =====
void handleButtons() {
  for(int i = 0; i < 9; i++) {
    buttonStates[i] = !digitalRead(buttonPins[i]);

    if(buttonStates[i] && !lastButtonStates[i]) {
      executeShortcut(i);
    }

    lastButtonStates[i] = buttonStates[i];
  }
}

// ===== MODE-BASED SHORTCUT EXECUTION =====
void executeShortcut(int buttonIndex) {
  switch(currentMode) {
    case 1: executeMode1(buttonIndex); break;
    case 2: executeMode2(buttonIndex); break;
    case 3: executeMode3(buttonIndex); break;
    case 4: executeMode4(buttonIndex); break;
    case 5: executeMode5(buttonIndex); break;
  }
}

// ===== MODE 1: BASIC SHORTCUTS =====
void executeMode1(int btn) {
  switch(btn) {
    case 0:
      sendKeyCombo(KEY_LEFT_CTRL, 'c');
      showMessage(F("Copy"), 800);
      break;
    case 1:
      sendKeyCombo(KEY_LEFT_CTRL, 'v');
      showMessage(F("Paste"), 800);
      break;
    case 2:
      sendKeyCombo(KEY_LEFT_CTRL, 'z');
      showMessage(F("Undo"), 800);
      break;
    case 3:
      sendKeyCombo(KEY_LEFT_CTRL, 's');
      showMessage(F("Save"), 800);
      break;
    case 4:
      sendKeyCombo(KEY_LEFT_CTRL, 't');
      showMessage(F("New Tab"), 800);
      break;
    case 5:
      sendKeyCombo(KEY_LEFT_CTRL, 'w');
      showMessage(F("Close"), 800);
      break;
    case 6:
      sendKeyCombo3(KEY_LEFT_GUI, KEY_LEFT_SHIFT, 's');
      showMessage(F("Screen"), 800);
      break;
    case 7:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_ESC);
      showMessage(F("Task Mgr"), 800);
      break;
    case 8:
      sendKeyCombo(KEY_LEFT_GUI, 'd');
      showMessage(F("Desktop"), 800);
      break;
  }
}

// ===== MODE 2: MEDIA CONTROLS =====
void executeMode2(int btn) {
  switch(btn) {
    case 0:
      Consumer.write(MEDIA_PLAY_PAUSE);
      showMessage(F("Play"), 800);
      break;
    case 1:
      Consumer.write(MEDIA_NEXT);
      showMessage(F("Next"), 800);
      break;
    case 2:
      Consumer.write(MEDIA_PREVIOUS);
      showMessage(F("Prev"), 800);
      break;
    case 3:
      Consumer.write(MEDIA_VOLUME_UP);
      showMessage(F("Vol +"), 800);
      break;
    case 4:
      Consumer.write(MEDIA_VOLUME_DOWN);
      showMessage(F("Vol -"), 800);
      break;
    case 5:
      Consumer.write(MEDIA_VOLUME_MUTE);
      showMessage(F("Mute"), 800);
      break;
    case 6:
      sendKeyCombo(KEY_LEFT_GUI, 'l');
      showMessage(F("Lock"), 800);
      break;
    case 7:
      sendKeyCombo(KEY_LEFT_ALT, KEY_F4);
      showMessage(F("Alt+F4"), 800);
      break;
    case 8:
      sendKeyCombo(KEY_LEFT_ALT, KEY_TAB);
      showMessage(F("Alt+Tab"), 800);
      break;
  }
}

// ===== MODE 3: CODING SHORTCUTS =====
void executeMode3(int btn) {
  switch(btn) {
    case 0:
      sendKeyCombo(KEY_LEFT_CTRL, '/');
      showMessage(F("Comment"), 800);
      break;
    case 1:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 'f');
      showMessage(F("Format"), 800);
      break;
    case 2:
      sendKeyCombo(KEY_LEFT_CTRL, 'f');
      showMessage(F("Find"), 800);
      break;
    case 3:
      sendKeyCombo(KEY_F5, 0);
      showMessage(F("Run"), 800);
      break;
    case 4:
      sendKeyCombo(KEY_LEFT_CTRL, 'b');
      showMessage(F("Build"), 800);
      break;
    case 5:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 'p');
      showMessage(F("Command"), 800);
      break;
    case 6:
      sendKeyCombo(KEY_LEFT_CTRL, '`');
      showMessage(F("Terminal"), 800);
      break;
    case 7:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 'k');
      showMessage(F("Del Line"), 800);
      break;
    case 8:
      sendKeyCombo(KEY_LEFT_CTRL, 'd');
      showMessage(F("Duplicate"), 800);
      break;
  }
}

// ===== MODE 4: CUSTOM SHORTCUTS =====
void executeMode4(int btn) {
  switch(btn) {
    case 0:
      Keyboard.print(F("Hello World!"));
      showMessage(F("Text 1"), 800);
      break;
    case 1:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 'n');
      showMessage(F("Incognito"), 800);
      break;
    case 2:
      sendKeyCombo(KEY_LEFT_GUI, 'r');
      showMessage(F("Run"), 800);
      break;
    case 3:
      sendKeyCombo(KEY_LEFT_ALT, KEY_F4);
      showMessage(F("Close"), 800);
      break;
    case 4:
      Keyboard.print(F("https://"));
      showMessage(F("URL"), 800);
      break;
    case 5:
      sendKeyCombo(KEY_F11, 0);
      showMessage(F("Fullscr"), 800);
      break;
    case 6:
      sendKeyCombo3(KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_ESC);
      showMessage(F("Task Mgr"), 800);
      break;
    case 7:
      Keyboard.print(F("Macro 7"));
      showMessage(F("Macro 7"), 800);
      break;
    case 8:
      Keyboard.print(F("Macro 8"));
      showMessage(F("Macro 8"), 800);
      break;
  }
}

// ===== MODE 5: DINO GAME =====
void executeMode5(int btn) {
  if(!gameRunning) {
    startDinoGame();
  } else if(!gameOver) {
    if(!dinoJumping) {
      dinoVelocity = jumpPower;
      dinoJumping = true;
    }
  } else {
    startDinoGame();
  }
}

// ===== START DINO GAME =====
void startDinoGame() {
  gameRunning = true;
  gameOver = false;
  dinoY = groundLevel;
  dinoVelocity = 0;
  dinoJumping = false;
  obstacleX = 128;
  gameScore = 0;
  obstacleSpeed = 4;
}

// ===== UPDATE DINO GAME =====
void updateDinoGame() {
  if(!gameRunning || gameOver) return;

  if(dinoJumping || dinoY < groundLevel) {
    dinoVelocity += gravity;
    dinoY += dinoVelocity;

    if(dinoY >= groundLevel) {
      dinoY = groundLevel;
      dinoVelocity = 0;
      dinoJumping = false;
    }
  }

  obstacleX -= obstacleSpeed;

  if(obstacleX < -10) {
    obstacleX = 128;
    gameScore++;

    if(gameScore % 5 == 0 && obstacleSpeed < 8) {
      obstacleSpeed++;
    }
  }

  if(obstacleX >= 5 && obstacleX <= 20) {
    if(dinoY >= groundLevel - 5) {
      gameOver = true;
    }
  }
}

// ===== DRAW DINO GAME =====
void drawDinoGame() {
  display.clearDisplay();

  if(gameOver) {
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.print(F("GAME OVER"));

    display.setTextSize(1);
    display.setCursor(30, 35);
    display.print(F("Score: "));
    display.print(gameScore);

    display.setCursor(15, 50);
    display.print(F("Press restart"));
  } else {
    display.drawLine(0, groundLevel + 10, 127, groundLevel + 10, SSD1306_WHITE);
    display.fillRect(10, dinoY, 10, 10, SSD1306_WHITE);
    display.drawPixel(12, dinoY + 2, SSD1306_BLACK);
    display.fillRect(obstacleX, groundLevel + 5, 8, 5, SSD1306_WHITE);
    display.fillRect(obstacleX + 2, groundLevel, 4, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(90, 0);
    display.print(F("S:"));
    display.print(gameScore);
  }

  display.display();
}

// ===== HELPER: Send 2-key combo =====
void sendKeyCombo(uint8_t key1, uint8_t key2) {
  if(key2 == 0) {
    Keyboard.press(key1);
  } else {
    Keyboard.press(key1);
    Keyboard.press(key2);
  }
  delay(50);
  Keyboard.releaseAll();
}

// ===== HELPER: Send 3-key combo =====
void sendKeyCombo3(uint8_t key1, uint8_t key2, uint8_t key3) {
  Keyboard.press(key1);
  Keyboard.press(key2);
  Keyboard.press(key3);
  delay(50);
  Keyboard.releaseAll();
}

// ===== MODE CHANGE DISPLAY =====
void showModeChange() {
  showingModeChange = true;
  lastModeChangeTime = millis();

  display.clearDisplay();
  display.drawRect(20, 10, 88, 44, SSD1306_WHITE);
  display.fillRect(22, 12, 84, 40, SSD1306_BLACK);

  display.setTextSize(3);
  display.setCursor(30, 18);
  display.print(F("MOD"));
  display.print(currentMode);

  display.setTextSize(1);
  display.setCursor(40, 42);

  // Mode names inline to save memory
  switch(currentMode) {
    case 1: display.print(F("BASIC")); break;
    case 2: display.print(F("MEDIA")); break;
    case 3: display.print(F("CODE")); break;
    case 4: display.print(F("CUSTOM")); break;
    case 5: display.print(F("GAME")); break;
  }

  display.display();
}

// ===== SHOW MESSAGE ON SCREEN =====
void showMessage(const __FlashStringHelper* message, int duration) {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);

  // Mode icons inline
  display.print(F("["));
  switch(currentMode) {
    case 1: display.print(F("B")); break;
    case 2: display.print(F("M")); break;
    case 3: display.print(F("C")); break;
    case 4: display.print(F("X")); break;
    case 5: display.print(F("G")); break;
  }
  display.print(F("] M"));
  display.print(currentMode);

  display.setTextSize(2);
  display.setCursor(10, 25);
  display.println(message);

  display.display();
  delay(duration);
}

// ===== DISPLAY UPDATE =====
void updateDisplay() {
  if(showingModeChange) {
    if(millis() - lastModeChangeTime > modeDisplayDuration) {
      showingModeChange = false;
    } else {
      return;
    }
  }

  if(currentMode == 5 && gameRunning) {
    if(millis() - lastGameUpdate > gameUpdateDelay) {
      updateDinoGame();
      drawDinoGame();
      lastGameUpdate = millis();
    }
  }
  else if(millis() - lastAnimTime > animDelay) {
    drawModeAnimation();
    lastAnimTime = millis();
  }
}

// ===== MODE-BASED ANIMATION =====
void drawModeAnimation() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);

  // Show mode name
  switch(currentMode) {
    case 1: display.print(F("[B] BASIC")); break;
    case 2: display.print(F("[M] MEDIA")); break;
    case 3: display.print(F("[C] CODE")); break;
    case 4: display.print(F("[X] CUSTOM")); break;
    case 5: display.print(F("[G] GAME")); break;
  }

  switch(currentMode) {
    case 1:
      {
        int centerX = 64 + (int)(30 * cos(animFrame * 0.1));
        int centerY = 32 + (int)(15 * sin(animFrame * 0.15));
        display.fillCircle(centerX, centerY, 8, SSD1306_WHITE);
      }
      break;

    case 2:
      {
        for(int i = 0; i < 5; i++) {
          int h = 10 + abs((int)(15 * sin((animFrame + i * 10) * 0.2)));
          display.fillRect(20 + i * 20, 40 - h/2, 10, h, SSD1306_WHITE);
        }
      }
      break;

    case 3:
      {
        display.drawRect(54, 22, 20, 20, SSD1306_WHITE);
        int offset = (animFrame % 20) - 10;
        display.fillRect(64 + offset, 32, 2, 2, SSD1306_WHITE);
      }
      break;

    case 4:
      {
        for(int i = 0; i < 3; i++) {
          int x = 30 + i * 35;
          int y = 32 + (int)(10 * sin((animFrame + i * 20) * 0.1));
          display.fillCircle(x, y, 3, SSD1306_WHITE);
        }
      }
      break;

    case 5:
      {
        if(!gameRunning) {
          display.setTextSize(1);
          display.setCursor(25, 20);
          display.print(F("DINO GAME"));
          display.setCursor(15, 35);
          display.print(F("Press any key"));

          int dinoX = 50 + (int)(10 * sin(animFrame * 0.15));
          display.fillRect(dinoX, 48, 8, 8, SSD1306_WHITE);
          display.drawPixel(dinoX + 2, 50, SSD1306_BLACK);
        }
      }
      break;
  }

  animFrame++;
  if(animFrame > 62) animFrame = 0;

  display.display();
}