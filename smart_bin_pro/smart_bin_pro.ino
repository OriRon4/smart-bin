#include <ESP32Servo.h> // ספרייה להפעלת סרווים בפח
#include <Wire.h> // ספרייה לתקשורת עם המסך
#include <Adafruit_GFX.h> // ספריית ציור למסך
#include <Adafruit_SSD1306.h> // ספרייה למסך התצוגה
#include <WiFi.h> // ספרייה לחיבור הפח לרשת
#include <WebServer.h> // ספרייה להפעלת דשבורד בדפדפן
#include <Preferences.h> // ספרייה לשמירת סטטיסטיקות בזיכרון קבוע


// =========================================================
// פיני סרווים
// =========================================================
const int TOP_SERVO_PIN = 18; // פין שמחובר לסרוו של הדלת העליונה
const int BOTTOM_SERVO_PIN = 19; // פין שמחובר לסרוו שער המיון


// =========================================================
// פיני חיישן מרחק
// =========================================================
const int ULTRASONIC_TRIG_PIN = 26; // פין השליחה של חיישן המרחק
const int ULTRASONIC_ECHO_PIN = 27; // פין קבלת ההד מחיישן המרחק


// =========================================================
// פיני תקשורת המצלמה
// =========================================================
const int CAMERA_RX_PIN = 16; // פין קבלת נתונים מהמצלמה
const int CAMERA_TX_PIN = 17; // פין שליחת פקודות למצלמה


// =========================================================
// הגדרות WiFi לדשבורד
// =========================================================
const char* WIFI_SSID = "College"; // שם הרשת שהפח מתחבר אליה לדשבורד
const char* WIFI_PASSWORD = "Amal1@st"; // סיסמת הרשת להפעלת הדשבורד


// =========================================================
// פין הבאזר
// באזר פסיבי
// =========================================================
const int BUZZER_PIN = 25; // פין שמחובר לבאזר המשוב


// =========================================================
// הגדרות מסך
// =========================================================
#define SCREEN_WIDTH 128 // רוחב מסך התצוגה בפיקסלים
#define SCREEN_HEIGHT 64 // גובה מסך התצוגה בפיקסלים
#define OLED_RESET -1 // המסך לא משתמש בפין איפוס נפרד
#define OLED_ADDRESS 0x3C // כתובת המסך בתקשורת
const int OLED_SDA_PIN = 21; // פין הנתונים של המסך
const int OLED_SCL_PIN = 22; // פין השעון של המסך

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // אובייקט שמנהל את מסך המערכת

bool oledReady = false; // שומר אם המסך אותחל בהצלחה


// =========================================================
// דשבורד ושמירת נתונים
// =========================================================
WebServer server(80); // שרת שמציג סטטיסטיקות בדפדפן
Preferences preferences; // אחסון קבוע למוני המיון


// =========================================================
// זוויות סרווים
// =========================================================
const int BOTTOM_CENTER_ANGLE = 100; // זווית שער המיון במרכז
const int PLASTIC_ANGLE = 40; // זווית שער המיון לפלסטיק
const int PAPER_ANGLE = 100; // זווית שער המיון לנייר
const int METAL_ANGLE = 150; // זווית שער המיון למתכת

const int TOP_CLOSED_ANGLE = 26; // זווית סגירת הדלת העליונה
const int TOP_OPEN_ANGLE = 0; // זווית פתיחת הדלת העליונה


// =========================================================
// זמני פעולה
// =========================================================
const unsigned long WAIT_BEFORE_START_MS = 3000; // השהיה קצרה לפני התחלת רצף המיון
const unsigned long WAIT_AFTER_BOTTOM_MOVE_MS = 1500; // זמן לשער התחתון להגיע לפח שנבחר
const unsigned long WAIT_WHILE_TOP_OPEN_MS = 2000; // משך פתיחת הדלת לשחרור החפץ
const unsigned long WAIT_AFTER_TOP_CLOSE_MS = 1500; // זמן אחרי סגירת הדלת לפני החזרת השער
const unsigned long SERVO_SMALL_DELAY_MS = 500; // זמן קצר להשלמת תנועת סרוו

const unsigned long OBJECT_SETTLE_DELAY_MS = 1500; // זמן ייצוב החפץ לפני צילום
const unsigned long DETECTION_COOLDOWN_MS = 10000; // מונע זיהוי חוזר של אותו חפץ

const unsigned long CAMERA_RESPONSE_TIMEOUT_MS = 30000; // זמן מקסימלי להמתנה לתשובת המצלמה


// =========================================================
// זיהוי חפץ בחיישן מרחק
// אם המרחק קטן או שווה לערך הזה, נחשב שיש חפץ
// =========================================================
float OBJECT_DETECTION_THRESHOLD_CM = 8; // מרחק שממנו הפח מזהה חפץ
const int ULTRASONIC_CONFIRM_SAMPLES = 7; // מספר דגימות לאישור חפץ
const unsigned long ULTRASONIC_SAMPLE_DELAY_MS = 60; // רווח זמן בין דגימות המרחק


// =========================================================
// מצב המערכת
// =========================================================
bool autoModeEnabled = true; // מפעיל את זרימת הזיהוי האוטומטית
unsigned long lastDetectionTime = 0; // שומר מתי זוהה החפץ האחרון
int totalSortedCount = 0; // סופר רק חפצים שמוינו פיזית בהצלחה
int plasticCount = 0; // סופר חפצי פלסטיק שמוינו
int paperCount = 0; // סופר חפצי נייר שמוינו
int metalCount = 0; // סופר חפצי מתכת שמוינו
int unknownCount = 0; // סופר חפצים שלא התאימו לקטגוריית מיון
String lastCategory = "none"; // שומר את הקטגוריה האחרונה
String systemStatus = "Starting"; // שומר מצב כללי לדשבורד


// =========================================================
// אובייקטי סרווים
// =========================================================
Servo bottomServo; // סרוו שמכוון את שער המיון
Servo topServo; // סרוו שפותח וסוגר את הדלת


// =========================================================
// Function declarations
// =========================================================
void initializeServos();
void initializeUltrasonic();
void initializeCameraCommunication();
void initializeBuzzer();
void initializeOled();
void moveToReadyPosition();
void loadStats();
void connectToWiFi();
void setupWebServer();
void beepSuccess();
void showReadyScreen();
void checkUltrasonicAndHandleObject();
void handleObjectDetected();
void requestCaptureFromCamera(bool requireUltrasonicConfirmation);
float readDistanceCm();
float readAverageDistanceCm();
bool confirmObjectByAverageDistance();
void clearCameraBuffer();
void sendCommandToCamera(String command);
String readCameraResponse();
bool cameraResponseFailed(String jsonText);
String extractCategoryFromCameraJson(String jsonText);
String extractStringValue(String jsonText, String key);
void returnToReadyState(unsigned long waitBeforeReadyScreenMs);
int getBottomAngleForCategory(String category);
void sortToCategory(String category);
void saveStats();
void resetStats();
void updateStatsForCategory(String category);
void handleDashboard();
void handleResetStats();
void showOledMessage(String line1, String line2 = "", String line3 = "", String line4 = "");
void showCategoryScreen(String category);
void showUnknownScreen();
void showDoneScreen();
void beepShort();
void beepDouble();
void beepLong();

// =========================================================
// Setup
// =========================================================
// =========================================================
// אתחול ראשוני
// =========================================================
void setup() { // מכין את רכיבי המערכת להפעלה
  delay(1000); // נותן לרכיבים להתייצב אחרי ההפעלה

  initializeServos(); // מפעיל את הסרווים של הדלת והשער
  initializeUltrasonic(); // מפעיל את חיישן זיהוי החפץ
  initializeCameraCommunication(); // פותח את הקשר למצלמה
  initializeBuzzer(); // מכין את הבאזר למשוב
  initializeOled(); // מפעיל את מסך המצב

  moveToReadyPosition(); // מחזיר דלת ושער למצב התחלה
  loadStats(); // מחזיר מונים קודמים כדי שהדשבורד ימשיך מאותה נקודה
  connectToWiFi(); // מחבר את הפח לרשת בשביל צפייה בדשבורד
  setupWebServer(); // פותח את עמודי הדשבורד בדפדפן

  beepSuccess(); // מסמן שהמערכת עלתה בהצלחה
  showReadyScreen(); // מציג שהפח מוכן לעבודה
}

// =========================================================
// Main loop
// =========================================================
// =========================================================
// לולאת עבודה
// =========================================================
void loop() { // מריץ את עבודת המערכת ברצף
  server.handleClient(); // מטפל בבקשות מהדשבורד בלי לעצור את המערכת

  if (autoModeEnabled) { // אם המצב האוטומטי פעיל
    checkUltrasonicAndHandleObject(); // בודק אם יש חפץ לטיפול
  }

  delay(100); // נותן מרווח קצר בין בדיקות חיישן
}

// =========================================================
// Main detection and sorting flow
// =========================================================
// =========================================================
// בדיקת חיישן וטיפול בחפץ
// במצב אוטומטי, בודק אם יש חפץ
// =========================================================
void checkUltrasonicAndHandleObject() { // בודק אוטומטית אם הוכנס חפץ
  unsigned long now = millis(); // שומר את זמן הבדיקה הנוכחית

  if (now - lastDetectionTime < DETECTION_COOLDOWN_MS) { // אם עדיין ממתינים לפני זיהוי חדש
    return; // עוצר כדי לא לטפל שוב באותו חפץ
  }

  float distance = readDistanceCm(); // מודד אם יש חפץ קרוב לפתח הפח

  if (distance < 0) { // אם החיישן לא החזיר קריאה תקינה
    return; // עוצר כי אין מדידת מרחק אמינה
  }

  if (distance <= OBJECT_DETECTION_THRESHOLD_CM) { // אם חפץ נמצא בטווח הזיהוי
    showOledMessage("OBJECT DETECTED", "Distance:", String(distance) + " cm"); // מציג שחיישן המרחק זיהה חפץ
    beepShort(); // נותן חיווי קצר שהפח זיהה חפץ

    lastDetectionTime = now; // מתחיל השהיה כדי למנוע זיהוי כפול

    handleObjectDetected(); // ממשיך לתהליך צילום וזיהוי

    lastDetectionTime = millis(); // מחדש השהיה אחרי סיום הטיפול בחפץ
  }
}

// =========================================================
// טיפול בחפץ שזוהה
// כשיש חפץ, מחכים רגע ואז מבקשים מהמצלמה קטגוריה
// =========================================================
void handleObjectDetected() { // מטפל בחפץ אחרי זיהוי ראשוני
  systemStatus = "Object detected"; // מעדכן לדשבורד שחפץ נכנס לתהליך
  showOledMessage("OBJECT DETECTED", "Stabilizing..."); // מציג שהחפץ מתייצב לפני צילום
  delay(OBJECT_SETTLE_DELAY_MS); // נותן לחפץ להתייצב לפני צילום

  requestCaptureFromCamera(true); // מבקש צילום אחרי אישור חפץ
}

// =========================================================
// בקשת צילום מהמצלמה
// שולח צילום למצלמה, מקבל תשובת נתונים, ומפעיל מיון לפי קטגוריה
// =========================================================
void requestCaptureFromCamera(bool requireUltrasonicConfirmation) { // מבקש צילום ומפעיל מיון לפי התשובה
  if (requireUltrasonicConfirmation && !confirmObjectByAverageDistance()) { // אם החיישן לא אישר שיש חפץ
    returnToReadyState(1200); // מחזיר את הפח להמתנה אחרי ביטול צילום
    return; // עוצר כי החפץ לא אושר על ידי החיישן
  }

  systemStatus = "Capturing"; // מעדכן לדשבורד שהפח מחכה לזיהוי
  showOledMessage("CAPTURING", "Please wait..."); // מציג שהמערכת מצלמת וממתינה לזיהוי
  beepShort(); // נותן משוב קולי קצר

  sendCommandToCamera("CAPTURE"); // מבקש מהמצלמה לצלם ולזהות

  String response = readCameraResponse(); // שומר את תשובת הזיהוי שהתקבלה

  if (response.length() == 0) { // אם לא התקבלה תשובה שימושית
    systemStatus = "Camera failed"; // מעדכן לדשבורד שאין תשובת מצלמה
    showOledMessage("CAPTURE FAILED", "No camera response"); // מציג שהמצלמה לא החזירה תשובה
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר כי לא התקבלה תשובה מהמצלמה
  }

  if (!response.startsWith("{")) { // אם תשובת המצלמה אינה נראית כמו JSON
    systemStatus = "Invalid camera response"; // מעדכן לדשבורד שהתשובה לא תקינה
    showOledMessage("CAPTURE FAILED", "Invalid response"); // מציג שתשובת המצלמה לא תקינה
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר כי התשובה אינה JSON תקין למיון
  }

  if (cameraResponseFailed(response)) { // אם תשובת הזיהוי מדווחת על כשל
    systemStatus = "Camera error"; // מעדכן לדשבורד שיש שגיאת מצלמה או זיהוי
    showOledMessage("CAMERA ERROR", "Try again"); // מציג שיש שגיאת מצלמה או זיהוי
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר כי תשובת הזיהוי דיווחה על כשל
  }

  String category = extractCategoryFromCameraJson(response); // שומר את סוג החומר מתשובת הזיהוי

  if (category == "unknown") { // אם החומר לא זוהה כמיון תקין
    systemStatus = "Unknown"; // מעדכן לדשבורד שהתוצאה לא זוהתה
    updateStatsForCategory("unknown"); // שומר אירוע לא מזוהה בסטטיסטיקה
    showUnknownScreen(); // מציג שהחומר לא ניתן למיון
    beepLong(); // מסמן שגיאה למשתמש

    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר כי אין קטגוריה תקינה למיון פיזי
  }

  showCategoryScreen(category); // מציג את סוג החומר לפני מיון
  beepDouble(); // נותן משוב לפני מיון

  sortToCategory(category); // מפעיל מיון פיזי לפי סוג החומר
}

// =========================================================
// Ultrasonic distance sensor functions
// =========================================================
// =========================================================
// אתחול חיישן מרחק
// =========================================================
void initializeUltrasonic() { // מכין את חיישן המרחק לזיהוי חפצים
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT); // מכין פין לשליחת פולס מדידה
  pinMode(ULTRASONIC_ECHO_PIN, INPUT); // מכין פין לקבלת ההד מהחיישן

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מבטיח שהחיישן מתחיל ללא פולס
}

// =========================================================
// קריאת מרחק בסנטימטרים
// מחזיר מרחק בס"מ
// אם אין קריאה תקינה, מחזיר -1
// =========================================================
float readDistanceCm() { // מודד אם יש חפץ קרוב לפתח הפח
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מוודא שפולס המדידה מתחיל ממצב שקט
  delayMicroseconds(2); // מכין את פולס המדידה

  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH); // שולח פולס מדידה לחיישן
  delayMicroseconds(10); // יוצר פולס קצר לחיישן המרחק

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מסיים את פולס המדידה

  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000); // מודד כמה זמן לקח להד לחזור

  if (duration == 0) { // אם החיישן לא החזיר קריאה תקינה
    return -1; // מסמן שאין קריאת מרחק תקינה
  }

  float distanceCm = duration * 0.0343 / 2.0; // מחשב מרחק לפי זמן ההד
  return distanceCm; // מחזיר את המרחק שחושב
}

// =========================================================
// קריאת מרחק ממוצע
// ממוצע קריאות תקינות בלבד
// =========================================================
float readAverageDistanceCm() { // מבצע כמה מדידות כדי למנוע צילום שגוי
  float totalDistance = 0; // צובר את המרחקים התקינים
  int validReadings = 0; // סופר כמה דגימות היו תקינות

  for (int i = 0; i < ULTRASONIC_CONFIRM_SAMPLES; i++) { // מבצע כמה דגימות מרחק לאימות
    float distance = readDistanceCm(); // מודד שוב אם החפץ עדיין בפתח

    if (distance >= 0) { // אם הדגימה תקינה ונכנסת לממוצע
      totalDistance += distance; // מוסיף דגימה תקינה לחישוב הממוצע
      validReadings++; // סופר דגימת מרחק תקינה
    }

    if (i < ULTRASONIC_CONFIRM_SAMPLES - 1) { // אם צריך להמתין לדגימה הבאה
      delay(ULTRASONIC_SAMPLE_DELAY_MS); // מפריד בין דגימות כדי לקבל ממוצע יציב
    }
  }

  if (validReadings == 0) { // אם לא התקבלה אף דגימה תקינה
    return -1; // מסמן שאין קריאת מרחק תקינה
  }

  return totalDistance / validReadings; // מחזיר מרחק ממוצע מדגימות תקינות
}

// =========================================================
// אישור חפץ לפי מרחק ממוצע
// בודק ממוצע לפני צילום
// =========================================================
bool confirmObjectByAverageDistance() { // מאשר שהחפץ עדיין נמצא לפני שליחת צילום
  float averageDistance = readAverageDistanceCm(); // בודק מרחק ממוצע כדי לא לצלם בלי חפץ

  if (averageDistance < 0) { // אם אין קריאת מרחק אמינה
    showOledMessage("ULTRASONIC ERROR", "No valid reading"); // מציג שגיאת חיישן מרחק
    beepLong(); // מסמן שגיאה למשתמש
    return false; // עוצר צילום כי אין אישור אמין מהחיישן
  }

  if (averageDistance > OBJECT_DETECTION_THRESHOLD_CM) { // אם החפץ כבר לא קרוב מספיק
    showOledMessage("NO OBJECT", "Average:", String(averageDistance) + " cm"); // מציג שהחפץ לא אושר לצילום
    beepLong(); // מסמן שגיאה למשתמש
    return false; // עוצר צילום כי החפץ כבר לא בטווח
  }

  return true; // מאשר שאפשר להמשיך לצילום במצלמה
}

// =========================================================
// ESP32-CAM UART communication functions
// =========================================================
// =========================================================
// אתחול תקשורת מצלמה
// =========================================================
void initializeCameraCommunication() { // פותח תקשורת מול המצלמה
  Serial2.begin(115200, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN); // פותח UART בין הבקר הראשי ל-ESP32-CAM
}

// =========================================================
// ניקוי תקשורת המצלמה
// מנקה תשובות ישנות מהמצלמה לפני שליחת פקודה חדשה
// =========================================================
void clearCameraBuffer() { // מנקה תשובות ישנות מהמצלמה
  while (Serial2.available()) { // כל עוד נשאר מידע ישן מהמצלמה
    Serial2.read(); // מוחק תו ישן כדי שהצילום הבא יקבל תשובה נקייה
  }
}

// =========================================================
// שליחת פקודה למצלמה
// שולח פקודה ל-מצלמת הזיהוי
// =========================================================
void sendCommandToCamera(String command) { // שולח פקודה למצלמה דרך UART
  clearCameraBuffer(); // מוחק תשובות ישנות לפני צילום חדש

  Serial2.println(command); // שולח למצלמה פקודת CAPTURE דרך UART
}

// =========================================================
// קריאת תשובת מצלמה
// קורא תשובה תקינה מה-מצלמת הזיהוי
// =========================================================
String readCameraResponse() { // ממתין לתשובת המצלמה
  unsigned long startTime = millis(); // שומר מתי התחילה ההמתנה למצלמה

  while (millis() - startTime < CAMERA_RESPONSE_TIMEOUT_MS) { // ממתין לתשובת המצלמה עד הזמן המוגדר
    if (Serial2.available()) { // בודק אם המצלמה שלחה תשובה
      String response = Serial2.readStringUntil('\n'); // קורא תשובת JSON שהמצלמה מחזירה בשורה אחת
      response.trim(); // מנקה את תשובת המצלמה לפני בדיקת המבנה

      if (response.length() == 0) { // אם לא התקבלה תשובה שימושית
        continue; // מתעלם משורה ריקה וממשיך להמתין לתשובת מצלמה
      }

      if (response.startsWith("{")) { // אם התקבלה תשובה שנראית כמו JSON
        return response; // מחזיר JSON לעיבוד תוצאת הזיהוי
      }
    }

    delay(20); // נותן מרווח קצר בין בדיקות UART
  }

  return ""; // מסמן שלא התקבלה תשובה מהמצלמה
}

// =========================================================
// JSON extraction functions
// =========================================================
bool cameraResponseFailed(String jsonText) { // בודק אם המצלמה או השרת החזירו success:false
  jsonText.toLowerCase(); // מאחד כתיבה כדי לזהות success:false בכל צורה
  jsonText.replace(" ", ""); // מסיר רווחים כדי לבדוק את שדה ההצלחה

  return jsonText.indexOf("\"success\":false") != -1; // מחזיר אם התשובה מסמנת כשל
}

// =========================================================
// חילוץ קטגוריה מתשובת המצלמה
// מוציא את קטגוריה מהתשובה של המצלמה
// =========================================================
String extractCategoryFromCameraJson(String jsonText) { // מוציא את סוג החומר מתשובת המצלמה
  String category = extractStringValue(jsonText, "category"); // קורא את קטגוריית החומר מה-JSON

  category.trim(); // מנקה את תוצאת הזיהוי לפני השוואה
  category.toLowerCase(); // מאחד כתיבה לפני בדיקת קטגוריות

  if (category == "plastic" || category == "paper" || category == "metal") { // אם התקבל חומר שניתן למיון
    return category; // מחזיר את סוג החומר למיון
  }

  return "unknown"; // מחזיר שהחומר לא מוכר למערכת
}

// =========================================================
// חילוץ ערך מתשובת JSON פשוטה
// הפונקציה מיועדת לשדות טקסט פשוטים כמו category
// =========================================================
String extractStringValue(String jsonText, String key) { // מוציא ערך טקסטואלי מתשובת JSON פשוטה
  String keyPattern = "\"" + key + "\""; // בונה תבנית לחיפוש שדה בתשובת המצלמה

  int keyIndex = jsonText.indexOf(keyPattern); // מוצא איפה נמצא השדה המבוקש

  if (keyIndex == -1) { // אם שדה הנתונים לא נמצא
    return ""; // מסמן שהשדה הדרוש לא נמצא בתשובה
  }

  int colonIndex = jsonText.indexOf(":", keyIndex); // שומר את מיקום הנקודתיים בשדה

  if (colonIndex == -1) { // אם מבנה השדה לא תקין
    return ""; // מסמן שלא ניתן לקרוא את מבנה השדה
  }

  int firstQuote = jsonText.indexOf("\"", colonIndex); // שומר את תחילת הערך בתשובה

  if (firstQuote == -1) { // אם תחילת הערך לא נמצאה
    return ""; // מסמן שלא נמצא ערך טקסטואלי לשדה
  }

  int secondQuote = jsonText.indexOf("\"", firstQuote + 1); // שומר את סוף הערך בתשובה

  if (secondQuote == -1) { // אם סוף הערך לא נמצא
    return ""; // מסמן שהערך בתשובה לא נסגר בצורה תקינה
  }

  return jsonText.substring(firstQuote + 1, secondQuote); // מחזיר את הערך שנמצא בתשובה
}

// =========================================================
// Sorting and servo functions
// =========================================================
// =========================================================
// אתחול סרווים
// =========================================================
void initializeServos() { // מחבר את מנועי המיון לפינים שלהם
  bottomServo.attach(BOTTOM_SERVO_PIN); // מחבר את סרוו שער המיון לפין הבקרה
  topServo.attach(TOP_SERVO_PIN); // מחבר את סרוו הדלת העליונה לפין הבקרה

  delay(500); // נותן לסרווים להתייצב אחרי החיבור
}

// =========================================================
// מעבר למצב מוכן
// =========================================================
void moveToReadyPosition() { // מחזיר את הסרווים למצב המתנה
  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת כדי שלא ייפול חפץ בזמן המתנה
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לסרוו הדלת להשלים סגירה

  bottomServo.write(BOTTOM_CENTER_ANGLE); // מחזיר את שער המיון למרכז
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לשער התחתון לחזור למרכז
}

// =========================================================
// חזרה למצב מוכן
// =========================================================
void returnToReadyState(unsigned long waitBeforeReadyScreenMs) { // מחזיר את הפח למסך המתנה
  moveToReadyPosition(); // מחזיר דלת ושער למצב התחלה

  if (waitBeforeReadyScreenMs > 0) { // אם צריך להציג שגיאה לפני חזרה
    delay(waitBeforeReadyScreenMs); // משאיר את הודעת השגיאה על המסך לפני READY
  }

  showReadyScreen(); // מציג שהפח מוכן לעבודה
}

// =========================================================
// בחירת זווית תחתונה לפי קטגוריה
// =========================================================
int getBottomAngleForCategory(String category) { // בוחר זווית שער לפי סוג החומר
  category.trim(); // מנקה את תוצאת הזיהוי לפני בחירת זווית
  category.toLowerCase(); // מאחד כתיבה כדי להתאים לשמות הקטגוריות

  if (category == "plastic") { // אם החומר הוא פלסטיק
    return PLASTIC_ANGLE; // מחזיר את זווית השער לפח הפלסטיק
  }

  if (category == "paper") { // אם החומר הוא נייר
    return PAPER_ANGLE; // מחזיר את זווית השער לפח הנייר
  }

  if (category == "metal") { // אם החומר הוא מתכת
    return METAL_ANGLE; // מחזיר את זווית השער לפח המתכת
  }

  return BOTTOM_CENTER_ANGLE; // מחזיר את השער למרכז כברירת מחדל
}

// =========================================================
// מיון לפי קטגוריה
// =========================================================
void sortToCategory(String category) { // מבצע את תהליך המיון הפיזי
  category.trim(); // מנקה את תוצאת הזיהוי לפני מיון
  category.toLowerCase(); // מאחד כתיבה לפני בדיקת יעד מיון

  if (category != "plastic" && category != "paper" && category != "metal") { // אם אין יעד מיון חוקי
    systemStatus = "Unknown"; // מעדכן לדשבורד שאין יעד מיון תקין
    updateStatsForCategory("unknown"); // שומר אירוע לא מזוהה בסטטיסטיקה
    showUnknownScreen(); // מציג שהחומר לא ניתן למיון
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר כי הקטגוריה לא תקינה למיון
  }

  int targetBottomAngle = getBottomAngleForCategory(category); // שומר את זווית שער המיון שנבחרה

  systemStatus = "Sorting"; // מעדכן לדשבורד שהפח מבצע מיון
  showOledMessage("SORTING", "Category:", category); // מציג את סוג החומר לפני תנועת השער

  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת כדי שהחפץ לא ייפול לפני כיוון השער
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לסרוו הדלת להשלים סגירה

  delay(WAIT_BEFORE_START_MS); // נותן השהיה קצרה לפני רצף המיון הפיזי

  showOledMessage("SORTING", "Moving gate...", category); // מציג שהשער זז לפח המתאים
  bottomServo.write(targetBottomAngle); // מכוון את שער המיון לפח הנכון
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לשער להגיע לזווית הקטגוריה

  delay(WAIT_AFTER_BOTTOM_MOVE_MS); // ממתין שהשער יתייצב לפני פתיחת הדלת

  showOledMessage("SORTING", "Opening door...", category); // מציג שהדלת נפתחת להפלת החפץ
  topServo.write(TOP_OPEN_ANGLE); // פותח את הדלת כדי להפיל את החפץ
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לדלת העליונה להיפתח לגמרי

  delay(WAIT_WHILE_TOP_OPEN_MS); // משאיר את הדלת פתוחה כדי שהחפץ ייפול

  showOledMessage("SORTING", "Closing door...", category); // מציג שהדלת נסגרת אחרי ההפלה
  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת אחרי שהחפץ נפל
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לדלת העליונה להיסגר לגמרי

  delay(WAIT_AFTER_TOP_CLOSE_MS); // ממתין אחרי סגירת הדלת לפני החזרת השער

  showOledMessage("SORTING", "Returning center..."); // מציג שהשער חוזר למרכז
  bottomServo.write(BOTTOM_CENTER_ANGLE); // מחזיר את שער המיון למרכז
  delay(SERVO_SMALL_DELAY_MS); // מאפשר לשער התחתון לחזור למרכז

  updateStatsForCategory(category); // מעדכן ושומר מיון פיזי מוצלח בסטטיסטיקה
  showDoneScreen(); // מציג שהמיון הסתיים
  beepSuccess(); // מסמן למשתמש שהמיון הסתיים בהצלחה

  delay(1500); // משאיר את הודעת הסיום לפני חזרה להמתנה
  showReadyScreen(); // מציג שהפח מוכן לעבודה
}

// =========================================================
// Statistics functions
// =========================================================
void loadStats() { // טוען מונים קודמים מהזיכרון הקבוע
  preferences.begin("smartbin", false); // פותח אזור שמירה קבוע לפרויקט
  totalSortedCount = preferences.getInt("total", 0); // טוען סך מיונים פיזיים מוצלחים
  plasticCount = preferences.getInt("plastic", 0); // טוען כמות פלסטיק
  paperCount = preferences.getInt("paper", 0); // טוען כמות נייר
  metalCount = preferences.getInt("metal", 0); // טוען כמות מתכת
  unknownCount = preferences.getInt("unknown", 0); // טוען כמות חפצים ללא יעד מיון
  lastCategory = preferences.getString("last", "none"); // טוען את הקטגוריה האחרונה
}

void saveStats() { // שומר מונים אחרי אירוע כדי שלא יאבדו בכיבוי
  preferences.putInt("total", totalSortedCount); // שומר סך מיונים פיזיים מוצלחים
  preferences.putInt("plastic", plasticCount); // שומר כמות פלסטיק
  preferences.putInt("paper", paperCount); // שומר כמות נייר
  preferences.putInt("metal", metalCount); // שומר כמות מתכת
  preferences.putInt("unknown", unknownCount); // שומר כמות חפצים ללא יעד מיון
  preferences.putString("last", lastCategory); // שומר את הקטגוריה האחרונה
}

void resetStats() { // מאפס את סטטיסטיקות הדשבורד
  totalSortedCount = 0; // מאפס סך מיונים פיזיים מוצלחים
  plasticCount = 0; // מאפס כמות פלסטיק
  paperCount = 0; // מאפס כמות נייר
  metalCount = 0; // מאפס כמות מתכת
  unknownCount = 0; // מאפס כמות חפצים ללא יעד מיון
  lastCategory = "none"; // מנקה את הקטגוריה האחרונה
  saveStats(); // שומר את האיפוס בזיכרון קבוע
}

void updateStatsForCategory(String category) { // מעדכן מונים לפי תוצאת הזיהוי
  category.trim(); // מנקה את תוצאת הזיהוי לפני השוואה לקטגוריות
  category.toLowerCase(); // מאחד את כתיבת הקטגוריה לפני עדכון מונים

  if (category == "plastic") { // אם החפץ מוין לפלסטיק
    totalSortedCount++; // מוסיף מיון פיזי מוצלח לסך הכללי
    plasticCount++; // מוסיף חפץ למונה הפלסטיק
    lastCategory = "plastic"; // שומר שזו הקטגוריה האחרונה
  } else if (category == "paper") { // אם החפץ מוין לנייר
    totalSortedCount++; // מוסיף מיון פיזי מוצלח לסך הכללי
    paperCount++; // מוסיף חפץ למונה הנייר
    lastCategory = "paper"; // שומר שזו הקטגוריה האחרונה
  } else if (category == "metal") { // אם החפץ מוין למתכת
    totalSortedCount++; // מוסיף מיון פיזי מוצלח לסך הכללי
    metalCount++; // מוסיף חפץ למונה המתכת
    lastCategory = "metal"; // שומר שזו הקטגוריה האחרונה
  } else { // אם החפץ לא קיבל קטגוריית מיון תקינה
    unknownCount++; // מוסיף חפץ למונה ללא יעד מיון
    lastCategory = "unknown"; // שומר שהתוצאה האחרונה לא זוהתה
  }

  saveStats(); // שומר את הסטטיסטיקה לאחר עדכון הקטגוריה
}

// =========================================================
// WiFi and dashboard functions
// =========================================================
// =========================================================
// חיבור WiFi ודשבורד
// =========================================================
void connectToWiFi() { // מחבר את הפח לרשת בשביל צפייה בסטטיסטיקות
  WiFi.mode(WIFI_STA); // מגדיר את ה-ESP32 כלקוח ברשת המקומית
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // מחבר את ה-ESP32 לרשת בשביל הדשבורד

  int attempts = 0; // סופר ניסיונות חיבור לרשת

  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // ממתין לחיבור בלי להיתקע לנצח
    delay(500); // נותן ל-WiFi זמן להשלים ניסיון חיבור
    attempts++; // מתקדם לניסיון החיבור הבא
  }

  if (WiFi.status() == WL_CONNECTED) { // אם הדשבורד זמין ברשת
    showOledMessage("WiFi OK", WiFi.localIP().toString()); // מציג את כתובת הדשבורד על המסך
    delay(1200); // נותן זמן קצר לקרוא את כתובת ה-IP
  } else { // אם החיבור לרשת לא הצליח
    showOledMessage("WiFi FAILED", "Dashboard off"); // מודיע שהדשבורד לא זמין כרגע
    delay(1200); // נותן זמן קצר לראות את הודעת הרשת
  }
}

void setupWebServer() { // מגדיר את כתובות הדשבורד
  server.on("/", handleDashboard); // מחבר את כתובת הבית לעמוד הסטטיסטיקות
  server.on("/reset", handleResetStats); // מחבר כתובת איפוס למוני הדשבורד
  server.begin(); // מתחיל להאזין לבקשות דפדפן
}

void handleDashboard() { // שולח לדפדפן את עמוד הסטטיסטיקות
  String html = ""; // בונה את עמוד הדשבורד

  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>"; // מתחיל את עמוד הדשבורד ומגדיר קידוד תווים
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>"; // מתאים את העמוד לטלפון
  html += "<title>Smart Bin Dashboard</title>"; // כותרת הדפדפן
  html += "<style>"; // מוסיף עיצוב בסיסי לדשבורד
  html += "body{font-family:Arial,sans-serif;margin:24px;background:#f5f7fa;color:#17202a;}"; // שומר על תצוגה נקייה
  html += "table{border-collapse:collapse;width:100%;max-width:520px;background:white;}"; // מסדר את נתוני הפח בטבלה
  html += "td,th{border:1px solid #d7dde5;padding:10px;text-align:left;}"; // מפריד בין שורות הנתונים
  html += "th{background:#eef2f6;}"; // מדגיש את כותרת הטבלה
  html += "a{display:inline-block;margin-top:16px;color:#0b63ce;}"; // מציג קישור איפוס פשוט
  html += "</style></head><body>"; // מסיים את הגדרות העמוד
  html += "<h1>Smart Bin Dashboard</h1>"; // מציג כותרת ראשית לדשבורד
  html += "<table>"; // מתחיל טבלת סטטיסטיקות
  html += "<tr><th>Item</th><th>Value</th></tr>"; // מציג כותרות לעמודות
  html += "<tr><td>System status</td><td>"; // מתחיל שורת מצב מערכת
  html += systemStatus; // מציג מצב מערכת
  html += "</td></tr>"; // סוגר שורת מצב מערכת
  html += "<tr><td>Total sorted</td><td>"; // מתחיל שורת סך מיונים
  html += String(totalSortedCount); // מציג רק מיונים פיזיים מוצלחים
  html += "</td></tr>"; // סוגר שורת סך מיונים
  html += "<tr><td>Plastic count</td><td>"; // מתחיל שורת פלסטיק
  html += String(plasticCount); // מציג כמות פלסטיק
  html += "</td></tr>"; // סוגר שורת פלסטיק
  html += "<tr><td>Paper count</td><td>"; // מתחיל שורת נייר
  html += String(paperCount); // מציג כמות נייר
  html += "</td></tr>"; // סוגר שורת נייר
  html += "<tr><td>Metal count</td><td>"; // מתחיל שורת מתכת
  html += String(metalCount); // מציג כמות מתכת
  html += "</td></tr>"; // סוגר שורת מתכת
  html += "<tr><td>Unknown count</td><td>"; // מתחיל שורת לא מזוהה
  html += String(unknownCount); // מציג חפצים שלא קיבלו יעד מיון
  html += "</td></tr>"; // סוגר שורת לא מזוהה
  html += "<tr><td>Last category</td><td>"; // מתחיל שורת קטגוריה אחרונה
  html += lastCategory; // מציג את תוצאת הזיהוי האחרונה
  html += "</td></tr>"; // סוגר שורת קטגוריה אחרונה
  html += "<tr><td>Auto mode</td><td>"; // מתחיל שורת מצב אוטומטי
  html += (autoModeEnabled ? "ON" : "OFF"); // מציג אם המצב האוטומטי פעיל
  html += "</td></tr>"; // סוגר שורת מצב אוטומטי
  html += "<tr><td>Object threshold</td><td>"; // מתחיל שורת סף זיהוי
  html += String(OBJECT_DETECTION_THRESHOLD_CM); // מציג סף זיהוי חפץ
  html += " cm</td></tr>"; // סוגר שורת סף זיהוי
  html += "</table>"; // מסיים את טבלת הסטטיסטיקות
  html += "<a href='/reset'>Reset statistics</a>"; // קישור לאיפוס מוני הדשבורד
  html += "</body></html>"; // מסיים את עמוד הדשבורד

  server.send(200, "text/html", html); // מחזיר את הדשבורד לדפדפן
}

void handleResetStats() { // מטפל בבקשת איפוס מהדשבורד
  resetStats(); // מאפס מונים ושומר כדי שהאיפוס יישאר אחרי כיבוי

  String html = ""; // בונה הודעת איפוס פשוטה
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>"; // מגדיר עמוד תשובה תקין
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>"; // מתאים את העמוד לטלפון
  html += "<title>Stats Reset</title></head><body>"; // מגדיר כותרת לדף האיפוס
  html += "<h1>Statistics reset</h1>"; // מודיע שהאיפוס בוצע
  html += "<p>All counters were reset.</p>"; // מפרט שהמונים אופסו
  html += "<a href='/'>Back to dashboard</a>"; // מחזיר את המשתמש לדשבורד
  html += "</body></html>"; // מסיים את עמוד האיפוס

  server.send(200, "text/html", html); // מחזיר לדפדפן אישור איפוס
}

// =========================================================
// OLED display functions
// =========================================================
// =========================================================
// פעולות מסך
// =========================================================
void initializeOled() { // מפעיל את המסך להצגת מצב הפח
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN); // פותח תקשורת I2C למסך המצב

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // בודק אם המסך מחובר וזמין
    oledReady = false; // מסמן שלא ניתן להשתמש במסך
    return; // עוצר כי המסך לא אותחל
  }

  oledReady = true; // מסמן שהמסך מוכן להצגת הודעות

  display.clearDisplay(); // מנקה הודעה קודמת מהמסך
  display.display(); // מרענן את המסך בפועל
}

void showOledMessage(String line1, String line2, String line3, String line4) { // מציג למשתמש את מצב הפעולה הנוכחי
  if (!oledReady) { // אם המסך לא זמין להצגה
    return; // עוצר כי אין מסך זמין להצגת הודעה
  }

  display.clearDisplay(); // מנקה הודעה קודמת מהמסך
  display.setTextColor(SSD1306_WHITE); // קובע צבע כתיבה למסך

  display.setTextSize(1); // קובע גודל טקסט אחיד למסך
  display.setCursor(0, 0); // ממקם את כותרת המערכת
  display.println("SMART BIN"); // מציג את שם המערכת במסך

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE); // מפריד בין הכותרת למידע

  display.setCursor(0, 18); // ממקם את שורת המידע הראשונה
  display.println(line1); // מציג את מצב הפעולה הראשי

  if (line2.length() > 0) { // אם יש שורה שנייה להצגה
    display.setCursor(0, 30); // ממקם את שורת המידע השנייה
    display.println(line2); // מציג פרט נוסף למשתמש
  }

  if (line3.length() > 0) { // אם יש שורה שלישית להצגה
    display.setCursor(0, 42); // ממקם את שורת המידע השלישית
    display.println(line3); // מציג מידע נוסף על הפעולה
  }

  if (line4.length() > 0) { // אם יש שורה רביעית להצגה
    display.setCursor(0, 54); // ממקם את שורת המידע הרביעית
    display.println(line4); // מציג מידע נוסף אם יש מקום
  }

  display.display(); // מרענן את המסך בפועל
}

void showReadyScreen() { // מציג שהפח מוכן לחפץ הבא
  systemStatus = "Ready"; // מעדכן לדשבורד שהפח ממתין
  showOledMessage("READY", "Waiting for object", autoModeEnabled ? "Auto: ON" : "Auto: OFF"); // מציג שהפח ממתין לחפץ
}

void showCategoryScreen(String category) { // מציג את סוג החומר שהתקבל
  category.toUpperCase(); // הופך את שם החומר לברור יותר על המסך
  showOledMessage("CATEGORY:", category, "Sorting..."); // מציג את סוג החומר בזמן מיון
}

void showUnknownScreen() { // מציג שהחומר לא זוהה למיון
  showOledMessage("UNKNOWN", "Sorting cancelled"); // מציג שלא נמצא יעד מיון מתאים
}

void showDoneScreen() { // מציג שהמיון הסתיים
  showOledMessage("DONE", "Ready again"); // מציג שהמיון הסתיים והפח מוכן
}

// =========================================================
// Buzzer feedback functions
// =========================================================
// =========================================================
// פעולות באזר
// =========================================================
void initializeBuzzer() { // מכין את הבאזר למשוב קולי
  pinMode(BUZZER_PIN, OUTPUT); // מגדיר את הבאזר כפלט
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}

void beepShort() { // משמיע צפצוף קצר לאישור פעולה
  tone(BUZZER_PIN, 1000); // מתחיל צפצוף אישור קצר
  delay(120); // משאיר את צפצוף האישור פעיל לזמן קצר
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}

void beepDouble() { // משמיע שני צפצופים לפני מיון
  beepShort(); // משמיע צפצוף ראשון לפני תחילת המיון
  delay(120); // יוצר מרווח קצר בין שני הצפצופים
  beepShort(); // משמיע צפצוף שני לפני תחילת המיון
}

void beepLong() { // משמיע צפצוף ארוך לשגיאה
  tone(BUZZER_PIN, 700); // מתחיל צפצוף שגיאה
  delay(500); // משאיר את צפצוף השגיאה פעיל לחצי שנייה
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}

void beepSuccess() { // משמיע צליל הצלחה בסיום
  tone(BUZZER_PIN, 1200); // מתחיל צליל הצלחה ראשון
  delay(120); // משאיר את הטון הראשון פעיל לזמן קצר
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר

  delay(100); // יוצר מרווח קצר בין שני צלילי ההצלחה

  tone(BUZZER_PIN, 1600); // מתחיל צליל הצלחה שני
  delay(160); // משאיר את הטון השני פעיל לזמן קצר
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}
