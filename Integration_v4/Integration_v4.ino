/**************************************************************************
 * ESP32-S3 Dual-Core Stitch Cam with Gallery
 * GZIP HTML Version (No Raw HTML Stored in RAM)
 *
 * Core 0: Camera + LCD + Buttons + LED + SD
 * Core 1: WiFi + HTTP server
 **************************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <ArduCAM.h>
#include "memorysaver.h"

#ifdef swap
  #undef swap
#endif

#include <TJpg_Decoder.h>
#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// ====================== GZIP HTML ======================
#include "index_html_gz.h"
#include "gallery_html_gz.h"

// ====================== WiFi ======================
const char* ssid     = "KellyiPhone";
const char* password = "kelly200636";

WebServer server(80);

// ====================== Pins ======================
static const int PIN_SPI_SCK  = 10;
static const int PIN_SPI_MISO = 11;
static const int PIN_SPI_MOSI = 12;
static const int PIN_SD_CS    = 14;
static const int PIN_CAM_CS   = 13;
static const int PIN_SDA      = 9;
static const int PIN_SCL      = 8;
static const int PIN_BUTTON1  = 1;
static const int PIN_BUTTON2  = 45;
static const int PIN_LED_RED   = 2;
static const int PIN_LED_GREEN = 42;
static const int PIN_LED_BLUE  = 41;

// ====================== Buffers ===================
#define MAX_JPEG_SIZE 32768
uint8_t jpegBuf[MAX_JPEG_SIZE];
volatile uint32_t jpegLen = 0;

static const uint16_t FRAME_W = 320;
static const uint16_t FRAME_H = 240;
static const uint32_t FRAME_BYTES = FRAME_W * FRAME_H * 2;
uint8_t frameBuf[FRAME_BYTES];

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// ====================== State =====================
bool sdCardAvailable = false;

enum CaptureMode {
  MODE_INSTANT,
  MODE_COUNTDOWN
};
volatile CaptureMode currentMode = MODE_INSTANT;

volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile uint32_t lastButton1Time = 0;
volatile uint32_t lastButton2Time = 0;
const uint32_t DEBOUNCE_DELAY = 200;

volatile bool webCaptureRequested = false;
volatile bool webCountdownRequested = false;

String lastCaptureStatus = "Idle";
uint32_t totalPhotosSaved = 0;
uint32_t photoCounter = 1;

TaskHandle_t cameraTaskHandle = NULL;

// ============ Forward Declarations ================
void initCamera();
void initSDCard();
void initLED();
bool captureJpegToBuffer();
bool decodeJpegToRGB565();
void streamFrameToLcd_Optimized(const uint8_t *src);
bool savePhoto();
void showSaveMessage();
void handleCaptureInstant();
void handleCaptureCountdown();
void setLED(bool red, bool green, bool blue);
void IRAM_ATTR button1ISR();
void IRAM_ATTR button2ISR();
void cameraTask(void* parameter);

// Web handlers
void handleRoot();
void handleGallery();
void handleCapture();
void handleToggleMode();
void handleCountdownStart();
void handleStatus();
void handleStream();
void handlePhotoList();
void handlePhoto();
void handleDeletePhoto();

// ====================== TJpgDec Callback ==========
bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (uint16_t row = 0; row < h; row++) {
    if ((y + row) >= FRAME_H) break;
    for (uint16_t col = 0; col < w; col++) {
      if ((x + col) >= FRAME_W) break;
      uint32_t dstIdx = ((y + row) * FRAME_W + (x + col)) * 2;
      uint16_t pixel = bitmap[row * w + col];
      frameBuf[dstIdx] = pixel >> 8;
      frameBuf[dstIdx + 1] = pixel & 0xFF;
    }
  }
  return true;
}

// ====================== Button ISRs ===============
void IRAM_ATTR button1ISR() {
  uint32_t now = millis();
  if (now - lastButton1Time > DEBOUNCE_DELAY) {
    button1Pressed = true;
    lastButton1Time = now;
  }
}
void IRAM_ATTR button2ISR() {
  uint32_t now = millis();
  if (now - lastButton2Time > DEBOUNCE_DELAY) {
    button2Pressed = true;
    lastButton2Time = now;
  }
}

// ====================== setup() ===================
void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON1), button1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON2), button2ISR, FALLING);

  initLED();

  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_CAM_CS, HIGH);

  Wire.begin(PIN_SDA, PIN_SCL);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  initSDCard();
  initCamera();

  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  LCD_Clear(BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  server.on("/", handleRoot);
  server.on("/gallery", handleGallery);
  server.on("/capture", handleCapture);
  server.on("/toggle", handleToggleMode);
  server.on("/countdown_start", handleCountdownStart);
  server.on("/status", handleStatus);
  server.on("/stream", handleStream);
  server.on("/photos", handlePhotoList);
  server.on("/photo", handlePhoto);
  server.on("/delete", handleDeletePhoto);
  server.begin();

  xTaskCreatePinnedToCore(cameraTask, "CameraTask", 8192, NULL, 1, &cameraTaskHandle, 0);
}

// ====================== loop() (Core 1) ===========
void loop() {
  server.handleClient();
  delay(1);
}

// ====================== cameraTask (Core 0) =======
void cameraTask(void* parameter) {
  for (;;) {
    if (webCaptureRequested) {
      webCaptureRequested = false;
      if (currentMode == MODE_INSTANT) handleCaptureInstant();
    }

    if (webCountdownRequested) {
      webCountdownRequested = false;
      if (currentMode == MODE_COUNTDOWN) handleCaptureCountdown();
    }

    if (button2Pressed) {
      button2Pressed = false;
      currentMode = (currentMode == MODE_INSTANT) ? MODE_COUNTDOWN : MODE_INSTANT;
    }

    if (button1Pressed) {
      button1Pressed = false;
      if (currentMode == MODE_INSTANT) handleCaptureInstant();
      else handleCaptureCountdown();
    }

    if (!captureJpegToBuffer()) { vTaskDelay(5); continue; }
    if (!decodeJpegToRGB565())  { vTaskDelay(5); continue; }

    streamFrameToLcd_Optimized(frameBuf);
    vTaskDelay(1);
  }
}

// ====================== Web Handlers ==============

// ▼▼▼ 100% FIXED — SERVE GZIP HTML ▼▼▼

void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html",
    (const char*)index_html_gz,
    index_html_gz_len
  );
}

void handleGallery() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html",
    (const char*)gallery_html_gz,
    gallery_html_gz_len
  );
}

// ▲▲▲ END OF FIX — NO RAW HTML ANYWHERE ▲▲▲

void handleCapture() {
  webCaptureRequested = true;
  server.send(200, "text/plain", "OK");
}
void handleToggleMode() {
  button2Pressed = true;
  server.send(200, "text/plain", "OK");
}
void handleCountdownStart() {
  webCountdownRequested = true;
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String mode = (currentMode == MODE_INSTANT) ? "Instant" : "Countdown";
  String json = "{\"mode\":\"" + mode + "\",\"status\":\"" + lastCaptureStatus + "\",\"photos\":" + totalPhotosSaved + "}";
  server.send(200, "application/json", json);
}

void handleStream() {
  if (jpegLen == 0 || jpegLen > MAX_JPEG_SIZE) {
    server.send(503, "text/plain", "No frame");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)jpegBuf, jpegLen);
}

void handlePhotoList() {
  if (!sdCardAvailable) {
    server.send(200, "application/json", "{\"photos\":[]}");
    return;
  }

  String json = "{\"photos\":[";
  File root = SD.open("/photos");
  bool first = true;
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      if (!first) json += ",";
      json += "\"" + String(f.name()) + "\"";
      first = false;
    }
    f.close();
    f = root.openNextFile();
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handlePhoto() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Missing file"); return; }

  String path = "/photos/" + server.arg("file");
  if (!SD.exists(path)) { server.send(404, "text/plain", "Not found"); return; }

  File f = SD.open(path);
  server.streamFile(f, "image/jpeg");
  f.close();
}

void handleDeletePhoto() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "Missing file"); return; }
  String path = "/photos/" + server.arg("file");

  if (SD.remove(path)) {
    if (totalPhotosSaved > 0) totalPhotosSaved--;
    server.send(200, "text/plain", "Deleted");
  } else {
    server.send(500, "text/plain", "Failed");
  }
}

// ====================== Capture Functions =========

void handleCaptureInstant() {
  lastCaptureStatus = "Capturing...";
  if (sdCardAvailable) {
    showSaveMessage();
    savePhoto();
    lastCaptureStatus = "Saved!";
    delay(500);
  }
  lastCaptureStatus = "Idle";
}

void handleCaptureCountdown() {
  lastCaptureStatus = "Countdown...";
  for (int s = 3; s >= 1; s--) {
    setLED(true, false, false);
    delay(300);
    setLED(false, false, false);
    delay(300);
  }

  captureJpegToBuffer();
  decodeJpegToRGB565();
  streamFrameToLcd_Optimized(frameBuf);

  if (sdCardAvailable) {
    setLED(false, true, false);
    showSaveMessage();
    savePhoto();
    delay(500);
  }

  setLED(false, false, false);
  lastCaptureStatus = "Idle";
}

// ====================== LED =======================

void initLED() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  setLED(false, false, false);
}
void setLED(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_RED, r ? LOW : HIGH);
  digitalWrite(PIN_LED_GREEN, g ? LOW : HIGH);
  digitalWrite(PIN_LED_BLUE, b ? LOW : HIGH);
}

void showSaveMessage() {
  LCD_Clear(BLACK);
  Paint_DrawString_EN(100, 160, "SAVING...", &Font24, BLACK, WHITE);
}

// ====================== SD Card ==================

bool savePhoto() {
  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.exists("/photos")) SD.mkdir("/photos");

  char name[32];
  snprintf(name, sizeof(name), "/photos/photo_%d.jpg", photoCounter++);

  File f = SD.open(name, FILE_WRITE);
  if (!f) return false;

  f.write(jpegBuf, jpegLen);
  f.close();
  totalPhotosSaved++;
  return true;
}

void initSDCard() {
  digitalWrite(PIN_CAM_CS, HIGH);
  if (!SD.begin(PIN_SD_CS)) {
    sdCardAvailable = false;
    return;
  }

  sdCardAvailable = true;
  if (!SD.exists("/photos")) SD.mkdir("/photos");

  File root = SD.open("/photos");
  File f = root.openNextFile();
  while (f) {
    totalPhotosSaved++;
    f.close();
    f = root.openNextFile();
  }
}

// ====================== Camera ====================

void initCamera() {
  digitalWrite(PIN_SD_CS, HIGH);

  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  SPI.endTransaction();

  delay(200);
}

bool captureJpegToBuffer() {
  digitalWrite(PIN_SD_CS, HIGH);

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  SPI.endTransaction();

  uint32_t start = millis();
  while (millis() - start < 5000) {
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
      SPI.endTransaction();
      break;
    }
    SPI.endTransaction();
    delay(5);
  }

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  uint32_t len = myCAM.read_fifo_length();
  SPI.endTransaction();

  if (len == 0 || len > MAX_JPEG_SIZE) return false;

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  for (uint32_t i = 0; i < len; i++) jpegBuf[i] = SPI.transfer(0x00);
  myCAM.CS_HIGH();
  SPI.endTransaction();

  jpegLen = len;
  return jpegBuf[0] == 0xFF && jpegBuf[1] == 0xD8;
}

bool decodeJpegToRGB565() {
  memset(frameBuf, 0, FRAME_BYTES);
  uint16_t w, h;
  if (TJpgDec.getJpgSize(&w, &h, jpegBuf, jpegLen) != JDR_OK) return false;
  return TJpgDec.drawJpg(0, 0, jpegBuf, jpegLen) == JDR_OK;
}

void streamFrameToLcd_Optimized(const uint8_t *src) {
  uint8_t *dmaBuf = getDMABuffer();
  const uint16_t LCD_W = 240;
  const uint16_t LCD_H = 320;

  LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
  DEV_SPI_Write_Bulk_Start();

  for (uint16_t y = 0; y < LCD_H; y++) {
    uint16_t ptr = 0;

    for (uint16_t x = 0; x < LCD_W; x++) {
      uint16_t srcRow = LCD_W - 1 - x;
      uint16_t srcCol = y;
      uint32_t idx = (srcRow * FRAME_W + srcCol) * 2;

      dmaBuf[ptr++] = src[idx];
      dmaBuf[ptr++] = src[idx + 1];

      if (ptr >= DMA_BUFFER_SIZE) {
        DEV_SPI_Write_Bulk_Data(dmaBuf, ptr);
        ptr = 0;
      }
    }

    if (ptr > 0) DEV_SPI_Write_Bulk_Data(dmaBuf, ptr);
  }

  DEV_SPI_Write_Bulk_End();
}
