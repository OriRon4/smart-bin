#include <WiFi.h> // ספרייה לחיבור המצלמה לרשת
#include <HTTPClient.h> // ספרייה לשליחת תמונה לשרת
#include "esp_camera.h" // ספרייה להפעלת המצלמה

// =========================================================
// רשת ושרת
// =========================================================
const char* WIFI_SSID = "College"; // שם הרשת שאליה המצלמה מתחברת
const char* WIFI_PASSWORD = "Amal1@st"; // סיסמת הרשת של המצלמה

// לשנות ל-כתובת רשת של המחשב שעליו רץ שרת הזיהוי
const char* SERVER_URL = "http://10.0.0.231:5000/classify"; // כתובת השרת שמקבל את התמונה


// =========================================================
// פלאש
// =========================================================
#define USE_FLASH true // קובע אם להשתמש בפלאש בצילום
#define FLASH_LED_PIN 4 // פין הפלאש במודול המצלמה
#define FLASH_STABILIZE_DELAY_MS 600 // זמן ייצוב התאורה לפני צילום


// =========================================================
// הגדרות בקשת רשת
// =========================================================
#define SERVER_SEND_RETRIES 3 // מספר ניסיונות שליחה לשרת
#define SERVER_RETRY_DELAY_MS 1200 // המתנה בין ניסיונות שליחה
#define HTTP_TIMEOUT_MS 20000 // זמן מקסימלי לבקשת השרת


// =========================================================
// מצב מצלמה
// =========================================================
bool cameraReady = false; // שומר אם המצלמה אותחלה בהצלחה


// =========================================================
// פיני מודול המצלמה
// =========================================================
#define PWDN_GPIO_NUM     32 // פין כיבוי של מודול המצלמה
#define RESET_GPIO_NUM    -1 // פין איפוס של מודול המצלמה
#define XCLK_GPIO_NUM      0 // פין שעון ראשי של המצלמה
#define SIOD_GPIO_NUM     26 // פין נתוני בקרה של המצלמה
#define SIOC_GPIO_NUM     27 // פין שעון בקרה של המצלמה

#define Y9_GPIO_NUM       35 // פין נתוני תמונה מהמצלמה
#define Y8_GPIO_NUM       34 // פין נתוני תמונה מהמצלמה
#define Y7_GPIO_NUM       39 // פין נתוני תמונה מהמצלמה
#define Y6_GPIO_NUM       36 // פין נתוני תמונה מהמצלמה
#define Y5_GPIO_NUM       21 // פין נתוני תמונה מהמצלמה
#define Y4_GPIO_NUM       19 // פין נתוני תמונה מהמצלמה
#define Y3_GPIO_NUM       18 // פין נתוני תמונה מהמצלמה
#define Y2_GPIO_NUM        5 // פין נתוני תמונה מהמצלמה

#define VSYNC_GPIO_NUM    25 // פין סנכרון אנכי של התמונה
#define HREF_GPIO_NUM     23 // פין סנכרון שורה של התמונה
#define PCLK_GPIO_NUM     22 // פין שעון פיקסלים של המצלמה


// =========================================================
// חיבור לרשת
// =========================================================
bool connectToWiFi() { // מחבר את המצלמה לרשת
  WiFi.mode(WIFI_STA); // מגדיר את המצלמה כלקוח רשת
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // מתחבר לרשת כדי לשלוח תמונות לשרת

  int attempts = 0; // סופר ניסיונות חיבור לרשת

  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // מנסה להתחבר לרשת לפני צילום
    delay(500); // נותן ל-WiFi זמן להשלים ניסיון חיבור
    attempts++; // סופר ניסיון חיבור נוסף לרשת
  }

  if (WiFi.status() == WL_CONNECTED) { // אם המצלמה מחוברת לרשת
    return true; // מחזיר שהפעולה הצליחה
  }

  return false; // מחזיר שהפעולה נכשלה
}


// =========================================================
// אתחול מצלמה
// =========================================================
bool initCamera() { // מאתחל את מודול המצלמה
  camera_config_t config; // יוצר מבנה הגדרות למצלמה

  config.ledc_channel = LEDC_CHANNEL_0; // מגדיר ערוץ שעון למצלמה
  config.ledc_timer = LEDC_TIMER_0; // מגדיר טיימר שעון למצלמה

  config.pin_d0 = Y2_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d1 = Y3_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d2 = Y4_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d3 = Y5_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d4 = Y6_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d5 = Y7_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d6 = Y8_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות
  config.pin_d7 = Y9_GPIO_NUM; // מחבר קו נתוני תמונה להגדרות

  config.pin_xclk = XCLK_GPIO_NUM; // מחבר את שעון המצלמה להגדרות
  config.pin_pclk = PCLK_GPIO_NUM; // מחבר את שעון הפיקסלים להגדרות
  config.pin_vsync = VSYNC_GPIO_NUM; // מחבר סנכרון אנכי להגדרות
  config.pin_href = HREF_GPIO_NUM; // מחבר סנכרון שורה להגדרות

  config.pin_sccb_sda = SIOD_GPIO_NUM; // מחבר נתוני בקרה להגדרות
  config.pin_sccb_scl = SIOC_GPIO_NUM; // מחבר שעון בקרה להגדרות

  config.pin_pwdn = PWDN_GPIO_NUM; // מחבר פין כיבוי להגדרות
  config.pin_reset = RESET_GPIO_NUM; // מחבר פין איפוס להגדרות

  config.xclk_freq_hz = 20000000; // קובע תדר עבודה למצלמה
  config.pixel_format = PIXFORMAT_JPEG; // קובע צילום בפורמט תמונה דחוס

  if (psramFound()) { // אם יש זיכרון נוסף לתמונה טובה יותר
    config.frame_size = FRAMESIZE_VGA; // בוחר רזולוציה גבוהה כשיש זיכרון
    config.jpeg_quality = 12; // קובע איכות תמונה טובה יותר
    config.fb_count = 2; // משתמש בשני מאגרי תמונה
  } else { // משתמש בהגדרות קלות יותר ללא זיכרון נוסף
    config.frame_size = FRAMESIZE_QVGA; // בוחר רזולוציה קלה ללא זיכרון נוסף
    config.jpeg_quality = 15; // מוריד מעט איכות כדי לחסוך זיכרון
    config.fb_count = 1; // משתמש במאגר תמונה אחד
  }

  esp_err_t err = esp_camera_init(&config); // מנסה להפעיל את המצלמה

  if (err != ESP_OK) { // אם אתחול המצלמה נכשל
    return false; // מחזיר שהפעולה נכשלה
  }

  return true; // מחזיר שהפעולה הצליחה
}


// =========================================================
// פלאש
// =========================================================
void turnFlashOn() { // מדליק תאורה לפני צילום
  if (USE_FLASH) { // אם מוגדר להשתמש בפלאש
    digitalWrite(FLASH_LED_PIN, HIGH); // מדליק פלאש לפני צילום
    delay(FLASH_STABILIZE_DELAY_MS); // ממתין לייצוב התאורה לפני הצילום
  }
}


void turnFlashOff() { // מכבה תאורה אחרי צילום
  if (USE_FLASH) { // אם מוגדר להשתמש בפלאש
    digitalWrite(FLASH_LED_PIN, LOW); // מכבה פלאש אחרי צילום
  }
}


// =========================================================
// שליחת תמונה לשרת
// מחזיר את תשובת הנתונים שהשרת החזיר
// =========================================================
String sendPhotoBufferToServer(camera_fb_t* fb) { // שולח את התמונה לשרת הזיהוי
  HTTPClient http; // יוצר אובייקט לשליחת התמונה לשרת

  http.begin(SERVER_URL); // פותח חיבור לשרת הזיהוי
  http.setTimeout(HTTP_TIMEOUT_MS); // מגביל זמן המתנה לשרת

  String boundary = "----SmartBinBoundary"; // יוצר גבול לבקשת העלאת התמונה
  String contentType = "multipart/form-data; boundary=" + boundary; // מגדיר בקשת רשת עם תמונה

  http.addHeader("Content-Type", contentType); // מגדיר שהבקשה כוללת תמונה

  String bodyStart = // בונה את תחילת בקשת העלאת התמונה
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"image\"; filename=\"esp32cam.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = // בונה את סוף בקשת העלאת התמונה
    "\r\n--" + boundary + "--\r\n";

  int totalLength = bodyStart.length() + fb->len + bodyEnd.length(); // מחשב גודל מלא של הבקשה

  uint8_t* requestBody = (uint8_t*)malloc(totalLength); // מקצה זיכרון לבקשת התמונה

  if (!requestBody) { // אם אין מספיק זיכרון לשליחת התמונה
    http.end(); // סוגר את חיבור השרת
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"ESP32-CAM memory allocation failed\"}"; // מחזיר כשל בפורמט שהבקר מבין
  }

  memcpy(requestBody, bodyStart.c_str(), bodyStart.length()); // מוסיף את פתיחת בקשת התמונה
  memcpy(requestBody + bodyStart.length(), fb->buf, fb->len); // מוסיף את נתוני התמונה לבקשה
  memcpy(requestBody + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length()); // מוסיף את סוף בקשת התמונה

  int httpResponseCode = http.POST(requestBody, totalLength); // שולח את תמונת החפץ לשרת

  free(requestBody); // משחרר זיכרון אחרי השליחה

  if (httpResponseCode == 200) { // אם השרת קיבל את התמונה בהצלחה
    String response = http.getString(); // קורא את תשובת הזיהוי מהשרת
    response.trim(); // מנקה רווחים מתשובת השרת
    http.end(); // סוגר את חיבור השרת

    if (response.startsWith("{")) { // אם התקבלה תשובה שנראית כמו JSON
      return response; // מחזיר את תשובת השרת לבקר הראשי
    }

    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Server response was not valid JSON\"}"; // מחזיר כשל בפורמט שהבקר מבין
  }

  http.end(); // סוגר את חיבור השרת

  return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"HTTP request failed\"}"; // מחזיר כשל בפורמט שהבקר מבין
}


// =========================================================
// צילום, שליחה והחזרת תשובה
// =========================================================
String captureSendAndReturnJson() { // מצלם ומחזיר תשובת זיהוי
  if (!cameraReady) { // אם המצלמה לא מוכנה לצילום
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Camera not initialized\"}"; // מחזיר כשל בפורמט שהבקר מבין
  }

  if (WiFi.status() != WL_CONNECTED) { // אם צריך להתחבר מחדש לרשת
    bool wifiOk = connectToWiFi(); // שומר אם החיבור לרשת הצליח

    if (!wifiOk) { // אם המצלמה לא הצליחה להתחבר לרשת
      return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"WiFi not connected\"}"; // מחזיר כשל בפורמט שהבקר מבין
    }
  }

  turnFlashOn(); // מדליק תאורה לצילום ברור

// צילום חימום לייצוב המצלמה לפני התמונה המרכזית
  camera_fb_t* warmupFb = esp_camera_fb_get(); // מקבל תמונת חימום מהמצלמה

  if (warmupFb) { // אם התקבלה תמונת חימום
    esp_camera_fb_return(warmupFb); // משחרר את תמונת החימום
  }

  delay(200); // נותן למצלמה להתייצב אחרי חימום

  camera_fb_t* fb = esp_camera_fb_get(); // מקבל תמונה מהמצלמה

  turnFlashOff(); // מכבה תאורה אחרי הצילום

  if (!fb) { // אם הצילום מהמצלמה נכשל
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Camera capture failed\"}"; // מחזיר כשל בפורמט שהבקר מבין
  }

  String serverResponse = ""; // שומר את תשובת השרת לזיהוי

  for (int attempt = 1; attempt <= SERVER_SEND_RETRIES; attempt++) { // מנסה לשלוח את התמונה כמה פעמים
    serverResponse = sendPhotoBufferToServer(fb); // שומר את תשובת השרת לניסיון הנוכחי

    if (serverResponse.startsWith("{") && serverResponse.indexOf("\"success\":true") != -1) { // אם השרת החזיר זיהוי מוצלח
      break; // עוצר ניסיונות כי הזיהוי הצליח
    }

    if (attempt < SERVER_SEND_RETRIES) { // אם נשאר ניסיון שליחה נוסף
      delay(SERVER_RETRY_DELAY_MS); // ממתין לפני ניסיון שליחה נוסף לשרת
    }
  }

  esp_camera_fb_return(fb); // משחרר את התמונה אחרי שליחה

  if (serverResponse.length() == 0) { // אם השרת לא החזיר תשובה
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"No response from server\"}"; // מחזיר כשל בפורמט שהבקר מבין
  }

  return serverResponse; // מחזיר לבקר את תשובת השרת
}


// =========================================================
// טיפול בפקודת הבקר הראשי
// =========================================================
void handleMainCommand(String command) { // מטפל בפקודה מהבקר הראשי
  command.trim(); // מנקה רווחים מהפקודה שהתקבלה

  if (command.length() == 0) { // אם התקבלה פקודה ריקה
    return; // מתעלם מפקודה ריקה
  }

  if (command == "CAPTURE") { // אם הבקר ביקש צילום וזיהוי
    String jsonResponse = captureSendAndReturnJson(); // שומר את התשובה שתישלח לבקר

    jsonResponse.trim(); // מנקה רווחים מתשובת הזיהוי

    if (!jsonResponse.startsWith("{")) { // אם תשובת הזיהוי אינה נראית כמו JSON
      jsonResponse = "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Invalid JSON response\"}"; // מייצר תשובת כשל כשהפורמט לא תקין
    }

    Serial.println(jsonResponse); // מחזיר לבקר הראשי תשובת זיהוי אחת
    return; // עוצר כי פקודת הצילום כבר טופלה
  }
}


// =========================================================
// אתחול ראשוני
// =========================================================
void setup() { // מכין את רכיבי המערכת להפעלה
  Serial.begin(115200); // פותח UART מול הבקר הראשי לקבלת CAPTURE והחזרת JSON
  delay(1000); // נותן לרכיבים להתייצב אחרי ההפעלה

  pinMode(FLASH_LED_PIN, OUTPUT); // מגדיר את הפלאש כפלט
  digitalWrite(FLASH_LED_PIN, LOW); // מוודא שהפלאש כבוי בתחילת העבודה

  connectToWiFi(); // מחבר את המצלמה לרשת בתחילת העבודה
  cameraReady = initCamera(); // שומר אם המצלמה מוכנה לצילום

// לא מדפיסים כלום כאן במצב עבודה רגיל.
// הבקר הראשי יקבל תשובה רק כשישלח צילום.
}


// =========================================================
// לולאת עבודה
// =========================================================
void loop() { // מריץ את עבודת המערכת ברצף
  if (Serial.available()) { // בודק אם הבקר שלח פקודה
    String command = Serial.readStringUntil('\n'); // קורא פקודה מהבקר הראשי
    handleMainCommand(command); // מפעיל טיפול בפקודת צילום מהבקר
  }

  delay(20); // מונע קריאה רציפה מדי מהתקשורת
}
