/**************************************************************************
 * ESP32-S3 Dual-Core Camera + LCD + WiFi Web Control + MJPEG Stream
 *
 * Core 0: Camera + LCD + Buttons + LED
 * Core 1: WiFi + WebServer + MJPEG streaming
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

#include "esp_heap_caps.h"   // for PSRAM allocations

// ======================= WiFi Config =========================
const char* ssid     = "KellyiPhone";
const char* password = "kelly200636";

WebServer server(80);

// ======================= Pins ===============================
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

// ======================= Buffers (PSRAM) =====================

#define MAX_JPEG_SIZE 32768

static const uint16_t FRAME_W = 320;
static const uint16_t FRAME_H = 240;
static const uint32_t FRAME_BYTES = FRAME_W * FRAME_H * 2;

// These will be allocated in PSRAM at runtime
uint8_t *jpegBuf         = nullptr;   // capture buffer
uint8_t *frameBuf        = nullptr;   // RGB565 frame for LCD
uint8_t *jpegStreamBuf   = nullptr;   // latest JPEG for MJPEG clients
uint8_t *streamWorkBuf   = nullptr;   // per-client working buffer

uint32_t jpegLen         = 0;
uint32_t jpegStreamLen   = 0;
volatile uint32_t jpegStreamFrameId = 0;

// Spinlock for synchronized camera-to-webserver JPEG sharing
portMUX_TYPE camMux = portMUX_INITIALIZER_UNLOCKED;

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// ======================= State Variables ====================
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

volatile bool webCaptureRequested     = false;
volatile bool webModeToggleRequested  = false;

uint32_t       totalPhotosSaved = 0;
volatile char  lastCaptureStatus[32] = "Idle";

TaskHandle_t cameraTaskHandle = NULL;

// ======================= Forward Declarations ================
bool allocBuffers();
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
void cameraTask(void *parameter);

// Web handlers
void handleRoot();
void handleCapture();
void handleToggleMode();
void handleStatus();
void handleStream();

// ======================= TJpgDec Callback (Correct Signature) ====================
bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
    if (!frameBuf) return false;

    for (uint16_t row = 0; row < h; row++) {
        if ((y + row) >= FRAME_H) break;

        for (uint16_t col = 0; col < w; col++) {
            if ((x + col) >= FRAME_W) break;

            uint32_t idx = ((y + row) * FRAME_W + (x + col)) * 2;
            uint16_t px  = bitmap[row * w + col];

            frameBuf[idx]     = px >> 8;
            frameBuf[idx + 1] = px & 0xFF;
        }
    }
    return true;
}

// ======================= Buffer Allocation (PSRAM) ===========

bool allocBuffers() {
    frameBuf      = (uint8_t*) heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpegBuf       = (uint8_t*) heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpegStreamBuf = (uint8_t*) heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    streamWorkBuf = (uint8_t*) heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!frameBuf || !jpegBuf || !jpegStreamBuf || !streamWorkBuf) {
        Serial.println("❌ PSRAM buffer allocation failed. Check PSRAM setting.");
        return false;
    }

    Serial.println("✓ PSRAM buffers allocated.");
    return true;
}

// ======================= Button ISRs =========================

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

// ======================= Setup (Core 1) ======================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== ESP32-S3 Dual-Core MJPEG Camera (PSRAM) ===");

    if (!allocBuffers()) {
        while (1) {
            Serial.println("HALT: No PSRAM or allocation failed.");
            delay(2000);
        }
    }

    // Buttons
    pinMode(PIN_BUTTON1, INPUT_PULLUP);
    pinMode(PIN_BUTTON2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON1), button1ISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON2), button2ISR, FALLING);

    // LED
    initLED();

    // Chip selects
    pinMode(PIN_SD_CS, OUTPUT);
    pinMode(PIN_CAM_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
    digitalWrite(PIN_CAM_CS, HIGH);

    // Buses
    Wire.begin(PIN_SDA, PIN_SCL);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

    // Peripherals
    initSDCard();
    initCamera();

    Config_Init();
    LCD_Init();
    LCD_SetBacklight(100);
    LCD_Clear(BLACK);

    TJpgDec.setCallback(tjpgOutput);

    // ======== WiFi Init (Core 1) ========
    WiFi.begin(ssid, password);
    Serial.print("Connecting");
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(400);
        Serial.print(".");
        tries++;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi failed. Physical mode only.");
    } else {
        Serial.printf("✓ WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    }

    // Web routes
    server.on("/",        handleRoot);
    server.on("/capture", handleCapture);
    server.on("/toggle",  handleToggleMode);
    server.on("/status",  handleStatus);
    server.on("/stream",  handleStream);
    server.begin();
    Serial.println("✓ Web server started.");

    // ======== Start Camera Task on Core 0 ========
    xTaskCreatePinnedToCore(
        cameraTask,
        "CameraTask",
        15000,
        NULL,
        1,
        &cameraTaskHandle,
        0
    );

    Serial.println("✓ CameraTask pinned to Core 0.");
}

// ======================= Main Loop (Core 1) ==================

void loop() {
    server.handleClient();
    delay(1);
}

// ======================= Camera Task (Core 0) =================

void cameraTask(void *parameter)
{
    Serial.println("[Core0] Camera Task Running.");

    uint32_t frameCount    = 0;
    uint32_t lastPrintTime = 0;

    for (;;) {
        // Web-triggered events
        if (webModeToggleRequested) {
            webModeToggleRequested = false;
            button2Pressed = true;
        }

        if (webCaptureRequested) {
            webCaptureRequested = false;
            button1Pressed = true;
        }

        // Mode toggle
        if (button2Pressed) {
            button2Pressed = false;

            currentMode = (currentMode == MODE_INSTANT)
                          ? MODE_COUNTDOWN
                          : MODE_INSTANT;

            setLED(false,false,true);
            vTaskDelay(pdMS_TO_TICKS(200));
            setLED(false,false,false);
        }

        // Capture actions
        if (button1Pressed) {
            button1Pressed = false;

            if (currentMode == MODE_INSTANT)
                handleCaptureInstant();
            else
                handleCaptureCountdown();

            totalPhotosSaved++;
        }

        // Continuous live stream to LCD + MJPEG buffer
        if (captureJpegToBuffer()) {
            if (decodeJpegToRGB565()) {
                streamFrameToLcd_Optimized(frameBuf);
            }

            // Publish JPEG for web clients
            portENTER_CRITICAL(&camMux);
            jpegStreamLen = jpegLen;
            memcpy(jpegStreamBuf, jpegBuf, jpegLen);
            jpegStreamFrameId++;
            portEXIT_CRITICAL(&camMux);

            frameCount++;
        }

        uint32_t now = millis();
        if (now - lastPrintTime > 5000) {
            lastPrintTime = now;
            const char* modeStr = (currentMode == MODE_INSTANT) ? "INSTANT" : "COUNTDOWN";
            Serial.printf("[Core0] Frame:%lu | Mode:%s | Photos:%lu\n",
                          (unsigned long)frameCount, modeStr, (unsigned long)totalPhotosSaved);
        }

        vTaskDelay(1);
    }
}

// ======================= Web Handlers ========================

void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    server.sendContent(
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>Stitch Cam (MJPEG)</title>"
        "<style>"
        "body{margin:0;padding:20px;font-family:Arial;background:#1a1a2e;color:#fff}"
        ".card{max-width:420px;margin:0 auto;background:#16213e;padding:20px;border-radius:15px;"
        "box-shadow:0 4px 20px rgba(0,0,0,0.5)}"
        "h1{margin:0 0 20px;font-size:1.5rem;text-align:center}"
        ".stream{width:100%;height:240px;background:#000;border-radius:10px;margin-bottom:15px;"
        "overflow:hidden;display:flex;align-items:center;justify-content:center}"
        "#streamImg{width:100%;height:100%;object-fit:cover}"
        ".controls{display:flex;gap:10px;margin-bottom:15px;align-items:center}"
        ".toggle{display:flex;align-items:center;gap:8px;flex:1}"
        ".switch{position:relative;width:50px;height:26px;background:#444;border-radius:20px;cursor:pointer;transition:0.3s}"
        ".switch.on{background:#4CAF50}"
        ".knob{position:absolute;top:3px;left:3px;width:20px;height:20px;background:#fff;border-radius:50%;transition:0.3s}"
        ".switch.on .knob{transform:translateX(24px)}"
        ".btn{flex:2;padding:12px;background:#2196F3;color:#fff;border:none;border-radius:25px;font-size:1rem;font-weight:bold;cursor:pointer;transition:0.2s}"
        ".btn:active{transform:scale(0.95);background:#1976D2}"
        ".status{text-align:center;padding:10px;background:#0f3460;border-radius:8px;font-size:0.9rem}"
        ".dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#4CAF50;margin-right:8px}"
        "</style></head><body>"
        "<div class='card'>"
        "<h1>🎥 Stitch Cam (MJPEG)</h1>"
        "<div class='stream'><img id='streamImg' src='/stream'></div>"
        "<div class='controls'>"
        "<div class='toggle'><span>Countdown</span>"
        "<div id='modeSwitch' class='switch' onclick='toggleMode()'><div class='knob'></div></div>"
        "</div>"
        "<button class='btn' onclick='capture()'>📸 Capture</button>"
        "</div>"
        "<div class='status'><span class='dot'></span>"
        "<span id='statusText'>Ready</span> | <span id='photoCount'>Photos: 0</span>"
        "</div>"
        "</div>"
        "<script>"
        "let countdownMode=false,busy=false;"
        "function updateStatus(){"
        " fetch('/status').then(r=>r.json()).then(d=>{"
        "  document.getElementById('statusText').textContent=d.status;"
        "  document.getElementById('photoCount').textContent='Photos: '+d.photos;"
        "  countdownMode=(d.mode=='Countdown');"
        "  document.getElementById('modeSwitch').className=countdownMode?'switch on':'switch';"
        " });"
        "}"
        "function toggleMode(){fetch('/toggle').then(()=>setTimeout(updateStatus,200));}"
        "function capture(){if(busy)return;busy=true;"
        " fetch('/capture').then(()=>setTimeout(()=>{updateStatus();busy=false;},1000));}"
        "setInterval(updateStatus,3000);updateStatus();"
        "</script></body></html>"
    );
    server.sendContent("");
}

void handleCapture() {
    webCaptureRequested = true;
    server.send(200, "text/plain", "OK");
}

void handleToggleMode() {
    webModeToggleRequested = true;
    server.send(200, "text/plain", "OK");
}

void handleStatus() {
    String mode = (currentMode == MODE_INSTANT ? "Countdown" : "Instant");
    // Actually, currentMode==INSTANT => label "Instant" (typo fix)
    mode = (currentMode == MODE_INSTANT ? "Instant" : "Countdown");
    String status((const char*)lastCaptureStatus);

    String json = "{\"mode\":\"" + mode +
                  "\",\"status\":\"" + status +
                  "\",\"photos\":" + String(totalPhotosSaved) + "}";

    server.send(200,"application/json",json);
}

// ---------- MJPEG Streaming ----------
void handleStream()
{
    if (!jpegStreamBuf || !streamWorkBuf) {
        server.send(503,"text/plain","No buffers");
        return;
    }

    WiFiClient client = server.client();
    if (!client) return;

    client.print(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n\r\n"
    );

    uint32_t last = jpegStreamFrameId;

    while (client.connected()) {
        // Wait for new frame
        uint32_t waitStart = millis();
        while (jpegStreamFrameId == last && millis() - waitStart < 1000) {
            delay(5);
        }
        last = jpegStreamFrameId;

        uint32_t len;

        portENTER_CRITICAL(&camMux);
        len = jpegStreamLen;
        if (len > 0 && len <= MAX_JPEG_SIZE) {
            memcpy(streamWorkBuf, jpegStreamBuf, len);
        }
        portEXIT_CRITICAL(&camMux);

        if (len == 0 || len > MAX_JPEG_SIZE) continue;

        client.print("--frame\r\n");
        client.print("Content-Type: image/jpeg\r\n");
        client.print("Content-Length: ");
        client.print(len);
        client.print("\r\n\r\n");
        client.write(streamWorkBuf, len);
        client.print("\r\n");

        delay(40); // ~25 fps max
    }
}

// ======================= Capture Logic =======================

void handleCaptureInstant() {
    strcpy((char*)lastCaptureStatus,"Capturing...");

    if (!captureJpegToBuffer()) {
        strcpy((char*)lastCaptureStatus,"Capture fail");
        return;
    }

    if (sdCardAvailable) {
        showSaveMessage();
        clearAllPhotos();
        if (saveCurrentPhoto()) {
            strcpy((char*)lastCaptureStatus,"Saved!");
        } else {
            strcpy((char*)lastCaptureStatus,"Save fail");
        }
    } else {
        strcpy((char*)lastCaptureStatus,"No SD");
    }

    vTaskDelay(pdMS_TO_TICKS(400));
    strcpy((char*)lastCaptureStatus,"Idle");
}

void handleCaptureCountdown() {
    strcpy((char*)lastCaptureStatus,"Countdown...");

    for (int c=3; c>=1; c--) {
        setLED(true,false,false);
        vTaskDelay(pdMS_TO_TICKS(250));
        setLED(false,false,false);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if (!captureJpegToBuffer()) {
        strcpy((char*)lastCaptureStatus,"Capture fail");
        return;
    }

    setLED(false,true,false);

    if (sdCardAvailable) {
        showSaveMessage();
        clearAllPhotos();
        if (saveCurrentPhoto()) {
            strcpy((char*)lastCaptureStatus,"Saved!");
        } else {
            strcpy((char*)lastCaptureStatus,"Save fail");
        }
    } else {
        strcpy((char*)lastCaptureStatus,"No SD");
    }

    vTaskDelay(pdMS_TO_TICKS(400));
    setLED(false,false,false);
    strcpy((char*)lastCaptureStatus,"Idle");
}

// ======================= LED ================================

void initLED() {
    pinMode(PIN_LED_RED,OUTPUT);
    pinMode(PIN_LED_GREEN,OUTPUT);
    pinMode(PIN_LED_BLUE,OUTPUT);
    setLED(false,false,false);
}

void setLED(bool r,bool g,bool b) {
    digitalWrite(PIN_LED_RED,   r?LOW:HIGH);
    digitalWrite(PIN_LED_GREEN, g?LOW:HIGH);
    digitalWrite(PIN_LED_BLUE,  b?LOW:HIGH);
}

// ======================= Display Utils =======================

void showSaveMessage() {
    if (!frameBuf) return;

    const char* text = "SAVING...";
    uint16_t textX = 110;
    uint16_t textY = 150;

    UBYTE* oldImage  = Paint.Image;
    Paint.Image      = frameBuf;
    Paint.WidthMemory  = FRAME_W;
    Paint.HeightMemory = FRAME_H;
    Paint.Width        = FRAME_W;
    Paint.Height       = FRAME_H;
    Paint.WidthByte    = FRAME_W;
    Paint.HeightByte   = FRAME_H;
    Paint.Color        = 0;
    Paint.Rotate       = ROTATE_0;
    Paint.Mirror       = MIRROR_NONE;

    Paint_DrawString_EN(textX,   textY,   text, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(textX+1, textY,   text, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(textX,   textY+1, text, &Font24, BLACK, WHITE);
    Paint_DrawString_EN(textX+1, textY+1, text, &Font24, BLACK, WHITE);

    Paint.Image = oldImage;
    streamFrameToLcd_Optimized(frameBuf);
}

// ======================= SD Card =============================

void clearAllPhotos() {
    if (!sdCardAvailable) return;

    if (!SD.exists("/photos")) {
        SD.mkdir("/photos");
        return;
    }

    File root = SD.open("/photos");
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return;
    }

    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String path = String("/photos/") + f.name();
            f.close();
            SD.remove(path.c_str());
        } else {
            f.close();
        }
        f = root.openNextFile();
    }
    root.close();
}

bool saveCurrentPhoto() {
    if (!sdCardAvailable || !jpegBuf) return false;

    if (!SD.exists("/photos")) SD.mkdir("/photos");

    const char* fn = "/photos/current.jpg";
    if (SD.exists(fn)) SD.remove(fn);

    File f = SD.open(fn, FILE_WRITE);
    if (!f) return false;

    size_t written = f.write(jpegBuf, jpegLen);
    f.close();
    return (written == jpegLen);
}

void initSDCard() {
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("SD init failed.");
        sdCardAvailable=false;
        return;
    }
    sdCardAvailable=true;
    Serial.println("SD ready.");
}

// ======================= Camera ==============================

void initCamera() {
    Serial.println("Init ArduCAM...");

    SPI.beginTransaction(SPISettings(4000000,MSBFIRST,SPI_MODE0));
    myCAM.set_format(JPEG);
    myCAM.InitCAM();
    myCAM.OV2640_set_JPEG_size(OV2640_320x240);
    SPI.endTransaction();

    Serial.println("Camera init done.");
}

bool captureJpegToBuffer() {
    if (!jpegBuf) return false;

    digitalWrite(PIN_SD_CS,HIGH);

    SPI.beginTransaction(SPISettings(8000000,MSBFIRST,SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();

    uint32_t t=millis();
    while (millis()-t<3000) {
        SPI.beginTransaction(SPISettings(8000000,MSBFIRST,SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG,CAP_DONE_MASK);
        SPI.endTransaction();
        if(done) break;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    SPI.beginTransaction(SPISettings(8000000,MSBFIRST,SPI_MODE0));
    jpegLen = myCAM.read_fifo_length();
    SPI.endTransaction();

    if (jpegLen==0 || jpegLen>MAX_JPEG_SIZE) return false;

    SPI.beginTransaction(SPISettings(20000000,MSBFIRST,SPI_MODE0));
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    for (uint32_t i=0;i<jpegLen;i++) {
        jpegBuf[i]=SPI.transfer(0x00);
    }
    myCAM.CS_HIGH();
    SPI.endTransaction();

    return true;
}

bool decodeJpegToRGB565() {
    if (!jpegBuf || !frameBuf) return false;
    return (TJpgDec.drawJpg(0,0,jpegBuf,jpegLen) == JDR_OK);
}

// ======================= LCD DMA =============================

void streamFrameToLcd_Optimized(const uint8_t *src)
{
    if (!src) return;

    uint8_t *dma = getDMABuffer();
    const uint16_t LCD_W=240;
    const uint16_t LCD_H=320;

    LCD_SetCursor(0,0,LCD_W-1,LCD_H-1);
    DEV_SPI_Write_Bulk_Start();

    for (uint16_t y=0; y<LCD_H; y++) {
        uint16_t idx=0;
        for (uint16_t x=0; x<LCD_W; x++) {
            uint16_t sx = LCD_W-1-x;
            uint16_t sy = y;
            uint32_t si=(sy*FRAME_W+sx)*2;

            dma[idx++] = src[si];
            dma[idx++] = src[si+1];

            if (idx >= DMA_BUFFER_SIZE) {
                DEV_SPI_Write_Bulk_Data(dma, idx);
                idx=0;
            }
        }
        if (idx>0) DEV_SPI_Write_Bulk_Data(dma, idx);
    }

    DEV_SPI_Write_Bulk_End();
}
