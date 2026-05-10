#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// =========================================================
// WiFi + Server
// =========================================================
const char* WIFI_SSID = "College";
const char* WIFI_PASSWORD = "Amal1@st";

// לשנות ל-IP של המחשב שעליו רץ שרת Python
const char* SERVER_URL = "http://10.0.0.231:5000/classify";


// =========================================================
// Flash
// =========================================================
#define USE_FLASH true
#define FLASH_LED_PIN 4
#define FLASH_STABILIZE_DELAY_MS 600


// =========================================================
// HTTP Settings
// =========================================================
#define SERVER_SEND_RETRIES 3
#define SERVER_RETRY_DELAY_MS 1200
#define HTTP_TIMEOUT_MS 20000


// =========================================================
// AI Thinker ESP32-CAM Pins
// =========================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// =========================================================
// Connect To WiFi
// =========================================================
bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  return false;
}


// =========================================================
// Init Camera
// =========================================================
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    return false;
  }

  return true;
}


// =========================================================
// Flash
// =========================================================
void turnFlashOn() {
  if (USE_FLASH) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(FLASH_STABILIZE_DELAY_MS);
  }
}


void turnFlashOff() {
  if (USE_FLASH) {
    digitalWrite(FLASH_LED_PIN, LOW);
  }
}


// =========================================================
// Send Photo Buffer To Python Server
// מחזיר את ה-JSON שהשרת החזיר
// =========================================================
String sendPhotoBufferToServer(camera_fb_t* fb) {
  HTTPClient http;

  http.begin(SERVER_URL);
  http.setTimeout(HTTP_TIMEOUT_MS);

  String boundary = "----SmartBinBoundary";
  String contentType = "multipart/form-data; boundary=" + boundary;

  http.addHeader("Content-Type", contentType);

  String bodyStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"image\"; filename=\"esp32cam.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd =
    "\r\n--" + boundary + "--\r\n";

  int totalLength = bodyStart.length() + fb->len + bodyEnd.length();

  uint8_t* requestBody = (uint8_t*)malloc(totalLength);

  if (!requestBody) {
    http.end();
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"ESP32-CAM memory allocation failed\"}";
  }

  memcpy(requestBody, bodyStart.c_str(), bodyStart.length());
  memcpy(requestBody + bodyStart.length(), fb->buf, fb->len);
  memcpy(requestBody + bodyStart.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());

  int httpResponseCode = http.POST(requestBody, totalLength);

  free(requestBody);

  if (httpResponseCode == 200) {
    String response = http.getString();
    response.trim();
    http.end();

    if (response.startsWith("{")) {
      return response;
    }

    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Server response was not valid JSON\"}";
  }

  http.end();

  return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"HTTP request failed\"}";
}


// =========================================================
// Capture, Send To Server, Return JSON
// =========================================================
String captureSendAndReturnJson() {
  if (WiFi.status() != WL_CONNECTED) {
    bool wifiOk = connectToWiFi();

    if (!wifiOk) {
      return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"WiFi not connected\"}";
    }
  }

  turnFlashOn();

  // פריים חימום
  camera_fb_t* warmupFb = esp_camera_fb_get();

  if (warmupFb) {
    esp_camera_fb_return(warmupFb);
  }

  delay(200);

  camera_fb_t* fb = esp_camera_fb_get();

  turnFlashOff();

  if (!fb) {
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Camera capture failed\"}";
  }

  String serverResponse = "";

  for (int attempt = 1; attempt <= SERVER_SEND_RETRIES; attempt++) {
    serverResponse = sendPhotoBufferToServer(fb);

    if (serverResponse.startsWith("{") && serverResponse.indexOf("\"success\":true") != -1) {
      break;
    }

    if (attempt < SERVER_SEND_RETRIES) {
      delay(SERVER_RETRY_DELAY_MS);
    }
  }

  esp_camera_fb_return(fb);

  if (serverResponse.length() == 0) {
    return "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"No response from server\"}";
  }

  return serverResponse;
}


// =========================================================
// Handle Command From Main ESP32
// =========================================================
void handleMainCommand(String command) {
  command.trim();

  if (command.length() == 0) {
    return;
  }

  if (command == "CAPTURE") {
    String jsonResponse = captureSendAndReturnJson();

    jsonResponse.trim();

    if (!jsonResponse.startsWith("{")) {
      jsonResponse = "{\"success\":false,\"category\":\"unknown\",\"confidence\":0,\"reason\":\"Invalid JSON response\"}";
    }

    Serial.println(jsonResponse);
    return;
  }
}


// =========================================================
// setup
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  connectToWiFi();
  initCamera();

  // לא מדפיסים כלום כאן במצב עבודה רגיל.
  // ה-ESP32 הראשי יקבל תשובה רק כשישלח CAPTURE.
}


// =========================================================
// loop
// =========================================================
void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleMainCommand(command);
  }

  delay(20);
}
