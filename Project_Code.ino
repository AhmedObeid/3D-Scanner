/*
          Coded by Eng. Ahmed M. Ubayed (الباشمهندث)
          Made with Love and Passion!
          as part of U34. Research Project
*/

/*
        Project: 3D Scanner UI 
        Hardware: Arduino Mega + 3.5" TFT Shield (ILI9486) + AccelStepper
        
        LOGIC & HARDWARE:
        - Elevator: T8 Pitch 4mm, 1.8deg, 1/16 Microstepping. (800 steps/mm).
        - Elevator Control: Absolute Positioning (mm).
        - Scanning: Continuous smooth movement. 
*/

#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <TouchScreen.h>
#include <AccelStepper.h>

// --- FONTS ---
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ==========================================
//      HARDWARE PINS
// ==========================================

const int XP = 8, XM = A2, YP = A3, YM = 9;
const int TS_LEFT = 900, TS_RT = 130, TS_TOP = 950, TS_BOT = 120;

#define TABLE_STEP_PIN 51
#define TABLE_DIR_PIN 49
#define TABLE_EN_PIN 43

#define ELEV1_STEP_PIN 47
#define ELEV1_DIR_PIN 45
#define ELEV1_EN_PIN 41

#define ELEV2_STEP_PIN 33
#define ELEV2_DIR_PIN 39
#define ELEV2_EN_PIN 35

#define CAM_STEP_PIN 31
#define CAM_DIR_PIN 53
#define CAM_EN_PIN 29

#define RELAY_PIN 38

#define LIM_ELEV_DOWN_1 21
#define LIM_ELEV_DOWN_2 20
#define LIM_ELEV_UP_1 19
#define LIM_ELEV_UP_2 18

// --- MOTOR OBJECTS ---
AccelStepper tableStepper(1, TABLE_STEP_PIN, TABLE_DIR_PIN);
AccelStepper elevStepper1(1, ELEV1_STEP_PIN, ELEV1_DIR_PIN);
AccelStepper elevStepper2(1, ELEV2_STEP_PIN, ELEV2_DIR_PIN);
AccelStepper camStepper(1, CAM_STEP_PIN, CAM_DIR_PIN);

// --- CALCULATIONS ---
#define ELEV_STEPS_PER_MM 800.0
#define CAM_MAX_SPEED 100.0
#define CAM_ACCEL 80.0

// --- COLORS ("OLED CYBER" THEME) ---
#define C_BG 0x0000
#define C_PANEL 0x10A2
#define C_ACCENT 0x07FF
#define C_BTN 0x03EF
#define C_BTN_DIM 0x2124
#define C_TEXT 0xFFFF
#define C_TEXT_DIM 0x8410
#define C_RED 0xF800
#define C_GREEN 0x07E0
#define C_ORANGE 0xFC00

// --- OBJECTS ---
MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// --- GLOBAL VARIABLES ---
int pixel_x, pixel_y;
char currentPage = '0';
unsigned long lastTouchTime = 0;

// Settings Variables
int val_Speed = 250;    // Range restricted: 50 to 500
int val_Elevator = -1;  // -1 = Unhomed
int val_Shutter = 30;

// --- STATE FLAGS ---
bool isTableTesting = false;
unsigned long tableTestStartTime = 0;

// Elevator Logic
bool isElevatorHoming = false;
bool isElevatorBacking = false;
bool isElevatorActing = false;

// Camera Logic
bool isCamRefHoming = false;

// Scanning Logic
bool isScanningActive = false;
bool isScanningPaused = false;

// Time Logic
unsigned long scanStartTime = 0;
unsigned long pauseStartTimestamp = 0;
unsigned long totalPausedDuration = 0;
unsigned long scanPausedTime = 0;

// Stats Logic
unsigned long lastShutterTime = 0;
int totalShutterCount = 0;
bool relayState = false;
unsigned long relayTriggerTime = 0;

// =========================================================================
//      HELPER: Enable/Disable
// =========================================================================
void enableMotor(int enPin) {
  digitalWrite(enPin, LOW);
}
void disableMotor(int enPin) {
  digitalWrite(enPin, HIGH);
}

void enableElevators() {
  enableMotor(ELEV1_EN_PIN);
  enableMotor(ELEV2_EN_PIN);
}
void disableElevators() {
  disableMotor(ELEV1_EN_PIN);
  disableMotor(ELEV2_EN_PIN);
}
void enableCam() {
  enableMotor(CAM_EN_PIN);
}
void disableCam() {
  disableMotor(CAM_EN_PIN);
}
void enableTable() {
  enableMotor(TABLE_EN_PIN);
}
void disableTable() {
  disableMotor(TABLE_EN_PIN);
}

// =========================================================================
//      SETUP
// =========================================================================
void setup() {
  Serial.begin(9600);

  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(TABLE_EN_PIN, OUTPUT);
  disableMotor(TABLE_EN_PIN);
  pinMode(ELEV1_EN_PIN, OUTPUT);
  disableMotor(ELEV1_EN_PIN);
  pinMode(ELEV2_EN_PIN, OUTPUT);
  disableMotor(ELEV2_EN_PIN);
  pinMode(CAM_EN_PIN, OUTPUT);
  disableMotor(CAM_EN_PIN);

  pinMode(LIM_ELEV_DOWN_1, INPUT_PULLUP);
  pinMode(LIM_ELEV_DOWN_2, INPUT_PULLUP);
  pinMode(LIM_ELEV_UP_1, INPUT_PULLUP);
  pinMode(LIM_ELEV_UP_2, INPUT_PULLUP);

  // Motor Config
  tableStepper.setMaxSpeed(3000);
  tableStepper.setAcceleration(1000);

  elevStepper1.setMaxSpeed(1000);
  elevStepper1.setAcceleration(400);
  elevStepper2.setMaxSpeed(1000);
  elevStepper2.setAcceleration(400);

  camStepper.setMaxSpeed(CAM_MAX_SPEED);
  camStepper.setAcceleration(CAM_ACCEL);

  drawHomeScreen();
}

// =========================================================================
//      MAIN LOOP
// =========================================================================
void loop() {
  if (isElevatorHoming || isElevatorBacking || isElevatorActing || elevStepper1.distanceToGo() != 0) {
    elevStepper1.run();
    elevStepper2.run();
  }

  if (isCamRefHoming || isScanningActive || camStepper.distanceToGo() != 0) {
    camStepper.run();
  }

  // Table: Constant Speed
  if ((isScanningActive && !isScanningPaused) || isTableTesting) {
    tableStepper.setSpeed(val_Speed);
    tableStepper.runSpeed();
  }

  // ============================
  // LOGIC: TABLE TEST
  // ============================
  if (isTableTesting) {
    if (millis() - tableTestStartTime > 5000) {
      isTableTesting = false;
      tableStepper.setSpeed(0);
      tableStepper.stop();
      disableTable();
      if (currentPage == 'S') drawSetupPage();
    }
  }

  // ============================
  // LOGIC: ELEVATOR HOMING + BACKOFF
  // ============================
  if (isElevatorHoming) {
    if (digitalRead(LIM_ELEV_DOWN_1) == LOW || digitalRead(LIM_ELEV_DOWN_2) == LOW) {
      // Instant Stop
      elevStepper1.setSpeed(0);
      elevStepper1.moveTo(elevStepper1.currentPosition());
      elevStepper2.setSpeed(0);
      elevStepper2.moveTo(elevStepper2.currentPosition());

      elevStepper1.setCurrentPosition(0);
      elevStepper2.setCurrentPosition(0);

      long backoffSteps = -4000;  // REVERSED BACKOFF
      elevStepper1.moveTo(backoffSteps);
      elevStepper2.moveTo(backoffSteps);

      isElevatorHoming = false;
      isElevatorBacking = true;
    }
  }

  if (isElevatorBacking) {
    if (elevStepper1.distanceToGo() == 0 && elevStepper2.distanceToGo() == 0) {
      isElevatorBacking = false;
      elevStepper1.setCurrentPosition(0);
      elevStepper2.setCurrentPosition(0);
      val_Elevator = 0;
      disableElevators();
      if (currentPage == 'S') drawSetupPage();
    }
  }

  // ============================
  // LOGIC: ELEVATOR ACTING (ABSOLUTE)
  // ============================
  if (isElevatorActing) {
    bool hitLimit = false;

    // Ensure limit switches are mapped correctly to the travel direction
    if (elevStepper1.distanceToGo() < 0) {  // Moving UP (Negative direction steps)
      if (digitalRead(LIM_ELEV_UP_1) == LOW || digitalRead(LIM_ELEV_UP_2) == LOW) hitLimit = true;
    } else if (elevStepper1.distanceToGo() > 0) {  // Moving DOWN (Positive direction steps)
      if (digitalRead(LIM_ELEV_DOWN_1) == LOW || digitalRead(LIM_ELEV_DOWN_2) == LOW) hitLimit = true;
    }

    if (hitLimit) {
      elevStepper1.setSpeed(0);
      elevStepper1.moveTo(elevStepper1.currentPosition());
      elevStepper2.setSpeed(0);
      elevStepper2.moveTo(elevStepper2.currentPosition());

      elevStepper1.setCurrentPosition(elevStepper1.currentPosition());
      elevStepper2.setCurrentPosition(elevStepper2.currentPosition());

      val_Elevator = abs(elevStepper1.currentPosition()) / ELEV_STEPS_PER_MM;
      isElevatorActing = false;
      disableElevators();
      if (currentPage == 'S') drawSetupPage();
    } else if (elevStepper1.distanceToGo() == 0 && elevStepper2.distanceToGo() == 0) {
      isElevatorActing = false;
      disableElevators();
      if (currentPage == 'S') drawSetupPage();
    }
  }

  // ============================
  // LOGIC: SCANNING ROUTINE
  // ============================
  if (isScanningActive && !isScanningPaused) {
    // Limits Removed - Relying on manual ABORT/DONE buttons

    // Relay Control
    unsigned long interval = val_Shutter * 100;
    if (millis() - lastShutterTime > interval) {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
      relayTriggerTime = millis();
      lastShutterTime = millis();
      totalShutterCount++;
    }

    // Release Relay
    if (relayState && (millis() - relayTriggerTime > 100)) {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
    }
  }

  // --- TOUCH POLLING ---
  static unsigned long lastTouchSample = 0;
  if (millis() - lastTouchSample > 50) {
    lastTouchSample = millis();
    TSPoint p = ts.getPoint();
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XP, OUTPUT);
    pinMode(YM, OUTPUT);

    if (p.z > 100 && p.z < 1000) {
      if (millis() - lastTouchTime > 200) {
        pixel_x = map(p.y, TS_LEFT, TS_RT, 0, 480);
        pixel_y = map(p.x, TS_TOP, TS_BOT, 0, 320);

        if (currentPage == '0') handleHomeTouch(pixel_x, pixel_y);
        else if (currentPage == 'S') handleSetupTouch(pixel_x, pixel_y);
        else if (currentPage == 'P') handlePreScanTouch(pixel_x, pixel_y);
        else if (currentPage == 'G') handleScanningTouch(pixel_x, pixel_y);
        else if (currentPage == 'R') handleResultTouch(pixel_x, pixel_y);

        lastTouchTime = millis();
      }
    }
  }
}

// =========================================================================
//      UI & DRAWING FUNCTIONS
// =========================================================================

void drawHeader(const char* title, bool showBackBtn = true) {
  tft.fillRect(0, 0, 480, 50, C_PANEL);
  tft.drawFastHLine(0, 50, 480, C_ACCENT);
  tft.drawFastHLine(0, 51, 480, C_ACCENT);

  tft.setTextColor(C_TEXT);
  printCentered(title, 33, &FreeSansBold12pt7b);

  if (showBackBtn) {
    tft.fillTriangle(25, 25, 40, 15, 40, 35, C_ACCENT);
  }
}

void drawFooter() {
  tft.setTextColor(C_TEXT_DIM);
  tft.setFont(&FreeSerif9pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  const char* text = "Developed by Eng. Ahmed Ubayed";
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((480 - w) / 2, 310);
  tft.print(text);
}

void printCentered(const char* text, int y, const GFXfont* font) {
  tft.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((480 - w) / 2, y);
  tft.print(text);
}

void drawButton(int x, int y, int w, int h, const char* label, uint16_t color, bool active = true) {
  uint16_t renderColor = active ? color : C_BTN_DIM;
  uint16_t textColor = active ? C_TEXT : C_TEXT_DIM;

  tft.fillRoundRect(x, y, w, h, 8, renderColor);

  if (active) {
    tft.drawRoundRect(x, y, w, h, 8, C_TEXT);
  } else {
    tft.drawRoundRect(x, y, w, h, 8, C_TEXT_DIM);
  }

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(textColor);
  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + (h + th) / 2 - 4);
  tft.print(label);
}

void drawSmallButton(int x, int y, int w, int h, const char* label, uint16_t color, bool active = true) {
  uint16_t renderColor = active ? color : C_BTN_DIM;
  uint16_t textColor = active ? C_TEXT : C_TEXT_DIM;

  tft.fillRoundRect(x, y, w, h, 6, renderColor);
  if (active) tft.drawRoundRect(x, y, w, h, 6, C_TEXT);

  tft.setFont(&FreeSerif9pt7b);
  tft.setTextColor(textColor);
  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + (h + th) / 2 - 3);
  tft.print(label);
}

void drawControlRow(int y, const char* label, int value, const char* actionLabel = NULL, bool isActive = true) {
  tft.fillRoundRect(5, y - 5, 470, 45, 6, C_PANEL);

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT);
  tft.setCursor(15, y + 24);
  tft.print(label);

  tft.fillRect(150, y, 60, 35, C_BG);
  tft.drawRect(150, y, 60, 35, C_ACCENT);
  tft.setTextColor(C_ACCENT);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(155, y + 26);
  if (value == -1) tft.print("?");
  else tft.print(value);

  // Disable 'Def' if elevator is already fully referenced to 0
  bool defActive = isActive;
  if (strcmp(label, "Elevator") == 0 && value == 0) defActive = false;

  uint16_t btnCol = defActive ? C_BTN : C_BTN_DIM;
  drawSmallButton(220, y, 50, 35, "Def", btnCol, defActive);

  bool canAdjust = (value != -1) && isActive;
  drawSmallButton(280, y, 40, 35, "-", (canAdjust ? C_RED : C_BTN_DIM), canAdjust);
  drawSmallButton(330, y, 40, 35, "+", (canAdjust ? C_GREEN : C_BTN_DIM), canAdjust);

  if (actionLabel != NULL) {
    bool actActive = canAdjust;
    if (isTableTesting && strcmp(actionLabel, "Test") == 0) actActive = true;
    if (isElevatorActing || isTableTesting) actActive = false;
    drawSmallButton(380, y, 70, 35, actionLabel, C_ORANGE, actActive);
  }
}

void refreshSetupValue(int y, int value) {
  tft.fillRect(150, y, 60, 35, C_BG);
  tft.drawRect(150, y, 60, 35, C_ACCENT);
  tft.setTextColor(C_ACCENT);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(155, y + 26);
  if (value == -1) tft.print("?");
  else tft.print(value);
}

void drawHomeScreen() {
  currentPage = '0';
  tft.fillScreen(C_BG);
  drawHeader("3D SCANNER PRO", false);

  drawButton(90, 90, 300, 60, "START SCANNING", C_BTN);
  drawButton(90, 180, 300, 60, "SCANNING SETUP", C_PANEL);
  drawFooter();
}

void handleHomeTouch(int x, int y) {
  if (x > 90 && x < 390) {
    if (y > 90 && y < 150) drawPreScanPage();
    if (y > 180 && y < 240) drawSetupPage();
  }
}

void drawSetupPage() {
  currentPage = 'S';
  tft.fillScreen(C_BG);
  drawHeader("SETUP", true);

  drawControlRow(80, "Speed", val_Speed, "Test", true);
  drawControlRow(140, "Elevator", val_Elevator, "Act", true);
  drawControlRow(200, "Shutter", val_Shutter, NULL, true);
  drawFooter();
}

void handleSetupTouch(int x, int y) {
  if (x < 60 && y < 60) {
    if (isTableTesting || isElevatorHoming || isElevatorActing) return;
    drawHomeScreen();
    return;
  }

  // Row 1: Table Speed (Range: 50 to 500)
  if (y > 80 && y < 115) {
    if (x > 380 && x < 450) {
      if (!isTableTesting) {
        enableTable();
        isTableTesting = true;
        tableTestStartTime = millis();
        drawSetupPage();
      }
      return;
    }
    if (isTableTesting) return;
    if (x > 220 && x < 270) val_Speed = 50;
    if (x > 280 && x < 320 && val_Speed > 50) val_Speed -= 50;
    if (x > 330 && x < 370 && val_Speed < 500) val_Speed += 50;
    refreshSetupValue(80, val_Speed);
  }

  // Row 2: Elevator
  if (y > 140 && y < 175) {
    if (isElevatorHoming || isElevatorActing || isElevatorBacking) return;

    if (x > 220 && x < 270) {         // Def
      if (val_Elevator == 0) return;  // Prevent referencing again if already 0
      enableElevators();
      isElevatorHoming = true;
      elevStepper1.move(200000);  // REVERSED DIRECTION (Moves Down)
      elevStepper2.move(200000);  // REVERSED DIRECTION (Moves Down)
      drawSetupPage();
      return;
    }

    if (val_Elevator != -1) {
      if (x > 280 && x < 320 && val_Elevator > 0) val_Elevator -= 10;
      if (x > 330 && x < 370 && val_Elevator < 800) val_Elevator += 10;

      if (x > 380 && x < 450) {  // Act
        enableElevators();
        isElevatorActing = true;
        // IMPORTANT: If positive move implies down, going UP to absolute position needs NEGATIVE direction
        long targetSteps = -(long)(val_Elevator * ELEV_STEPS_PER_MM);
        elevStepper1.moveTo(targetSteps);
        elevStepper2.moveTo(targetSteps);
        drawSetupPage();
        return;
      }
      refreshSetupValue(140, val_Elevator);
    }
  }

  // Row 3: Shutter
  if (y > 200 && y < 235) {
    if (x > 220 && x < 270) val_Shutter = 30;
    if (x > 280 && x < 320 && val_Shutter > 5) val_Shutter -= 5;
    if (x > 330 && x < 370 && val_Shutter < 100) val_Shutter += 5;
    refreshSetupValue(200, val_Shutter);
  }
}

void drawPreScanPage() {
  currentPage = 'P';
  tft.fillScreen(C_BG);
  drawHeader("PREPARE SCAN", true);

  if (isCamRefHoming) {
    drawButton(90, 100, 300, 60, "ABORT REF", C_RED, true);
    drawButton(90, 200, 300, 60, "GO SCANNING", C_BTN_DIM, false);
  } else {
    drawButton(90, 100, 300, 60, "REFERENCE POINT", C_BTN);
    drawButton(90, 200, 300, 60, "GO SCANNING", C_GREEN);
  }
  drawFooter();
}

void handlePreScanTouch(int x, int y) {
  if (isCamRefHoming) {
    if (x > 90 && x < 390 && y > 100 && y < 160) {
      // Manual Abort Reference
      camStepper.setSpeed(0);
      camStepper.moveTo(camStepper.currentPosition());
      isCamRefHoming = false;
      disableCam();
      drawPreScanPage();
    }
    return;
  }
  if (x < 60 && y < 60) {
    drawHomeScreen();
    return;
  }

  if (x > 90 && x < 390 && y > 100 && y < 160) {
    enableCam();
    isCamRefHoming = true;
    camStepper.move(2000000000);  // REVERSED RELATIVE TO SCANNING
    drawPreScanPage();
  }
  if (x > 90 && x < 390 && y > 200 && y < 260) {
    drawScanningPage();
  }
}

void drawScanningPage() {
  currentPage = 'G';
  tft.fillScreen(C_BG);
  drawHeader("SCANNING IN PROGRESS", false);

  tft.setTextColor(C_TEXT);
  printCentered("Please wait, data is being captured...", 140, &FreeSerif9pt7b);

  tft.drawRoundRect(80, 160, 320, 10, 5, C_PANEL);
  tft.fillRoundRect(80, 160, 160, 10, 5, C_ACCENT);

  drawButton(40, 210, 180, 60, "ABORT", C_RED, true);
  drawButton(260, 210, 180, 60, "DONE", C_BTN_DIM, false);

  // RESET ALL VARIABLES FOR NEW SCAN
  scanStartTime = millis();
  totalPausedDuration = 0;
  scanPausedTime = 0;

  totalShutterCount = 0;
  lastShutterTime = millis();

  isScanningActive = true;
  isScanningPaused = false;

  enableCam();
  enableTable();
  camStepper.move(2000000000);  // MOVES OPPOSITE OF REFERENCING
  drawFooter();
}

void handleScanningTouch(int x, int y) {
  if (x > 40 && x < 220 && y > 210 && y < 270) {
    if (!isScanningPaused) {
      // PAUSE
      isScanningPaused = true;
      pauseStartTimestamp = millis();

      // INSTANT STOP
      camStepper.setSpeed(0);
      camStepper.moveTo(camStepper.currentPosition());
      tableStepper.setSpeed(0);
      tableStepper.stop();

      disableCam();
      disableTable();

      drawButton(40, 210, 180, 60, "CONTINUE", C_GREEN, true);
      drawButton(260, 210, 180, 60, "DONE", C_BTN, true);
    } else {
      // RESUME
      isScanningPaused = false;

      unsigned long thisPauseDuration = millis() - pauseStartTimestamp;
      totalPausedDuration += thisPauseDuration;

      lastShutterTime += thisPauseDuration;

      enableCam();
      enableTable();
      camStepper.move(-2000000000);  // CONTINUES

      drawButton(40, 210, 180, 60, "ABORT", C_RED, true);
      drawButton(260, 210, 180, 60, "DONE", C_BTN_DIM, false);
    }
  }

  if (x > 260 && x < 440 && y > 210 && y < 270) {
    if (isScanningPaused) finishScan();
  }
}

void finishScan() {
  isScanningActive = false;

  // INSTANT STOP
  camStepper.setSpeed(0);
  camStepper.moveTo(camStepper.currentPosition());
  tableStepper.setSpeed(0);
  tableStepper.stop();

  disableCam();
  disableTable();

  drawResultPage();
}

void drawResultPage() {
  currentPage = 'R';
  tft.fillScreen(C_BG);
  drawHeader("SCAN COMPLETED", false);

  // FINAL CALCULATION
  unsigned long totalRunMillis = millis() - scanStartTime - totalPausedDuration;
  unsigned long totalSeconds = totalRunMillis / 1000;
  int h = totalSeconds / 3600;
  int m = (totalSeconds % 3600) / 60;
  int s = totalSeconds % 60;

  char finalTimeStr[10];
  sprintf(finalTimeStr, "%02d:%02d:%02d", h, m, s);

  // Central Modern Data Card
  tft.fillRoundRect(60, 75, 360, 100, 8, C_PANEL);
  tft.drawRoundRect(60, 75, 360, 100, 8, C_BTN_DIM);

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(90, 110);
  tft.print("Total Time:");
  tft.setCursor(90, 150);
  tft.print("Shutters:");

  tft.setTextColor(C_ACCENT);
  tft.setCursor(240, 110);
  tft.print(finalTimeStr);
  tft.setCursor(240, 150);
  tft.print(totalShutterCount);

  drawButton(40, 200, 180, 60, "SCAN SETUP", C_PANEL);
  drawButton(260, 200, 180, 60, "HOME", C_BTN);
  drawFooter();
}

void handleResultTouch(int x, int y) {
  if (y > 200 && y < 260) {
    if (x > 40 && x < 220) drawPreScanPage();
    if (x > 260 && x < 440) drawHomeScreen();
  }
}
