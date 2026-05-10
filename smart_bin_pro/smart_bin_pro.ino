#include <ESP32Servo.h> // ספרייה להפעלת סרווים בפח
#include <Wire.h> // ספרייה לתקשורת עם המסך
#include <Adafruit_GFX.h> // ספריית ציור למסך
#include <Adafruit_SSD1306.h> // ספרייה למסך התצוגה


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
const unsigned long WAIT_BEFORE_START_MS = 3000; // זמן המתנה לפני תחילת המיון
const unsigned long WAIT_AFTER_BOTTOM_MOVE_MS = 1500; // זמן המתנה אחרי כיוון שער המיון
const unsigned long WAIT_WHILE_TOP_OPEN_MS = 2000; // משך פתיחת הדלת לשחרור החפץ
const unsigned long WAIT_AFTER_TOP_CLOSE_MS = 1500; // זמן המתנה אחרי סגירת הדלת
const unsigned long SERVO_SMALL_DELAY_MS = 500; // זמן קצר לתנועת סרוו בסיסית

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
bool autoModeEnabled = true; // קובע אם הפח מזהה חפצים אוטומטית
unsigned long lastDetectionTime = 0; // שומר מתי זוהה החפץ האחרון


// =========================================================
// אובייקטי סרווים
// =========================================================
Servo bottomServo; // סרוו שמכוון את שער המיון
Servo topServo; // סרוו שפותח וסוגר את הדלת


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

  beepSuccess(); // מסמן שהמערכת עלתה בהצלחה
  showReadyScreen(); // מציג שהפח מוכן לעבודה
}


// =========================================================
// לולאת עבודה
// =========================================================
void loop() { // מריץ את עבודת המערכת ברצף
  if (autoModeEnabled) { // אם המצב האוטומטי פעיל
    checkUltrasonicAndHandleObject(); // בודק אם יש חפץ לטיפול
  }

  delay(100); // מקטין עומס בין בדיקות חיישן
}


// =========================================================
// אתחול סרווים
// =========================================================
void initializeServos() { // מחבר את הסרווים לפינים שלהם
  bottomServo.attach(BOTTOM_SERVO_PIN); // מחבר את סרוו השער לפין שלו
  topServo.attach(TOP_SERVO_PIN); // מחבר את סרוו הדלת לפין שלו

  delay(500); // נותן לסרווים זמן להיצמד למצב התחלתי
}


// =========================================================
// אתחול חיישן מרחק
// =========================================================
void initializeUltrasonic() { // מכין את חיישן המרחק לזיהוי חפצים
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT); // מגדיר את פין שליחת החיישן
  pinMode(ULTRASONIC_ECHO_PIN, INPUT); // מגדיר את פין קבלת החיישן

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מבטיח שהחיישן מתחיל ללא פולס
}


// =========================================================
// אתחול תקשורת מצלמה
// =========================================================
void initializeCameraCommunication() { // פותח תקשורת מול המצלמה
  Serial2.begin(115200, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN); // פותח קו תקשורת מול מצלמת הזיהוי
}


// =========================================================
// פעולות מסך
// =========================================================
void initializeOled() { // מפעיל את המסך להצגת מצב הפח
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN); // פותח תקשורת למסך התצוגה

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // בודק אם המסך מחובר וזמין
    oledReady = false; // מסמן שלא ניתן להשתמש במסך
    return; // עוצר את הפעולה בנקודה זו
  }

  oledReady = true; // מסמן שהמסך מוכן להצגת הודעות

  display.clearDisplay(); // מנקה הודעה קודמת מהמסך
  display.display(); // מרענן את המסך בפועל
}


void showOledMessage(String line1, String line2 = "", String line3 = "", String line4 = "") { // מציג הודעה כללית במסך
  if (!oledReady) { // אם המסך לא זמין להצגה
    return; // עוצר את הפעולה בנקודה זו
  }

  display.clearDisplay(); // מנקה הודעה קודמת מהמסך
  display.setTextColor(SSD1306_WHITE); // קובע צבע כתיבה למסך

  display.setTextSize(1); // קובע גודל טקסט אחיד למסך
  display.setCursor(0, 0); // ממקם את כותרת המערכת
  display.println("SMART BIN"); // מציג את שם המערכת במסך

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE); // מפריד בין הכותרת למידע

  display.setCursor(0, 18); // ממקם את שורת המידע הראשונה
  display.println(line1); // מציג את ההודעה הראשית

  if (line2.length() > 0) { // אם יש שורה שנייה להצגה
    display.setCursor(0, 30); // ממקם את שורת המידע השנייה
    display.println(line2); // מציג פרטי מצב נוספים
  }

  if (line3.length() > 0) { // אם יש שורה שלישית להצגה
    display.setCursor(0, 42); // ממקם את שורת המידע השלישית
    display.println(line3); // מציג שורת מידע נוספת
  }

  if (line4.length() > 0) { // אם יש שורה רביעית להצגה
    display.setCursor(0, 54); // ממקם את שורת המידע הרביעית
    display.println(line4); // מציג שורת מידע אחרונה
  }

  display.display(); // מרענן את המסך בפועל
}


void showReadyScreen() { // מציג שהפח מוכן לחפץ הבא
  showOledMessage("READY", "Waiting for object", autoModeEnabled ? "Auto: ON" : "Auto: OFF"); // מציג שהפח ממתין לחפץ
}


void showCategoryScreen(String category) { // מציג את סוג החומר שהתקבל
  category.toUpperCase(); // מציג את סוג החומר בצורה ברורה
  showOledMessage("CATEGORY:", category, "Sorting..."); // מציג את סוג החומר בזמן מיון
}


void showUnknownScreen() { // מציג שהחומר לא זוהה למיון
  showOledMessage("UNKNOWN", "Sorting cancelled"); // מציג שלא נמצא יעד מיון מתאים
}


void showDoneScreen() { // מציג שהמיון הסתיים
  showOledMessage("DONE", "Ready again"); // מציג שהמיון הסתיים והפח מוכן
}


// =========================================================
// פעולות באזר
// =========================================================
void initializeBuzzer() { // מכין את הבאזר למשוב קולי
  pinMode(BUZZER_PIN, OUTPUT); // מגדיר את הבאזר כפלט
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}


void beepShort() { // משמיע צפצוף קצר לאישור פעולה
  tone(BUZZER_PIN, 1000); // מתחיל צפצוף אישור קצר
  delay(120); // קובע אורך צפצוף קצר
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}


void beepDouble() { // משמיע שני צפצופים לפני מיון
  beepShort(); // נותן משוב קולי קצר
  delay(120); // קובע אורך צפצוף קצר
  beepShort(); // נותן משוב קולי קצר
}


void beepLong() { // משמיע צפצוף ארוך לשגיאה
  tone(BUZZER_PIN, 700); // מתחיל צפצוף שגיאה
  delay(500); // נותן לסרווים זמן להיצמד למצב התחלתי
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}


void beepSuccess() { // משמיע צליל הצלחה בסיום
  tone(BUZZER_PIN, 1200); // מתחיל צליל הצלחה ראשון
  delay(120); // קובע אורך צפצוף קצר
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר

  delay(100); // מקטין עומס בין בדיקות חיישן

  tone(BUZZER_PIN, 1600); // מתחיל צליל הצלחה שני
  delay(160); // קובע את סוף צליל ההצלחה
  noTone(BUZZER_PIN); // עוצר את צליל הבאזר
}


// =========================================================
// קריאת מרחק בסנטימטרים
// מחזיר מרחק בס"מ
// אם אין קריאה תקינה, מחזיר -1
// =========================================================
float readDistanceCm() { // קורא מרחק מחיישן האולטרסוני
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מבטיח שהחיישן מתחיל ללא פולס
  delayMicroseconds(2); // מכין את פולס המדידה

  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH); // שולח פולס מדידה לחיישן
  delayMicroseconds(10); // יוצר פולס קצר לחיישן המרחק

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW); // מבטיח שהחיישן מתחיל ללא פולס

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
float readAverageDistanceCm() { // מחשב ממוצע מרחק לאישור חפץ
  float totalDistance = 0; // צובר את המרחקים התקינים
  int validReadings = 0; // סופר כמה דגימות היו תקינות

  for (int i = 0; i < ULTRASONIC_CONFIRM_SAMPLES; i++) { // מבצע כמה דגימות מרחק לאימות
    float distance = readDistanceCm(); // קורא מרחק רגעי מהחיישן

    if (distance >= 0) { // אם הדגימה תקינה ונכנסת לממוצע
      totalDistance += distance; // מוסיף דגימה תקינה לחישוב הממוצע
      validReadings++; // סופר דגימת מרחק תקינה
    }

    if (i < ULTRASONIC_CONFIRM_SAMPLES - 1) { // אם צריך להמתין לדגימה הבאה
      delay(ULTRASONIC_SAMPLE_DELAY_MS); // שומר על תזמון התהליך
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
bool confirmObjectByAverageDistance() { // מאשר שיש חפץ לפני צילום
  float averageDistance = readAverageDistanceCm(); // שומר את ממוצע המרחק לפני צילום

  if (averageDistance < 0) { // אם אין קריאת מרחק אמינה
    showOledMessage("ULTRASONIC ERROR", "No valid reading"); // מציג שגיאת חיישן מרחק
    beepLong(); // מסמן שגיאה למשתמש
    return false; // מחזיר שהפעולה נכשלה
  }

  if (averageDistance > OBJECT_DETECTION_THRESHOLD_CM) { // אם החפץ כבר לא קרוב מספיק
    showOledMessage("NO OBJECT", "Average:", String(averageDistance) + " cm"); // מציג שהחפץ לא אושר לצילום
    beepLong(); // מסמן שגיאה למשתמש
    return false; // מחזיר שהפעולה נכשלה
  }

  return true; // מחזיר שהפעולה הצליחה
}


// =========================================================
// ניקוי תקשורת המצלמה
// מנקה תשובות ישנות מהמצלמה לפני שליחת פקודה חדשה
// =========================================================
void clearCameraBuffer() { // מנקה תשובות ישנות מהמצלמה
  while (Serial2.available()) { // בודק אם המצלמה שלחה תשובה
    Serial2.read(); // מנקה תו ישן מתקשורת המצלמה
  }
}


// =========================================================
// שליחת פקודה למצלמה
// שולח פקודה ל-מצלמת הזיהוי
// =========================================================
void sendCommandToCamera(String command) { // שולח פקודה למצלמה דרך התקשורת
  clearCameraBuffer(); // מוחק תשובות ישנות לפני צילום חדש

  Serial2.println(command); // שולח פקודת צילום למצלמה דרך קו תקשורת
}


// =========================================================
// קריאת תשובת מצלמה
// קורא תשובה תקינה מה-מצלמת הזיהוי
// =========================================================
String readCameraResponse() { // ממתין לתשובת המצלמה
  unsigned long startTime = millis(); // שומר מתי התחילה ההמתנה למצלמה

  while (millis() - startTime < CAMERA_RESPONSE_TIMEOUT_MS) { // ממתין לתשובת המצלמה עד הזמן המוגדר
    if (Serial2.available()) { // בודק אם המצלמה שלחה תשובה
      String response = Serial2.readStringUntil('\n'); // קורא שורת תשובה מהמצלמה
      response.trim(); // מנקה רווחים מתשובת המצלמה

      if (response.length() == 0) { // אם לא התקבלה תשובה שימושית
        continue; // מדלג על תשובה ריקה וממשיך להמתין
      }

      if (response == "PONG") { // אם התקבלה תשובת בדיקה ישנה
        return response; // מחזיר את תשובת המצלמה לעיבוד
      }

      if (response.startsWith("{")) { // אם התקבלה תשובת נתונים תקינה
        return response; // מחזיר את תשובת המצלמה לעיבוד
      }
    }

    delay(20); // מונע קריאה רציפה מדי מהתקשורת
  }

  return ""; // מסמן שלא התקבלה תשובה מהמצלמה
}


// =========================================================
// חילוץ ערך מתשובת נתונים פשוטה
// מוציא ערך טקסטואלי מתוך תשובת נתונים פשוט בלי ספרייה חיצונית
// עובד גם אם יש רווחים:
// דוגמה לשדה חומר
// דוגמה לשדה חומר עם רווח
// =========================================================
String extractStringValue(String jsonText, String key) { // מוציא ערך מתוך תשובת הנתונים
  String keyPattern = "\"" + key + "\""; // בונה את שם השדה לחיפוש בתשובה

  int keyIndex = jsonText.indexOf(keyPattern); // שומר איפה נמצא שם השדה

  if (keyIndex == -1) { // אם שדה הנתונים לא נמצא
    return ""; // מסמן שלא התקבלה תשובה מהמצלמה
  }

  int colonIndex = jsonText.indexOf(":", keyIndex); // שומר את מיקום הנקודתיים בשדה

  if (colonIndex == -1) { // אם מבנה השדה לא תקין
    return ""; // מסמן שלא התקבלה תשובה מהמצלמה
  }

  int firstQuote = jsonText.indexOf("\"", colonIndex); // שומר את תחילת הערך בתשובה

  if (firstQuote == -1) { // אם תחילת הערך לא נמצאה
    return ""; // מסמן שלא התקבלה תשובה מהמצלמה
  }

  int secondQuote = jsonText.indexOf("\"", firstQuote + 1); // שומר את סוף הערך בתשובה

  if (secondQuote == -1) { // אם סוף הערך לא נמצא
    return ""; // מסמן שלא התקבלה תשובה מהמצלמה
  }

  return jsonText.substring(firstQuote + 1, secondQuote); // מחזיר את הערך שנמצא בתשובה
}


bool cameraResponseFailed(String jsonText) { // בודק אם השרת החזיר כשל
  jsonText.toLowerCase(); // מאחד את כתיבת תשובת השרת
  jsonText.replace(" ", ""); // מסיר רווחים לבדיקת כשל

  return jsonText.indexOf("\"success\":false") != -1; // מחזיר אם התשובה מסמנת כשל
}


// =========================================================
// חילוץ קטגוריה מתשובת המצלמה
// מוציא את קטגוריה מהתשובה של המצלמה
// =========================================================
String extractCategoryFromCameraJson(String jsonText) { // מוציא את סוג החומר מהתשובה
  String category = extractStringValue(jsonText, "category"); // שומר את סוג החומר מהשרת

  category.trim(); // מנקה רווחים מסוג החומר
  category.toLowerCase(); // מאחד את כתיבת סוג החומר

  if (category == "plastic" || category == "paper" || category == "metal") { // אם התקבל חומר שניתן למיון
    return category; // מחזיר את סוג החומר למיון
  }

  return "unknown"; // מחזיר שהחומר לא מוכר למערכת
}


// =========================================================
// בקשת צילום מהמצלמה
// שולח צילום למצלמה, מקבל תשובת נתונים, ומפעיל מיון לפי קטגוריה
// =========================================================
void requestCaptureFromCamera(bool requireUltrasonicConfirmation) { // מבקש צילום ומפעיל מיון לפי התשובה
  if (requireUltrasonicConfirmation && !confirmObjectByAverageDistance()) { // אם החיישן לא אישר שיש חפץ
    returnToReadyState(1200); // מחזיר את הפח להמתנה אחרי ביטול צילום
    return; // עוצר את הפעולה בנקודה זו
  }

  showOledMessage("CAPTURING", "Please wait..."); // מציג שהמערכת מצלמת וממתינה לזיהוי
  beepShort(); // נותן משוב קולי קצר

  sendCommandToCamera("CAPTURE"); // מבקש מהמצלמה לצלם ולזהות

  String response = readCameraResponse(); // שומר את תשובת המצלמה או השרת

  if (response.length() == 0) { // אם לא התקבלה תשובה שימושית
    showOledMessage("CAPTURE FAILED", "No camera response"); // מציג שהמצלמה לא החזירה תשובה
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר את הפעולה בנקודה זו
  }

  if (!response.startsWith("{")) { // אם תשובת המצלמה אינה נתונים תקינים
    showOledMessage("CAPTURE FAILED", "Invalid response"); // מציג שתשובת המצלמה לא תקינה
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר את הפעולה בנקודה זו
  }

  if (cameraResponseFailed(response)) { // אם השרת הודיע על כשל זיהוי
    showOledMessage("SERVER FAILED", "Try again"); // מציג שכשל הזיהוי הגיע מהשרת
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר את הפעולה בנקודה זו
  }

  String category = extractCategoryFromCameraJson(response); // שומר את סוג החומר מהשרת

  if (category == "unknown") { // אם החומר לא זוהה כמיון תקין
    showUnknownScreen(); // מציג שהחומר לא ניתן למיון
    beepLong(); // מסמן שגיאה למשתמש

    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר את הפעולה בנקודה זו
  }

  showCategoryScreen(category); // מציג את סוג החומר לפני מיון
  beepDouble(); // נותן משוב לפני מיון

  sortToCategory(category); // מפעיל מיון פיזי לפי סוג החומר
}


// =========================================================
// מעבר למצב מוכן
// =========================================================
void moveToReadyPosition() { // מחזיר את הסרווים למצב המתנה
  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת העליונה לפני מיון
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  bottomServo.write(BOTTOM_CENTER_ANGLE); // מחזיר את שער המיון למרכז
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך
}


// =========================================================
// חזרה למצב מוכן
// חזרה למצב מוכן
// =========================================================
void returnToReadyState(unsigned long waitBeforeReadyScreenMs) { // מחזיר את הפח למסך המתנה
  moveToReadyPosition(); // מחזיר דלת ושער למצב התחלה

  if (waitBeforeReadyScreenMs > 0) { // אם צריך להציג שגיאה לפני חזרה
    delay(waitBeforeReadyScreenMs); // שומר על תזמון התהליך
  }

  showReadyScreen(); // מציג שהפח מוכן לעבודה
}


// =========================================================
// בחירת זווית תחתונה לפי קטגוריה
// =========================================================
int getBottomAngleForCategory(String category) { // בוחר זווית שער לפי סוג החומר
  category.trim(); // מנקה רווחים מסוג החומר
  category.toLowerCase(); // מאחד את כתיבת סוג החומר

  if (category == "plastic") { // אם החומר הוא פלסטיק
    return PLASTIC_ANGLE; // מחזיר את זווית הפלסטיק
  }

  if (category == "paper") { // אם החומר הוא נייר
    return PAPER_ANGLE; // מחזיר את זווית הנייר
  }

  if (category == "metal") { // אם החומר הוא מתכת
    return METAL_ANGLE; // מחזיר את זווית המתכת
  }

  return BOTTOM_CENTER_ANGLE; // מחזיר את השער למרכז כברירת מחדל
}


// =========================================================
// מיון לפי קטגוריה
// =========================================================
void sortToCategory(String category) { // מבצע את תהליך המיון הפיזי
  category.trim(); // מנקה רווחים מסוג החומר
  category.toLowerCase(); // מאחד את כתיבת סוג החומר

  if (category != "plastic" && category != "paper" && category != "metal") { // אם אין יעד מיון חוקי
    showUnknownScreen(); // מציג שהחומר לא ניתן למיון
    beepLong(); // מסמן שגיאה למשתמש
    returnToReadyState(1500); // מחזיר את הפח להמתנה אחרי שגיאה
    return; // עוצר את הפעולה בנקודה זו
  }

  int targetBottomAngle = getBottomAngleForCategory(category); // שומר את זווית שער המיון שנבחרה

  showOledMessage("SORTING", "Category:", category); // מציג את סוג החומר לפני תנועת השער

  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת העליונה לפני מיון
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  delay(WAIT_BEFORE_START_MS); // שומר על תזמון התהליך

  showOledMessage("SORTING", "Moving gate...", category); // מציג שהשער זז לפח המתאים
  bottomServo.write(targetBottomAngle); // מכוון את שער המיון לפח הנכון
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  delay(WAIT_AFTER_BOTTOM_MOVE_MS); // שומר על תזמון התהליך

  showOledMessage("SORTING", "Opening door...", category); // מציג שהדלת נפתחת להפלת החפץ
  topServo.write(TOP_OPEN_ANGLE); // פותח את הדלת כדי להפיל את החפץ
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  delay(WAIT_WHILE_TOP_OPEN_MS); // שומר על תזמון התהליך

  showOledMessage("SORTING", "Closing door...", category); // מציג שהדלת נסגרת אחרי ההפלה
  topServo.write(TOP_CLOSED_ANGLE); // סוגר את הדלת העליונה לפני מיון
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  delay(WAIT_AFTER_TOP_CLOSE_MS); // שומר על תזמון התהליך

  showOledMessage("SORTING", "Returning center..."); // מציג שהשער חוזר למרכז
  bottomServo.write(BOTTOM_CENTER_ANGLE); // מחזיר את שער המיון למרכז
  delay(SERVO_SMALL_DELAY_MS); // שומר על תזמון התהליך

  showDoneScreen(); // מציג שהמיון הסתיים
  beepSuccess(); // מסמן שהמערכת עלתה בהצלחה

  delay(1500); // שומר על תזמון התהליך
  showReadyScreen(); // מציג שהפח מוכן לעבודה
}


// =========================================================
// בדיקת חיישן וטיפול בחפץ
// במצב אוטומטי, בודק אם יש חפץ
// =========================================================
void checkUltrasonicAndHandleObject() { // בודק אוטומטית אם הוכנס חפץ
  unsigned long now = millis(); // שומר את זמן הבדיקה הנוכחית

  if (now - lastDetectionTime < DETECTION_COOLDOWN_MS) { // אם עדיין ממתינים לפני זיהוי חדש
    return; // עוצר את הפעולה בנקודה זו
  }

  float distance = readDistanceCm(); // קורא מרחק רגעי מהחיישן

  if (distance < 0) { // אם החיישן לא החזיר קריאה תקינה
    return; // עוצר את הפעולה בנקודה זו
  }

  if (distance <= OBJECT_DETECTION_THRESHOLD_CM) { // אם חפץ נמצא בטווח הזיהוי
    showOledMessage("OBJECT DETECTED", "Distance:", String(distance) + " cm"); // מציג שחיישן המרחק זיהה חפץ
    beepShort(); // נותן משוב קולי קצר

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
  showOledMessage("OBJECT DETECTED", "Stabilizing..."); // מציג שהחפץ מתייצב לפני צילום
  delay(OBJECT_SETTLE_DELAY_MS); // שומר על תזמון התהליך

  requestCaptureFromCamera(true); // מבקש צילום אחרי אישור חפץ
}
