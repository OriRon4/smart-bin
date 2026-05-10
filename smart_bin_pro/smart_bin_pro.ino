#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


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
const int OLED_SDA_PIN = 21;
const int OLED_SCL_PIN = 22;

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
const unsigned long WAIT_AFTER_BOTTOM_MOVE_MS = 1500;
const unsigned long WAIT_WHILE_TOP_OPEN_MS = 2000;
const unsigned long WAIT_AFTER_TOP_CLOSE_MS = 1500;
const unsigned long SERVO_SMALL_DELAY_MS = 500;

const unsigned long OBJECT_SETTLE_DELAY_MS = 1500;
const unsigned long DETECTION_COOLDOWN_MS = 10000;

const unsigned long CAMERA_RESPONSE_TIMEOUT_MS = 30000;


// =========================================================
// Ultrasonic Detection
// אם המרחק קטן או שווה לערך הזה, נחשב שיש חפץ
// =========================================================
float OBJECT_DETECTION_THRESHOLD_CM = 8;
const int ULTRASONIC_CONFIRM_SAMPLES = 7;
const unsigned long ULTRASONIC_SAMPLE_DELAY_MS = 60;


// =========================================================
// System State
// =========================================================
bool autoModeEnabled = true;       //if needed to be auto without writing, change to true
unsigned long lastDetectionTime = 0;


// =========================================================
// Servo Objects
// =========================================================
Servo bottomServo;
Servo topServo;


// =========================================================
// setup
// =========================================================
void setup() {
  delay(1000);

  initializeServos();
  initializeUltrasonic();
  initializeCameraCommunication();
  initializeBuzzer();
  initializeOled();

  moveToReadyPosition();

  beepSuccess();
  showReadyScreen();
}


// =========================================================
// loop
// =========================================================
void loop() {
  if (autoModeEnabled) {
    checkUltrasonicAndHandleObject();
  }

  delay(100);
}


// =========================================================
// Initialize Servos
// =========================================================
void initializeServos() {
  bottomServo.attach(BOTTOM_SERVO_PIN);
  topServo.attach(TOP_SERVO_PIN);

  delay(500);
}


// =========================================================
// Initialize Ultrasonic
// =========================================================
void initializeUltrasonic() {
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
}


// =========================================================
// Initialize Camera UART Communication
// =========================================================
void initializeCameraCommunication() {
  Serial2.begin(115200, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN);
}


// =========================================================
// OLED Functions
// =========================================================
void initializeOled() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledReady = false;
    return;
  }

  oledReady = true;

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
  showOledMessage("READY", "Waiting for object", autoModeEnabled ? "Auto: ON" : "Auto: OFF");
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
// Buzzer Functions
// =========================================================
void initializeBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
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
// Read Average Distance In CM
// ממוצע קריאות תקינות בלבד
// =========================================================
float readAverageDistanceCm() {
  float totalDistance = 0;
  int validReadings = 0;

  for (int i = 0; i < ULTRASONIC_CONFIRM_SAMPLES; i++) {
    float distance = readDistanceCm();

    if (distance >= 0) {
      totalDistance += distance;
      validReadings++;
    }

    if (i < ULTRASONIC_CONFIRM_SAMPLES - 1) {
      delay(ULTRASONIC_SAMPLE_DELAY_MS);
    }
  }

  if (validReadings == 0) {
    return -1;
  }

  return totalDistance / validReadings;
}


// =========================================================
// Confirm Object By Average Distance
// בודק ממוצע לפני צילום
// =========================================================
bool confirmObjectByAverageDistance() {
  float averageDistance = readAverageDistanceCm();

  if (averageDistance < 0) {
    showOledMessage("ULTRASONIC ERROR", "No valid reading");
    beepLong();
    return false;
  }

  if (averageDistance > OBJECT_DETECTION_THRESHOLD_CM) {
    showOledMessage("NO OBJECT", "Average:", String(averageDistance) + " cm");
    beepLong();
    return false;
  }

  return true;
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

  Serial2.println(command);
}


// =========================================================
// Read Camera Response
// קורא תשובה תקינה מה-ESP32-CAM
// =========================================================
String readCameraResponse() {
  unsigned long startTime = millis();

  while (millis() - startTime < CAMERA_RESPONSE_TIMEOUT_MS) {
    if (Serial2.available()) {
      String response = Serial2.readStringUntil('\n');
      response.trim();

      if (response.length() == 0) {
        continue;
      }

      if (response == "PONG") {
        return response;
      }

      if (response.startsWith("{")) {
        return response;
      }
    }

    delay(20);
  }

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


bool cameraResponseFailed(String jsonText) {
  jsonText.toLowerCase();
  jsonText.replace(" ", "");

  return jsonText.indexOf("\"success\":false") != -1;
}


// =========================================================
// Extract Category From Camera JSON
// מוציא את category מהתשובה של המצלמה
// =========================================================
String extractCategoryFromCameraJson(String jsonText) {
  String category = extractStringValue(jsonText, "category");

  category.trim();
  category.toLowerCase();

  if (category == "plastic" || category == "paper" || category == "metal") {
    return category;
  }

  return "unknown";
}


// =========================================================
// Request Capture From Camera
// שולח CAPTURE למצלמה, מקבל JSON, ומפעיל מיון לפי category
// =========================================================
void requestCaptureFromCamera(bool requireUltrasonicConfirmation) {
  if (requireUltrasonicConfirmation && !confirmObjectByAverageDistance()) {
    returnToReadyState(1200);
    return;
  }

  showOledMessage("CAPTURING", "Please wait...");
  beepShort();

  sendCommandToCamera("CAPTURE");

  String response = readCameraResponse();

  if (response.length() == 0) {
    showOledMessage("CAPTURE FAILED", "No camera response");
    beepLong();
    returnToReadyState(1500);
    return;
  }

  if (!response.startsWith("{")) {
    showOledMessage("CAPTURE FAILED", "Invalid response");
    beepLong();
    returnToReadyState(1500);
    return;
  }

  if (cameraResponseFailed(response)) {
    showOledMessage("SERVER FAILED", "Try again");
    beepLong();
    returnToReadyState(1500);
    return;
  }

  String category = extractCategoryFromCameraJson(response);

  if (category == "unknown") {
    showUnknownScreen();
    beepLong();

    returnToReadyState(1500);
    return;
  }

  showCategoryScreen(category);
  beepDouble();

  sortToCategory(category);
}


// =========================================================
// Move To Ready Position
// =========================================================
void moveToReadyPosition() {
  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  bottomServo.write(BOTTOM_CENTER_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);
}


// =========================================================
// Return To Ready State
// חזרה למצב מוכן
// =========================================================
void returnToReadyState(unsigned long waitBeforeReadyScreenMs) {
  moveToReadyPosition();

  if (waitBeforeReadyScreenMs > 0) {
    delay(waitBeforeReadyScreenMs);
  }

  showReadyScreen();
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

  if (category != "plastic" && category != "paper" && category != "metal") {
    showUnknownScreen();
    beepLong();
    returnToReadyState(1500);
    return;
  }

  int targetBottomAngle = getBottomAngleForCategory(category);

  showOledMessage("SORTING", "Category:", category);

  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  delay(WAIT_BEFORE_START_MS);

  showOledMessage("SORTING", "Moving gate...", category);
  bottomServo.write(targetBottomAngle);
  delay(SERVO_SMALL_DELAY_MS);

  delay(WAIT_AFTER_BOTTOM_MOVE_MS);

  showOledMessage("SORTING", "Opening door...", category);
  topServo.write(TOP_OPEN_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  delay(WAIT_WHILE_TOP_OPEN_MS);

  showOledMessage("SORTING", "Closing door...", category);
  topServo.write(TOP_CLOSED_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  delay(WAIT_AFTER_TOP_CLOSE_MS);

  showOledMessage("SORTING", "Returning center...");
  bottomServo.write(BOTTOM_CENTER_ANGLE);
  delay(SERVO_SMALL_DELAY_MS);

  showDoneScreen();
  beepSuccess();

  delay(1500);
  showReadyScreen();
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
    showOledMessage("OBJECT DETECTED", "Distance:", String(distance) + " cm");
    beepShort();

    lastDetectionTime = now;

    handleObjectDetected();

    lastDetectionTime = millis();
  }
}


// =========================================================
// Handle Object Detected
// כשיש חפץ, מחכים רגע ואז מבקשים מהמצלמה category
// =========================================================
void handleObjectDetected() {
  showOledMessage("OBJECT DETECTED", "Stabilizing...");
  delay(OBJECT_SETTLE_DELAY_MS);

  requestCaptureFromCamera(true);
}
