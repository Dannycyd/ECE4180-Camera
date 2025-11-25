/**************************************************************************
 * ESP32-S3 Dual-Core Stitch Cam
 *
 * Core 0: Camera + LCD + Buttons + LED + SD (cameraTask)
 * Core 1: WiFi + HTTP server (loop)
 *
 * Based on:
 *   - Code 2: Camera LCD two-mode capture with RGB LED (main logic)
 *   - Code 1: Stitch UI style (HTML / CSS / JS)
 *
 * Features:
 *   - Live streaming on LCD (continuous)
 *   - Web UI on phone:
 *        /                -> fancy Stitch control page
 *        /capture         -> trigger INSTANT capture
 *        /toggle          -> toggle mode (Instant ↔ Countdown)
 *        /countdown_start -> start countdown capture (LED + camera)
 *        /status          -> JSON { mode, status, photos }
 *        /stream          -> latest JPEG frame (for <img> preview)
 **************************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// ---------- Camera / LCD libs ----------
#include <ArduCAM.h>
#include "memorysaver.h"

#ifdef swap
  #undef swap   // Fix ArduCAM macro conflict
#endif

#include <TJpg_Decoder.h>
#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// ====================== WiFi / Server ======================
const char* ssid     = "KellyiPhone";
const char* password = "kelly200636";

WebServer server(80);

// ====================== Pin Definitions ====================
static const int PIN_SPI_SCK  = 10;
static const int PIN_SPI_MISO = 11;
static const int PIN_SPI_MOSI = 12;

static const int PIN_SD_CS    = 14;
static const int PIN_CAM_CS   = 13;

static const int PIN_SDA      = 9;
static const int PIN_SCL      = 8;

static const int PIN_BUTTON1  = 1;   // Capture trigger
static const int PIN_BUTTON2  = 45;  // Mode toggle

static const int PIN_LED_RED   = 2;
static const int PIN_LED_GREEN = 42;
static const int PIN_LED_BLUE  = 41;

// ====================== Buffers ============================
#define MAX_JPEG_SIZE 32768
uint8_t jpegBuf[MAX_JPEG_SIZE];
volatile uint32_t jpegLen = 0;  // last captured frame length

static const uint16_t FRAME_W = 320;
static const uint16_t FRAME_H = 240;
static const uint32_t FRAME_BYTES = FRAME_W * FRAME_H * 2;
uint8_t frameBuf[FRAME_BYTES];

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// ====================== State ==============================
bool sdCardAvailable = false;

enum CaptureMode {
  MODE_INSTANT,
  MODE_COUNTDOWN
};
volatile CaptureMode currentMode = MODE_INSTANT;

// Button flags (from ISR)
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile uint32_t lastButton1Time = 0;
volatile uint32_t lastButton2Time = 0;
const uint32_t DEBOUNCE_DELAY = 200;

// Web-triggered actions (set on Core 1, consumed on Core 0)
volatile bool webCaptureRequested    = false; // for INSTANT
volatile bool webCountdownRequested  = false; // for COUNTDOWN

// Status / stats
String   lastCaptureStatus = "Idle";
uint32_t totalPhotosSaved  = 0;

// Task handle for camera task (Core 0)
TaskHandle_t cameraTaskHandle = NULL;

// ============ Forward Declarations =========================
void initCamera();
void initSDCard();
void initLED();
bool captureJpegToBuffer();
bool decodeJpegToRGB565();
void streamFrameToLcd_Optimized(const uint8_t *src);
void clearAllPhotos();
bool saveCurrentPhoto();
void showSaveMessage();
void handleCaptureInstant();
void handleCaptureCountdown();
void setLED(bool red, bool green, bool blue);
void IRAM_ATTR button1ISR();
void IRAM_ATTR button2ISR();
void cameraTask(void* parameter);

// Web handlers
void handleRoot();
void handleCapture();
void handleToggleMode();
void handleCountdownStart();
void handleStatus();
void handleStream();

// ====================== Stitch HTML (with 4:3 preview) =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Stitch Cam - ESP32S3</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    :root {
      --blue-dark: #0b3056;
      --blue-main: #2c6fff;
      --blue-soft: #74a9ff;
      --blue-pastel: #dbeaff;
      --white: #ffffff;
      --accent-pink: #ff87b7;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      padding: 0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
                   "Comic Sans MS", sans-serif;
      background: radial-gradient(circle at top, #4b91ff 0, #0b2140 45%, #020712 100%);
      color: var(--white);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .bubble-bg {
      position: fixed;
      inset: 0;
      overflow: hidden;
      pointer-events: none;
      z-index: -1;
    }
    .bubble {
      position: absolute;
      border-radius: 50%;
      background: radial-gradient(circle, rgba(255,255,255,0.85) 0, rgba(116,169,255,0.1) 60%);
      opacity: 0.15;
      filter: blur(0.5px);
      animation: floatUp 18s linear infinite;
    }
    .bubble:nth-child(1) { width: 140px; height: 140px; left: 5%;  bottom: -150px; animation-delay: 0s; }
    .bubble:nth-child(2) { width: 90px;  height: 90px;  left: 75%; bottom: -120px; animation-delay: 3s; }
    .bubble:nth-child(3) { width: 60px;  height: 60px;  left: 18%; bottom: -80px;  animation-delay: 6s; }
    .bubble:nth-child(4) { width: 190px; height: 190px; left: 55%; bottom: -200px; animation-delay: 9s; }

    @keyframes floatUp {
      from { transform: translateY(0); }
      to   { transform: translateY(-120vh); }
    }

    .card {
      width: min(420px, 92vw);
      background: linear-gradient(145deg, rgba(12,42,82,0.98), rgba(21,65,120,0.98));
      border-radius: 26px;
      padding: 22px 20px 18px;
      box-shadow:
        0 20px 40px rgba(0,0,0,0.55),
        0 0 0 1px rgba(255,255,255,0.05);
      position: relative;
      overflow: hidden;
    }

    .card::before {
      content: "";
      position: absolute;
      inset: 0;
      background:
        radial-gradient(circle at 0 0, rgba(255,255,255,0.14) 0, transparent 60%),
        radial-gradient(circle at 100% 100%, rgba(116,169,255,0.18) 0, transparent 58%);
      pointer-events: none;
    }

    .header {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 16px;
      position: relative;
      z-index: 1;
    }

    .avatar {
      width: 54px;
      height: 54px;
      border-radius: 50%;
      padding: 3px;
      background: radial-gradient(circle at 20% 0, #ffffff 0, #9fd3ff 40%, #2558a8 100%);
      box-shadow: 0 6px 15px rgba(0,0,0,0.35);
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }
    .avatar img {
      width: 100%;
      height: 100%;
      object-fit: cover;
      border-radius: 50%;
      display: block;
    }

    .title-block h1 {
      margin: 0;
      font-size: 1.3rem;
      letter-spacing: 0.06em;
      text-transform: uppercase;
    }
    .title-block p {
      margin: 2px 0 0;
      font-size: 0.8rem;
      opacity: 0.8;
    }

    .badge {
      margin-left: auto;
      padding: 4px 10px;
      border-radius: 999px;
      background: linear-gradient(135deg, var(--accent-pink), #ffd3f0);
      color: #151528;
      font-size: 0.73rem;
      font-weight: 600;
      box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    }

    .preview {
      position: relative;
      border-radius: 20px;
      background: radial-gradient(circle at 30% 0, #dbeaff 0, #77a8ff 40%, #102447 100%);
      margin-bottom: 16px;
      box-shadow:
        inset 0 0 0 1px rgba(255,255,255,0.35),
        0 12px 25px rgba(0,0,0,0.5);
      overflow: hidden;

      /* Maintain 4:3 aspect ratio */
      width: 100%;
      aspect-ratio: 4 / 3;
    }

    .preview-inner {
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 4px;
    }

    /* 4:3, keep correct proportions */
    #previewImg {
      width: 100%;
      height: 100%;
      object-fit: contain;
      border-radius: 18px;
      background: #000;
    }

    .countdown-display {
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 4rem;
      font-weight: 800;
      text-shadow: 0 0 15px rgba(0,0,0,0.8);
      opacity: 0;
      transform: scale(0.7);
      transition: opacity 0.2s ease, transform 0.2s ease;
      pointer-events: none;
    }
    .countdown-display.active {
      opacity: 1;
      transform: scale(1);
    }

    .controls-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 10px;
      position: relative;
      z-index: 1;
    }

    .toggle-group {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .toggle-wrapper { display: inline-flex; flex-direction: column; gap: 2px; }
    .toggle-label {
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      opacity: 0.9;
    }
    .toggle-sub {
      font-size: 0.68rem;
      opacity: 0.7;
    }

    .switch {
      position: relative;
      width: 50px;
      height: 26px;
      border-radius: 999px;
      background: linear-gradient(135deg, #1b3259, #10213b);
      box-shadow:
        inset 0 0 0 1px rgba(255,255,255,0.14),
        0 4px 10px rgba(0,0,0,0.6);
      cursor: pointer;
      transition: background 0.2s ease;
    }
    .switch-knob {
      position: absolute;
      top: 3px;
      left: 3px;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 0, #ffffff 0, #dbeaff 45%, #7fb0ff 100%);
      box-shadow: 0 3px 6px rgba(0,0,0,0.5);
      transition: transform 0.2s ease;
    }
    .switch.on { background: linear-gradient(135deg, #3f89ff, #9ed2ff); }
    .switch.on .switch-knob { transform: translateX(24px); }

    .capture-btn {
      flex: 1;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      padding: 10px 14px;
      border-radius: 999px;
      border: none;
      font-size: 0.9rem;
      font-weight: 600;
      letter-spacing: 0.07em;
      text-transform: uppercase;
      cursor: pointer;
      background: radial-gradient(circle at 30% 0, #ffffff 0, #dbeaff 40%, #2c6fff 100%);
      color: #082041;
      box-shadow:
        0 8px 18px rgba(0,0,0,0.6),
        0 0 0 1px rgba(255,255,255,0.45);
      transition: transform 0.08s ease, box-shadow 0.08s ease, filter 0.08s ease;
    }
    .capture-btn:active {
      transform: translateY(1px) scale(0.98);
      box-shadow:
        0 4px 8px rgba(0,0,0,0.5),
        0 0 0 1px rgba(255,255,255,0.4);
      filter: brightness(0.97);
    }
    .capture-icon {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      border: 2px solid #082041;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      position: relative;
    }
    .capture-icon::before {
      content: "";
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #082041;
    }

    .footer-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      font-size: 0.72rem;
      opacity: 0.85;
      position: relative;
      z-index: 1;
    }
    .status-chip {
      padding: 4px 10px;
      border-radius: 999px;
      background: rgba(12,42,82,0.9);
      border: 1px solid rgba(191,214,255,0.3);
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #60ff9b;
      box-shadow: 0 0 8px rgba(96,255,155,0.7);
    }
    .status-chip.counting .status-dot {
      background: #ffe066;
      box-shadow: 0 0 8px rgba(255,224,102,0.7);
    }
    .status-chip.captured .status-dot {
      background: #60ff9b;
      box-shadow: 0 0 8px rgba(96,255,155,0.7);
    }
    .status-chip.error .status-dot {
      background: #ff6b6b;
      box-shadow: 0 0 8px rgba(255,107,107,0.7);
    }
    .status-text {
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .hint { text-align: right; opacity: 0.75; }
    .pill {
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid rgba(191,214,255,0.35);
    }

    @media (max-width: 480px) {
      .preview { aspect-ratio: 4 / 3; }
      .countdown-display { font-size: 3.2rem; }
      .card { padding-inline: 16px; }
    }
  </style>
</head>
<body>
  <div class="bubble-bg">
    <div class="bubble"></div>
    <div class="bubble"></div>
    <div class="bubble"></div>
    <div class="bubble"></div>
  </div>

  <main class="card">
    <header class="header">
      <div class="avatar">
        <img src="https://www.pikpng.com/pngl/b/48-485933_stitch-png-clipart-stitch-cartoon-png-transparent-png.png"
             alt="Stitch Avatar">
      </div>
      <div class="title-block">
        <h1>Stitch Cam</h1>
        <p>ESP32-S3 Snapshot Remote</p>
      </div>
      <div class="badge">v1.0</div>
    </header>

    <section class="preview" aria-label="Live preview">
      <div class="preview-inner">
        <img id="previewImg" src="" alt="Camera Preview">
      </div>
      <div id="countdown" class="countdown-display">3</div>
    </section>

    <section class="controls-row">
      <div class="toggle-group">
        <div class="toggle-wrapper">
          <span class="toggle-label">3s Countdown</span>
          <span class="toggle-sub" id="countdownModeLabel">Enabled</span>
        </div>
        <div id="countdownSwitch" class="switch on" role="switch" aria-checked="true">
          <div class="switch-knob"></div>
        </div>
      </div>

      <button id="captureBtn" class="capture-btn">
        <span class="capture-icon"></span>
        <span>Capture</span>
      </button>
    </section>

    <section class="footer-row">
      <div id="statusChip" class="status-chip">
        <span class="status-dot"></span>
        <span id="statusText" class="status-text">Idle · waiting for capture</span>
      </div>
      <div class="hint">
        <span class="pill" id="photoCount">Photos: 0</span>
      </div>
    </section>
  </main>

  <script>
    (function() {
      const captureBtn        = document.getElementById('captureBtn');
      const countdownEl       = document.getElementById('countdown');
      const countdownSwitch   = document.getElementById('countdownSwitch');
      const countdownModeLabel= document.getElementById('countdownModeLabel');
      const statusChip        = document.getElementById('statusChip');
      const statusText        = document.getElementById('statusText');
      const photoCountEl      = document.getElementById('photoCount');
      const previewImg        = document.getElementById('previewImg');

      let countdownEnabled = true;
      let countdownTimer   = null;
      let busy             = false;

      function setStatusChip(modeClass, text) {
        statusChip.classList.remove('counting','captured','error');
        if (modeClass) statusChip.classList.add(modeClass);
        statusText.textContent = text;
      }

      function setCountdownVisible(visible, value) {
        if (visible) {
          countdownEl.textContent = value;
          countdownEl.classList.add('active');
        } else {
          countdownEl.classList.remove('active');
        }
      }

      function updateStatus() {
        fetch('/status')
          .then(r => r.json())
          .then(d => {
            countdownEnabled = (d.mode === 'Countdown');
            countdownSwitch.classList.toggle('on', countdownEnabled);
            countdownSwitch.setAttribute('aria-checked', countdownEnabled ? 'true' : 'false');
            countdownModeLabel.textContent = countdownEnabled ? 'Enabled' : 'Disabled';

            photoCountEl.textContent = 'Photos: ' + d.photos;
            setStatusChip('', d.status);
          })
          .catch(() => {});
      }

      function refreshStream() {
        previewImg.src = '/stream?t=' + Date.now();
      }

      countdownSwitch.addEventListener('click', () => {
        fetch('/toggle')
          .then(() => {
            setTimeout(updateStatus, 300);
          })
          .catch(() => {});
      });

      function triggerInstantBackend() {
        fetch('/capture')
          .then(() => {
            setTimeout(() => {
              updateStatus();
              busy = false;
            }, 1000);
          })
          .catch(() => { busy = false; });
      }

      function triggerCountdownBackend() {
        fetch('/countdown_start')
          .then(() => {
            // MCU starts LED + camera countdown
          })
          .catch(() => {});
      }

      function startCountdown(seconds) {
        if (busy) return;
        busy = true;

        // Tell MCU to start countdown at the same time as UI
        triggerCountdownBackend();

        let c = seconds;
        setCountdownVisible(true, c);
        setStatusChip('counting', 'Countdown started…');

        countdownTimer = setInterval(() => {
          c--;
          if (c > 0) {
            setCountdownVisible(true, c);
          } else {
            clearInterval(countdownTimer);
            countdownTimer = null;
            setCountdownVisible(false);
            setStatusChip('counting', 'Capturing…');

            // Give MCU a bit of time to save & update status
            setTimeout(() => {
              updateStatus();
              busy = false;
            }, 1500);
          }
        }, 1000);
      }

      function instantCapture() {
        if (busy) return;
        busy = true;
        setStatusChip('captured', 'Instant capture triggered…');
        triggerInstantBackend();
      }

      captureBtn.addEventListener('click', () => {
        if (countdownEnabled) {
          startCountdown(3);
        } else {
          instantCapture();
        }
      });

      // Initial status + periodic polling
      updateStatus();
      setInterval(updateStatus, 3000);

      // Live preview (pull JPEG every 500ms)
      refreshStream();
      setInterval(refreshStream, 500);
    })();
  </script>
</body>
</html>
)rawliteral";

// ====================== TJpgDec Callback ===================
bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  for (uint16_t row = 0; row < h; row++) {
    if ((y + row) >= FRAME_H) break;
    for (uint16_t col = 0; col < w; col++) {
      if ((x + col) >= FRAME_W) break;
      uint32_t dstIdx = ((y + row) * FRAME_W + (x + col)) * 2;
      uint16_t pixel  = bitmap[row * w + col];
      frameBuf[dstIdx]     = pixel >> 8;
      frameBuf[dstIdx + 1] = pixel & 0xFF;
    }
  }
  return true;
}

// ====================== Button ISRs ========================
void IRAM_ATTR button1ISR() {
  uint32_t currentTime = millis();
  if (currentTime - lastButton1Time > DEBOUNCE_DELAY) {
    button1Pressed = true;
    lastButton1Time = currentTime;
  }
}

void IRAM_ATTR button2ISR() {
  uint32_t currentTime = millis();
  if (currentTime - lastButton2Time > DEBOUNCE_DELAY) {
    button2Pressed = true;
    lastButton2Time = currentTime;
  }
}

// ====================== setup() ============================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Dual-Core Stitch Cam          ║");
  Serial.println("╚══════════════════════════════════════════╝\n");

  // Buttons
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON1), button1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON2), button2ISR, FALLING);
  Serial.printf("Button 1 (Capture): GPIO %d\n", PIN_BUTTON1);
  Serial.printf("Button 2 (Mode):    GPIO %d\n\n", PIN_BUTTON2);

  // LED
  initLED();
  Serial.println("RGB LED initialized\n");

  // LED test
  setLED(true, false, false); delay(200);
  setLED(false, true, false); delay(200);
  setLED(false, false, true); delay(200);
  setLED(false, false, false);

  // CS pins
  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_CAM_CS, HIGH);

  // I2C
  Serial.println("1. Init I2C...");
  Wire.begin(PIN_SDA, PIN_SCL);
  delay(100);
  Serial.println("   ✓ OK");

  // SPI
  Serial.println("2. Init Shared SPI...");
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  delay(100);
  Serial.println("   ✓ OK");

  // SD
  Serial.println("3. Init SD Card...");
  initSDCard();

  // ArduCAM test
  Serial.println("4. Test ArduCAM...");
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(1);
  uint8_t test = myCAM.read_reg(ARDUCHIP_TEST1);
  SPI.endTransaction();

  if (test != 0x55) {
    Serial.printf("   ✗ ERROR: Expected 0x55, got 0x%02X\n", test);
    while (1) delay(1000);
  }
  Serial.println("   ✓ OK");

  // OV2640 I2C check
  Serial.println("5. Check OV2640...");
  Wire.beginTransmission(0x30);
  if (Wire.endTransmission() != 0) {
    Serial.println("   ✗ ERROR: Camera not found!");
    while (1) delay(1000);
  }
  Serial.println("   ✓ OK");

  // Camera init
  Serial.println("6. Init Camera...");
  initCamera();

  // LCD init
  Serial.println("7. Init LCD...");
  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  LCD_Clear(BLACK);
  Serial.println("   ✓ OK");

  // JPEG decoder
  Serial.println("8. Init JPEG Decoder...");
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);
  Serial.println("   ✓ OK");

  // WiFi
  Serial.println("9. WiFi begin...");
  WiFi.begin(ssid, password);
  Serial.print("   Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✓ WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("   ✗ WiFi failed, continuing with local control only");
  }

  // HTTP routes
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/toggle", handleToggleMode);
  server.on("/countdown_start", handleCountdownStart);
  server.on("/status", handleStatus);
  server.on("/stream", handleStream);
  server.begin();
  Serial.println("   ✓ HTTP server started");

  // Start camera task on Core 0
  Serial.println("\n10. Start camera task on Core 0...");
  xTaskCreatePinnedToCore(
    cameraTask,
    "CameraTask",
    8192,
    NULL,
    1,
    &cameraTaskHandle,
    0      // Core 0
  );

  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║        SYSTEM READY                      ║");
  Serial.println("╚══════════════════════════════════════════╝");
  Serial.println("Core 0: Camera + LCD + Buttons + SD + LED");
  Serial.println("Core 1: WiFi + HTTP server");
  Serial.println();
}

// ====================== loop() (Core 1) ====================
void loop() {
  server.handleClient();
}

// ====================== cameraTask (Core 0) ================
void cameraTask(void* parameter) {
  static uint32_t frameCount    = 0;
  static uint32_t lastPrintTime = 0;

  Serial.println("[CameraTask] Started on Core 0");

  for (;;) {
    // 1) Web-triggered flags
    if (webCaptureRequested) {
      webCaptureRequested = false;
      if (currentMode == MODE_INSTANT) {
        handleCaptureInstant();
        totalPhotosSaved++;
      } else {
        // If web mistakenly calls /capture in countdown mode, still do countdown
        handleCaptureCountdown();
        totalPhotosSaved++;
      }
    }

    if (webCountdownRequested) {
      webCountdownRequested = false;
      if (currentMode == MODE_COUNTDOWN) {
        handleCaptureCountdown();
        totalPhotosSaved++;
      }
    }

    // 2) Mode toggle (Button 2)
    if (button2Pressed) {
      button2Pressed = false;

      if (currentMode == MODE_INSTANT) {
        currentMode = MODE_COUNTDOWN;
        Serial.println("\n╔══════════════════════════════════════════╗");
        Serial.println("║   MODE: COUNTDOWN (3 seconds)            ║");
        Serial.println("╚══════════════════════════════════════════╝");
        Serial.println("Press Button1 / Web → LED + countdown → Save\n");

        setLED(false, false, true);
        delay(300);
        setLED(false, false, false);
      } else {
        currentMode = MODE_INSTANT;
        Serial.println("\n╔══════════════════════════════════════════╗");
        Serial.println("║   MODE: INSTANT                          ║");
        Serial.println("╚══════════════════════════════════════════╝");
        Serial.println("Press Button1 / Web → Instant save\n");

        setLED(false, false, true);
        delay(300);
        setLED(false, false, false);
      }
    }

    // 3) Capture via physical Button 1
    if (button1Pressed) {
      button1Pressed = false;

      if (currentMode == MODE_INSTANT) {
        handleCaptureInstant();
      } else {
        handleCaptureCountdown();
      }
      totalPhotosSaved++;
    }

    // 4) Normal streaming to LCD & keep latest JPEG for /stream
    if (!captureJpegToBuffer()) {
      delay(50);
      continue;
    }

    if (!decodeJpegToRGB565()) {
      delay(50);
      continue;
    }

    // Stream to LCD
    streamFrameToLcd_Optimized(frameBuf);

    frameCount++;

    if (millis() - lastPrintTime > 3000) {
      lastPrintTime = millis();
      const char* modeStr = (currentMode == MODE_INSTANT) ? "INSTANT" : "COUNTDOWN";
      Serial.printf("[CameraTask] Frame:%d | Mode:%s | Photos:%d\n",
                    frameCount, modeStr, totalPhotosSaved);
    }

    vTaskDelay(1);
  }
}

// ====================== Web Handlers (Core 1) ==============

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleCapture() {
  // Instant capture trigger (used when mode is INSTANT)
  webCaptureRequested = true;
  server.send(200, "text/plain", "OK");
}

void handleToggleMode() {
  // Just re-use button2 logic on cameraTask side
  button2Pressed = true;
  server.send(200, "text/plain", "OK");
}

void handleCountdownStart() {
  // Start countdown capture (used in COUNTDOWN mode)
  webCountdownRequested = true;
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String mode = (currentMode == MODE_INSTANT) ? "Instant" : "Countdown";
  String json = "{\"mode\":\"" + mode + "\",";
  json += "\"status\":\"" + lastCaptureStatus + "\",";
  json += "\"photos\":" + String(totalPhotosSaved) + "}";
  server.send(200, "application/json", json);
}

void handleStream() {
  uint32_t len = jpegLen;
  if (len == 0 || len > MAX_JPEG_SIZE) {
    server.send(503, "text/plain", "No frame yet");
    return;
  }

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send_P(200, "image/jpeg", (const char*)jpegBuf, len);
}

// ====================== Capture Functions ==================

void handleCaptureInstant() {
  Serial.println("\n*** INSTANT CAPTURE (Button/Web) ***");
  lastCaptureStatus = "Capturing...";

  if (sdCardAvailable) {
    showSaveMessage();

    uint32_t t0 = millis();

    Serial.println("→ Clearing old photos...");
    clearAllPhotos();

    Serial.println("→ Saving current frame...");
    if (saveCurrentPhoto()) {
      uint32_t t1 = millis();
      Serial.printf("✓ Saved current.jpg (%d bytes, %dms)\n", jpegLen, t1 - t0);
      lastCaptureStatus = "Saved!";
    } else {
      Serial.println("✗ Failed to save");
      lastCaptureStatus = "Failed";
    }

    delay(1000);
    lastCaptureStatus = "Idle";
  } else {
    Serial.println("✗ SD Card not available\n");
    lastCaptureStatus = "No SD card";
    delay(500);
    lastCaptureStatus = "Idle";
  }
}

void handleCaptureCountdown() {
  Serial.println("\n*** COUNTDOWN MODE (Button/Web) ***");
  Serial.println("Starting 3-second countdown...");
  lastCaptureStatus = "Countdown...";

  // 3 seconds total, with RED LED blinking, while still streaming
  for (int second = 3; second >= 1; second--) {
    Serial.printf("  %d...\n", second);

    for (int blink = 0; blink < 3; blink++) {
      // LED ON (RED)
      setLED(true, false, false);
      uint32_t blinkStart = millis();
      while (millis() - blinkStart < 167) {
        if (captureJpegToBuffer() && decodeJpegToRGB565()) {
          streamFrameToLcd_Optimized(frameBuf);
        }
      }

      // LED OFF
      setLED(false, false, false);
      blinkStart = millis();
      while (millis() - blinkStart < 167) {
        if (captureJpegToBuffer() && decodeJpegToRGB565()) {
          streamFrameToLcd_Optimized(frameBuf);
        }
      }
    }
  }

  // Countdown complete - capture final frame
  Serial.println("  CAPTURE!");
  lastCaptureStatus = "Capturing...";

  if (captureJpegToBuffer()) {
    // LED GREEN at capture moment
    setLED(false, true, false);

    if (sdCardAvailable) {
      showSaveMessage();

      uint32_t t0 = millis();
      Serial.println("→ Clearing old photos...");
      clearAllPhotos();

      Serial.println("→ Saving current frame...");
      if (saveCurrentPhoto()) {
        uint32_t t1 = millis();
        Serial.printf("✓ Saved current.jpg (%d bytes, %dms)\n", jpegLen, t1 - t0);
        lastCaptureStatus = "Saved!";
      } else {
        Serial.println("✗ Failed to save");
        lastCaptureStatus = "Failed";
      }

      delay(1000);
    } else {
      Serial.println("✗ SD Card not available");
      lastCaptureStatus = "No SD card";
      delay(500);
    }
  } else {
    Serial.println("✗ Failed to capture final frame");
    lastCaptureStatus = "Capture failed";
    delay(500);
  }

  setLED(false, false, false);
  lastCaptureStatus = "Idle";
  Serial.println("Resuming...\n");
}

// ====================== LED Functions ======================

void initLED() {
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE,  OUTPUT);
  setLED(false, false, false);
}

void setLED(bool red, bool green, bool blue) {
  // Common-anode style (LOW = ON)
  digitalWrite(PIN_LED_RED,   red   ? LOW : HIGH);
  digitalWrite(PIN_LED_GREEN, green ? LOW : HIGH);
  digitalWrite(PIN_LED_BLUE,  blue  ? LOW : HIGH);
}

// ====================== Display Functions ==================

void showSaveMessage() {
  const char* text = "SAVING...";
  uint16_t textX = 110;
  uint16_t textY = 150;

  UBYTE* oldImage = Paint.Image;

  Paint.Image       = frameBuf;
  Paint.WidthMemory = FRAME_W;
  Paint.HeightMemory= FRAME_H;
  Paint.Width       = FRAME_W;
  Paint.Height      = FRAME_H;
  Paint.WidthByte   = FRAME_W;
  Paint.HeightByte  = FRAME_H;
  Paint.Color       = 0;
  Paint.Rotate      = ROTATE_0;
  Paint.Mirror      = MIRROR_NONE;

  // Bold text
  Paint_DrawString_EN(textX,   textY,   text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(textX+1, textY,   text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(textX,   textY+1, text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(textX+1, textY+1, text, &Font24, BLACK, WHITE);

  Paint.Image = oldImage;
  streamFrameToLcd_Optimized(frameBuf);
}

// ====================== SD Card Functions ==================

void clearAllPhotos() {
  if (!sdCardAvailable) return;

  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
    return;
  }

  File root = SD.open("/photos");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = String("/photos/") + file.name();
      file.close();
      SD.remove(filename.c_str());
    } else {
      file.close();
    }
    file = root.openNextFile();
  }
  root.close();
}

bool saveCurrentPhoto() {
  if (!sdCardAvailable) return false;

  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }

  const char* filename = "/photos/current.jpg";
  if (SD.exists(filename)) {
    SD.remove(filename);
  }

  File file = SD.open(filename, FILE_WRITE);
  if (!file) return false;

  size_t written = file.write(jpegBuf, jpegLen);
  file.close();

  return (written == jpegLen);
}

void initSDCard() {
  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("   ✗ SD Card init failed");
    sdCardAvailable = false;
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("   ✗ No SD card");
    sdCardAvailable = false;
    return;
  }

  Serial.print("   Type: ");
  switch(cardType) {
    case CARD_MMC:  Serial.println("MMC"); break;
    case CARD_SD:   Serial.println("SDSC"); break;
    case CARD_SDHC: Serial.println("SDHC"); break;
    default:        Serial.println("UNKNOWN"); break;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("   Size: %llu MB\n", cardSize);

  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }

  sdCardAvailable = true;
  Serial.println("   ✓ SD Card READY");
}

// ====================== Camera Functions ===================

void initCamera() {
  digitalWrite(PIN_SD_CS, HIGH);

  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  SPI.endTransaction();

  delay(500);

  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  SPI.endTransaction();

  uint32_t start = millis();
  while (millis() - start < 5000) {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
    SPI.endTransaction();
    if (done) break;
    delay(10);
  }

  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  uint32_t len = myCAM.read_fifo_length();
  myCAM.clear_fifo_flag();
  SPI.endTransaction();

  Serial.printf("   Test frame length: %d bytes\n", len);
  if (len > 0 && len < MAX_JPEG_SIZE) {
    Serial.println("   ✓ Camera OK");
  }
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
    bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
    SPI.endTransaction();
    if (done) break;
    delay(5);
  }

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  uint32_t len = myCAM.read_fifo_length();
  SPI.endTransaction();

  if (len == 0 || len > MAX_JPEG_SIZE) {
    return false;
  }

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  for (uint32_t i = 0; i < len; i++) {
    jpegBuf[i] = SPI.transfer(0x00);
  }

  myCAM.CS_HIGH();
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  myCAM.clear_fifo_flag();
  SPI.endTransaction();

  // Publish length last, so /stream will see consistent len->buf
  jpegLen = len;

  return (jpegBuf[0] == 0xFF && jpegBuf[1] == 0xD8);
}

bool decodeJpegToRGB565() {
  memset(frameBuf, 0, FRAME_BYTES);

  uint16_t w, h;
  if (TJpgDec.getJpgSize(&w, &h, jpegBuf, jpegLen) != JDR_OK) return false;

  return (TJpgDec.drawJpg(0, 0, jpegBuf, jpegLen) == JDR_OK);
}

// ====================== LCD Streaming ======================

void streamFrameToLcd_Optimized(const uint8_t *src) {
  uint8_t *dmaBuf = getDMABuffer();
  const uint16_t LCD_W = 240;
  const uint16_t LCD_H = 320;

  LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
  DEV_SPI_Write_Bulk_Start();

  for (uint16_t lcdRow = 0; lcdRow < LCD_H; lcdRow++) {
    uint16_t bufIdx = 0;

    for (uint16_t lcdCol = 0; lcdCol < LCD_W; lcdCol++) {
      // Rotate 90° to match your portrait orientation
      uint16_t srcRow = LCD_W - 1 - lcdCol; // 0..239
      uint16_t srcCol = lcdRow;             // 0..319
      uint32_t srcIdx = (srcRow * FRAME_W + srcCol) * 2;

      dmaBuf[bufIdx++] = src[srcIdx];
      dmaBuf[bufIdx++] = src[srcIdx + 1];

      if (bufIdx >= DMA_BUFFER_SIZE) {
        DEV_SPI_Write_Bulk_Data(dmaBuf, bufIdx);
        bufIdx = 0;
      }
    }

    if (bufIdx > 0) {
      DEV_SPI_Write_Bulk_Data(dmaBuf, bufIdx);
    }
  }

  DEV_SPI_Write_Bulk_End();
}
