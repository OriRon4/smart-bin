#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================================================
// Smart Bin - Main ESP32
// Servos + Ultrasonic + Camera UART + OLED + Passive Buzzer
// =========================================================


// =========================================================
// Servo Pins
// =========================================================
const int TOP_SERVO_PIN = 18;
const int BOTTOM_SERVO_PIN = 19;


// =========================================================
// Ultrasonic Pins
// =========================================================
const int ULTRASONIC_TRIG_PIN = 26;
const int ULTRASONIC_ECHO_PIN = 27;


// =========================================================
// Camera UART Pins
// Main ESP32 RX מקבל מה-TX של המצלמה
// Main ESP32 TX שולח ל-RX של המצלמה
// =========================================================
const int CAMERA_RX_PIN = 16;
const int CAMERA_TX_PIN = 17;


// =========================================================
// Buzzer Pin
// Passive Buzzer
// =========================================================
const int BUZZER_PIN = 25;


// =========================================================
// OLED Settings
// =========================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;


// =========================================================
// Servo Angles
// =========================================================
const int BOTTOM_CENTER_ANGLE = 100;
const int PLASTIC_ANGLE = 40;
const int PAPER_ANGLE = 100;
const int METAL_ANGLE = 150;

const int TOP_CLOSED_ANGLE = 26;
const int TOP_OPEN_ANGLE = 0;


// =========================================================
// Timing
// =========================================================
const unsigned long WAIT_BEFORE_START_MS = 3000;
const unsigned long WAIT_AFTER_BOTTOM_MOVE_MS = 2000;
const unsigned long WAIT_WHILE_TOP_OPEN_MS = 3000;
const unsigned long WAIT_AFTER_TOP_CLOSE_MS = 3000;
const unsigned long SERVO_SMALL_DELAY_MS = 500;

const unsigned long OBJECT_SETTLE_DELAY_MS = 1000;
const unsigned long DETECTION_COOLDOWN_MS = 8000;

const unsigned long CAMERA_RESPONSE_TIMEOUT_MS = 30000;


// =========================================================
// Ultrasonic Detection
// אם המרחק קטן או שווה לערך הזה, נחשב שיש חפץ
// אפשר לשנות דרך Serial:
// threshold 7.5
// =========================================================
float OBJECT_DETECTION_THRESHOLD_CM = 8;


// =========================================================
// System State
// =========================================================
bool autoModeEnabled = false;
unsigned long lastDetectionTime = 0;


// =========================================================
// Servo Objects
// =========================================================
Servo bottomServo;
Servo topServo;


// =========================================================
// Buzzer Functions
// =========================================================
void initializeBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Serial.print("[INIT] Buzzer initialized on GPIO ");
  Serial.println(BUZZER_PIN);
}


void beepShort() {
  tone(BUZZER_PIN, 1000);
  delay(120);
  noTone(BUZZER_PIN);
}


void beepDouble() {
  beepShort();
  delay(120);
  beepShort();
}


void beepLong() {
  tone(BUZZER_PIN, 700);
  delay(500);
  noTone(BUZZER_PIN);
}


void beepSuccess() {
  tone(BUZZER_PIN, 1200);
  delay(120);
  noTone(BUZZER_PIN);

  delay(100);

  tone(BUZZER_PIN, 1600);
  delay(160);
  noTone(BUZZER_PIN);
}


// =========================================================
// OLED Functions
// =========================================================
void initializeOled() {
  Serial.println("[INIT] Initializing OLED...");

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[INIT] OLED not found at 0x3C.");
    oledReady = false;
    return;
  }

  oledReady = true;
  Serial.println("[INIT] OLED initialized.");

  display.clearDisplay();
  display.display();
}


void showOledMessage(String line1, String line2 = "", String line3 = "", String line4 = "") {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART BIN");

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.println(line1);

  if (line2.length() > 0) {
    display.setCursor(0, 30);
    display.println(line2);
  }

  if (line3.length() > 0) {
    display.setCursor(0, 42);
    display.println(line3);
  }

  if (line4.length() > 0) {
    display.setCursor(0, 54);
    display.println(line4);
  }

  display.display();
}


void showReadyScreen() {
  showOledMessage("READY", "Waiting object", autoModeEnabled ? "Auto: ON" : "Auto: OFF");
}


void showCategoryScreen(String category) {
  category.toUpperCase();
  showOledMessage("CATEGORY:", category, "Sorting...");
}


void showUnknownScreen() {
  showOledMessage("UNKNOWN", "Sorting cancelled");
}


void showDoneScreen() {
  showOledMessage("DONE", "Ready again");
}


// =========================================================
// setup
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("Smart Bin - Main ESP32");
  Serial.println("Servos + Ultrasonic + Camera UART + OLED + Buzzer");
  Serial.println("==========================================");

  initializeServos();
  initializeUltrasonic();
  initializeCameraCommunication();
  initializeBuzzer();
  initializeOled();

  moveToReadyPosition();

  beepSuccess();
  showReadyScreen();

  printHelp();
}


// =========================================================
// loop
// =========================================================
void loop() {
  handleSerialCommands();

  if (autoModeEnabled) {
    checkUltrasonicAndHandleObject();
  }

  delay(100);
}


// =========================================================
// Initialize Servos
// =========================================================
void initializeServos() {
  Serial.println("[INIT] Initializing servos...");

  bottomServo.attach(BOTTOM_SERVO_PIN);
  topServo.attach(TOP_SERVO_PIN);

  delay(500);

  Serial.println("[INIT] Servos initialized.");
}


// =========================================================
// Initialize Ultrasonic
// =========================================================
void initializeUltrasonic() {
  Serial.println("[INIT] Initializing ultrasonic...");

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  Serial.print("[INIT] TRIG pin = ");
  Serial.println(ULTRASONIC_TRIG_PIN);

  Serial.print("[INIT] ECHO pin = ");
  Serial.println(ULTRASONIC_ECHO_PIN);

  Serial.println("[INIT] Ultrasonic initialized.");
}


// =========================================================
// Initialize Camera UART Communication
// =========================================================
void initializeCameraCommunication() {
  Serial.println("[INIT] Initializing camera UART communication...");

  Serial2.begin(115200, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN);

  Serial.print("[INIT] CAMERA_RX_PIN = ");
  Serial.println(CAMERA_RX_PIN);

  Serial.print("[INIT] CAMERA_TX_PIN = ");
  Serial.println(CAMERA_TX_PIN);

  Serial.println("[INIT] Camera UART initialized at 115200 baud.");
}


// =========================================================
// Clear Camera Serial Buffer
// מנקה תשובות ישנות מהמצלמה לפני שליחת פקודה חדשה
// =========================================================
void clearCameraBuffer() {
  while (Serial2.available()) {
    Serial2.read();
  }
}


// =========================================================
// Send Command To Camera
// שולח פקודה ל-ESP32-CAM
// =========================================================
void sendCommandToCamera(String command) {
  clearCameraBuffer();

  Serial.print("[CAMERA UART] Sending command: ");
  Serial.println(command);

  Serial2.println(command);
}


// =========================================================
// Read Camera Response
// קורא תשובה תקינה מה-ESP32-CAM
// =========================================================
String readCameraResponse() {
  Serial.println("[CAMERA UART] Waiting for camera response...");

  unsigned long startTime = millis();

  while (millis() - startTime < CAMERA_RESPONSE_TIMEOUT_MS) {
    if (Serial2.available()) {
      String response = Serial2.readStringUntil('\n');
      response.trim();

      if (response.length() == 0) {
        continue;
      }

      Serial.print("[CAMERA UART] Raw response: ");
      Serial.println(response);

      if (response == "PONG") {
        Serial.println("[CAMERA UART] Valid PONG received.");
        return response;
      }

      if (response.startsWith("{")) {
        Serial.println("[CAMERA UART] Valid JSON received.");
        return response;
      }

      Serial.println("[CAMERA UART] Ignoring noisy response...");
    }

    delay(20);
  }

  Serial.println("[CAMERA UART] Timeout. No valid response from camera.");
  return "";
}


// =========================================================
// Extract Value From Simple JSON
// מוציא ערך טקסטואלי מתוך JSON פשוט בלי ספרייה חיצונית
// עובד גם אם יש רווחים:
// "category":"metal"
// "category": "metal"
// =========================================================
String extractStringValue(String jsonText, String key) {
  String keyPattern = "\"" + key + "\"";

  int keyIndex = jsonText.indexOf(keyPattern);

  if (keyIndex == -1) {
    return "";
  }

  int colonIndex = jsonText.indexOf(":", keyIndex);

  if (colonIndex == -1) {
    return "";
  }

  int firstQuote = jsonText.indexOf("\"", colonIndex);

  if (firstQuote == -1) {
    return "";
  }

  int secondQuote = jsonText.indexOf("\"", firstQuote + 1);

  if (secondQuote == -1) {
    return "";
  }

  return jsonText.substring(firstQuote + 1, secondQuote);
}


// =========================================================
// Extract Category From Camera JSON
// מוציא את category מהתשובה של המצלמה
// =========================================================
String extractCategoryFromCameraJson(String jsonText) {
  String category = extractStringValue(jsonText, "category");

  category.trim();
  category.toLowerCase();

  Serial.println();
  Serial.println("========== CAMERA JSON PARSED ==========");
  Serial.print("category = ");
  Serial.println(category);
  Serial.println("========================================");

  if (category == "plastic" || category == "paper" || category == "metal") {
    return category;
  }

  return "unknown";
}


// =========================================================
// Test Camera Ping
// שולח PING ומצפה לקבל PONG
// =========================================================
void testCameraPing() {
  Serial.println();
  Serial.println("========== CAMERA PING TEST ==========");

  showOledMessage("CAMERA TEST", "Sending PING...");
  beepShort();

  sendCommandToCamera("PING");

  String response = readCameraResponse();

  if (response == "PONG") {
    Serial.println("[CAMERA UART] PING test successful.");
    showOledMessage("CAMERA OK", "PONG received");
    beepSuccess();
  } else if (response.length() == 0) {
    Serial.println("[CAMERA UART] No response. Check wiring or ESP32-CAM code.");
    showOledMessage("CAMERA ERROR", "No response");
    beepLong();
  } else {
    Serial.println("[CAMERA UART] Unexpected response.");
    showOledMessage("CAMERA ERROR", "Unexpected data");
    beepLong();
  }

  Serial.println("======================================");

  delay(1200);
  showReadyScreen();
}


// =========================================================
// Request Capture From Camera
// שולח CAPTURE למצלמה, מקבל JSON, ומפעיל מיון לפי category
// =========================================================
void requestCaptureFromCamera() {
  Serial.println();
  Serial.println("========== CAMERA CAPTURE TEST ==========");

  showOledMessage("CAPTURING", "Please wait...");
  beepShort();

  sendCommandToCamera("CAPTURE");

  String response = readCameraResponse();

  if (response.length() == 0) {
    Serial.println("[CAMERA UART] Capture failed. No response from camera.");
    showOledMessage("CAPTURE FAILED", "No camera response");
    beepLong();
    Serial.println("=========================================");
    delay(1500);
    showReadyScreen();
    return;
  }

  Serial.println("[CAMERA UART] Camera returned:");
  Serial.println(response);

  String category = extractCategoryFromCameraJson(response);

  Serial.print("[CAMERA UART] Final category = ");
  Serial.println(category);

  if (category == "unknown") {
    showUnknownScreen();
    beepLong();

    Serial.println("[CAMERA UART] Unknown category. Sorting cancelled.");
    moveToReadyPosition();

    Serial.println("=========================================");
    delay(1500);
    showReadyScreen();
    return;
  }

  showCategoryScreen(category);
  beepDouble();

  sortToCategory(category);

  Serial.println("=========================================");
}


// =========================================================
// Read Distance In CM
// מחזיר מרחק בס"מ
// אם אין קריאה תקינה, מחזיר -1
// =========================================================
float readDistanceCm() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  float distanceCm = duration * 0.0343 / 2.0;
  return distanceCm;
}


// =========================================================
// Is Object Detected
// =========================================================
bool isObjectDetected() {
  float distance = readDistanceCm();

  if (distance < 0) {
    return false;
  }

  return distance <= OBJECT_DETECTION_THRESHOLD_CM;
}


// =========================================================
// Check Ultrasonic And Handle Object
// במצב אוטומטי, בודק אם יש חפץ
// =========================================================
void checkUltrasonicAndHandleObject() {
  unsigned long now = millis();

  if (now - lastDetectionTime < DETECTION_COOLDOWN_MS) {
    return;
  }

  float distance = readDistanceCm();

  if (distance < 0) {
    return;
  }

  if (distance <= OBJECT_DETECTION_THRESHOLD_CM) {
    Serial.println();
    Serial.println("[AUTO] Object detected!");
    Serial.print("[AUTO] Distance = ");
    Serial.print(distance);
    Serial.println(" cm");

    showOledMessage("OBJECT DETECTED", "Distance:", String(distance) + " cm");
    beepShort();

    lastDetectionTime = now;

    handleObjectDetected();
  }
}


// =========================================================
// Handle Object Detected
// כשיש חפץ, מחכים רגע ואז מבקשים מהמצלמה category
// =========================================================
void handleObjectDetected() {
  Serial.println("[PROCESS] Object handling started.");
  Serial.print("[PROCESS] Waiting for object to settle: ");
  Serial.print(OBJECT_SETTLE_DELAY_MS);
  Serial.println(" ms");

  showOledMessage("OBJECT DETECTED", "Stabilizing...");
  delay(OBJECT_SETTLE_DELAY_MS);

  requestCaptureFromCamera();

  Serial.println("[PROCESS] Object handling finished.");
}


// =========================================================
// Move To Ready Position
// =========================================================
void moveToReadyPosition() {
  Serial.println();
  Serial.println("[READY] Moving to ready position...");

  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  bottomServo.write(BOTTOM_CENTER_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.print("[READY] Top closed angle = ");
  Serial.println(TOP_CLOSED_ANGLE);

  Serial.print("[READY] Bottom center angle = ");
  Serial.println(BOTTOM_CENTER_ANGLE);
}


// =========================================================
// Move Bottom Servo Manually
// =========================================================
void moveBottomServoTo(int angle) {
  angle = constrain(angle, 0, 180);

  Serial.print("[MANUAL] Bottom servo -> ");
  Serial.println(angle);

  bottomServo.write(angle);
  delay(SERVO_SMALL_DELAY_MS);
}


// =========================================================
// Move Top Servo Manually
// =========================================================
void moveTopServoTo(int angle) {
  angle = constrain(angle, 0, 180);

  Serial.print("[MANUAL] Top servo -> ");
  Serial.println(angle);

  topServo.write(angle);
  delay(SERVO_SMALL_DELAY_MS);
}


// =========================================================
// Open Top Door
// =========================================================
void openTopDoor() {
  Serial.print("[DOOR] Opening top door -> ");
  Serial.println(TOP_OPEN_ANGLE);

  topServo.write(TOP_OPEN_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);
}


// =========================================================
// Close Top Door
// =========================================================
void closeTopDoor() {
  Serial.print("[DOOR] Closing top door -> ");
  Serial.println(TOP_CLOSED_ANGLE);

  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);
}


// =========================================================
// Get Bottom Angle By Category
// =========================================================
int getBottomAngleForCategory(String category) {
  category.trim();
  category.toLowerCase();

  if (category == "plastic") {
    return PLASTIC_ANGLE;
  }

  if (category == "paper") {
    return PAPER_ANGLE;
  }

  if (category == "metal") {
    return METAL_ANGLE;
  }

  return BOTTOM_CENTER_ANGLE;
}


// =========================================================
// Sort To Category
// =========================================================
void sortToCategory(String category) {
  category.trim();
  category.toLowerCase();

  Serial.println();
  Serial.println("========== SORTING STARTED ==========");
  Serial.print("[SORT] Category = ");
  Serial.println(category);

  if (category != "plastic" && category != "paper" && category != "metal") {
    Serial.println("[SORT] Unknown category. Sorting cancelled.");
    showUnknownScreen();
    beepLong();
    moveToReadyPosition();
    Serial.println("========== SORTING CANCELLED ==========");
    delay(1500);
    showReadyScreen();
    return;
  }

  int targetBottomAngle = getBottomAngleForCategory(category);

  Serial.print("[SORT] Target bottom angle = ");
  Serial.println(targetBottomAngle);

  showOledMessage("SORTING", "Category:", category);

  Serial.println("[SORT] Making sure top door is closed...");
  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.print("[SORT] Waiting before start: ");
  Serial.print(WAIT_BEFORE_START_MS);
  Serial.println(" ms");
  delay(WAIT_BEFORE_START_MS);

  Serial.println("[SORT] Moving bottom servo to target bin...");
  showOledMessage("SORTING", "Moving gate...", category);
  bottomServo.write(targetBottomAngle);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.print("[SORT] Waiting after bottom movement: ");
  Serial.print(WAIT_AFTER_BOTTOM_MOVE_MS);
  Serial.println(" ms");
  delay(WAIT_AFTER_BOTTOM_MOVE_MS);

  Serial.println("[SORT] Opening top door...");
  showOledMessage("SORTING", "Opening door...", category);
  topServo.write(TOP_OPEN_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.print("[SORT] Top door stays open: ");
  Serial.print(WAIT_WHILE_TOP_OPEN_MS);
  Serial.println(" ms");
  delay(WAIT_WHILE_TOP_OPEN_MS);

  Serial.println("[SORT] Closing top door...");
  showOledMessage("SORTING", "Closing door...", category);
  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.print("[SORT] Waiting after top door close: ");
  Serial.print(WAIT_AFTER_TOP_CLOSE_MS);
  Serial.println(" ms");
  delay(WAIT_AFTER_TOP_CLOSE_MS);

  Serial.println("[SORT] Returning bottom servo to center...");
  showOledMessage("SORTING", "Returning center...");
  bottomServo.write(BOTTOM_CENTER_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  Serial.println("========== SORTING FINISHED ==========");
  Serial.println("[SYSTEM] Ready for next object.");

  showDoneScreen();
  beepSuccess();

  delay(1500);
  showReadyScreen();
}


// =========================================================
// Print Current Distance
// =========================================================
void printDistance() {
  float distance = readDistanceCm();

  if (distance < 0) {
    Serial.println("[DISTANCE] No valid reading.");
    showOledMessage("DISTANCE", "No valid reading");
    return;
  }

  Serial.print("[DISTANCE] ");
  Serial.print(distance);
  Serial.print(" cm");

  if (distance <= OBJECT_DETECTION_THRESHOLD_CM) {
    Serial.print(" -> OBJECT DETECTED");
    showOledMessage("DISTANCE", String(distance) + " cm", "OBJECT DETECTED");
  } else {
    showOledMessage("DISTANCE", String(distance) + " cm", "No object");
  }

  Serial.println();
}


// =========================================================
// Handle Serial Commands
// =========================================================
void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  Serial.println();
  Serial.print("[SERIAL] Command received: ");
  Serial.println(command);

  if (command == "help") {
    printHelp();
    return;
  }

  if (command == "status") {
    printStatus();
    return;
  }

  if (command == "ready") {
    moveToReadyPosition();
    showReadyScreen();
    return;
  }

  if (command == "center") {
    moveBottomServoTo(BOTTOM_CENTER_ANGLE);
    showOledMessage("MANUAL", "Bottom center");
    return;
  }

  if (command == "open") {
    openTopDoor();
    showOledMessage("MANUAL", "Door open");
    return;
  }

  if (command == "close") {
    closeTopDoor();
    showOledMessage("MANUAL", "Door closed");
    return;
  }

  if (command == "plastic") {
    sortToCategory("plastic");
    return;
  }

  if (command == "paper") {
    sortToCategory("paper");
    return;
  }

  if (command == "metal") {
    sortToCategory("metal");
    return;
  }

  if (command == "unknown") {
    Serial.println("[MANUAL] Unknown category. No sorting.");
    showUnknownScreen();
    beepLong();
    moveToReadyPosition();
    delay(1500);
    showReadyScreen();
    return;
  }

  if (command == "run") {
    sortToCategory("plastic");
    return;
  }

  if (command == "distance") {
    printDistance();
    return;
  }

  if (command == "ping") {
    testCameraPing();
    return;
  }

  if (command == "capture") {
    requestCaptureFromCamera();
    return;
  }

  if (command == "auto on") {
    autoModeEnabled = true;
    Serial.println("[AUTO] Auto mode enabled.");
    showReadyScreen();
    beepSuccess();
    return;
  }

  if (command == "auto off") {
    autoModeEnabled = false;
    Serial.println("[AUTO] Auto mode disabled.");
    showReadyScreen();
    beepLong();
    return;
  }

  if (command == "beep") {
    beepShort();
    return;
  }

  if (command == "beep double") {
    beepDouble();
    return;
  }

  if (command == "beep long") {
    beepLong();
    return;
  }

  if (command == "beep success") {
    beepSuccess();
    return;
  }

  if (command == "oled") {
    showOledMessage("OLED TEST", "Display working", "Smart Bin");
    return;
  }

  if (command.startsWith("threshold ")) {
    float newThreshold = command.substring(10).toFloat();

    if (newThreshold <= 0) {
      Serial.println("[ERROR] Invalid threshold.");
      showOledMessage("ERROR", "Invalid threshold");
      beepLong();
      return;
    }

    OBJECT_DETECTION_THRESHOLD_CM = newThreshold;

    Serial.print("[ULTRASONIC] New threshold = ");
    Serial.print(OBJECT_DETECTION_THRESHOLD_CM);
    Serial.println(" cm");

    showOledMessage("THRESHOLD SET", String(OBJECT_DETECTION_THRESHOLD_CM) + " cm");
    beepShort();
    return;
  }

  if (command.startsWith("b ")) {
    int angle = command.substring(2).toInt();
    moveBottomServoTo(angle);
    showOledMessage("MANUAL", "Bottom:", String(angle));
    return;
  }

  if (command.startsWith("t ")) {
    int angle = command.substring(2).toInt();
    moveTopServoTo(angle);
    showOledMessage("MANUAL", "Top:", String(angle));
    return;
  }

  Serial.print("[ERROR] Unknown command: ");
  Serial.println(command);
  Serial.println("Type 'help' to see available commands.");

  showOledMessage("ERROR", "Unknown command");
  beepLong();
}


// =========================================================
// Print Status
// =========================================================
void printStatus() {
  Serial.println();
  Serial.println("========== STATUS ==========");

  Serial.println("Servo pins:");
  Serial.print("TOP_SERVO_PIN = ");
  Serial.println(TOP_SERVO_PIN);

  Serial.print("BOTTOM_SERVO_PIN = ");
  Serial.println(BOTTOM_SERVO_PIN);

  Serial.println();

  Serial.println("Ultrasonic pins:");
  Serial.print("ULTRASONIC_TRIG_PIN = ");
  Serial.println(ULTRASONIC_TRIG_PIN);

  Serial.print("ULTRASONIC_ECHO_PIN = ");
  Serial.println(ULTRASONIC_ECHO_PIN);

  Serial.println();

  Serial.println("Camera UART pins:");
  Serial.print("CAMERA_RX_PIN = ");
  Serial.println(CAMERA_RX_PIN);

  Serial.print("CAMERA_TX_PIN = ");
  Serial.println(CAMERA_TX_PIN);

  Serial.println();

  Serial.println("OLED:");
  Serial.print("OLED ready = ");
  Serial.println(oledReady ? "YES" : "NO");
  Serial.print("OLED SDA = ");
  Serial.println(21);
  Serial.print("OLED SCL = ");
  Serial.println(22);

  Serial.println();

  Serial.println("Buzzer:");
  Serial.print("BUZZER_PIN = ");
  Serial.println(BUZZER_PIN);

  Serial.println();

  Serial.println("Angles:");
  Serial.print("BOTTOM_CENTER_ANGLE = ");
  Serial.println(BOTTOM_CENTER_ANGLE);

  Serial.print("PLASTIC_ANGLE = ");
  Serial.println(PLASTIC_ANGLE);

  Serial.print("PAPER_ANGLE = ");
  Serial.println(PAPER_ANGLE);

  Serial.print("METAL_ANGLE = ");
  Serial.println(METAL_ANGLE);

  Serial.print("TOP_CLOSED_ANGLE = ");
  Serial.println(TOP_CLOSED_ANGLE);

  Serial.print("TOP_OPEN_ANGLE = ");
  Serial.println(TOP_OPEN_ANGLE);

  Serial.println();

  Serial.println("Ultrasonic:");
  Serial.print("Auto mode = ");
  Serial.println(autoModeEnabled ? "ON" : "OFF");

  Serial.print("OBJECT_DETECTION_THRESHOLD_CM = ");
  Serial.println(OBJECT_DETECTION_THRESHOLD_CM);

  printDistance();

  Serial.println("============================");
}


// =========================================================
// Print Help
// =========================================================
void printHelp() {
  Serial.println();
  Serial.println("========== HELP ==========");

  Serial.println("Manual sorting:");
  Serial.println("  plastic       -> sort as plastic");
  Serial.println("  paper         -> sort as paper");
  Serial.println("  metal         -> sort as metal");
  Serial.println("  unknown       -> no sorting, return to ready");
  Serial.println("  run           -> test plastic sorting");
  Serial.println();

  Serial.println("Servo manual control:");
  Serial.println("  b 40          -> move bottom servo to angle 40");
  Serial.println("  t 26          -> move top servo to angle 26");
  Serial.println("  center        -> move bottom servo to center");
  Serial.println("  open          -> open top door");
  Serial.println("  close         -> close top door");
  Serial.println();

  Serial.println("Ultrasonic:");
  Serial.println("  distance      -> read current distance");
  Serial.println("  auto on       -> enable automatic detection");
  Serial.println("  auto off      -> disable automatic detection");
  Serial.println("  threshold 7.5 -> set object detection threshold");
  Serial.println();

  Serial.println("Camera UART:");
  Serial.println("  ping          -> send PING to ESP32-CAM and wait for PONG");
  Serial.println("  capture       -> send CAPTURE to ESP32-CAM and wait for JSON response");
  Serial.println();

  Serial.println("OLED:");
  Serial.println("  oled          -> test OLED message");
  Serial.println();

  Serial.println("Buzzer:");
  Serial.println("  beep          -> short beep");
  Serial.println("  beep double   -> double beep");
  Serial.println("  beep long     -> long error beep");
  Serial.println("  beep success  -> success sound");
  Serial.println();

  Serial.println("System:");
  Serial.println("  ready         -> move both servos to ready position");
  Serial.println("  status        -> print current settings");
  Serial.println("  help          -> print this help menu");

  Serial.println("==========================");
  Serial.println();
}