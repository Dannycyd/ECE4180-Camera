/**************************************************************************
 * ESP32-C6 Dual-SPI ArduCAM Streaming
 *
 * SPI BUS 1 (HSPI) → LCD (from DEV_Config.cpp)
 * SPI BUS 2 (FSPI) → ArduCAM (20 MHz, MODE0)
 * 
 * NOTE: ArduCAM library uses global SPI object internally, so we need
 * to manually handle camera SPI operations using SPIcam
 **************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include "memorysaver.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// ArduCAM register definitions (from ArduCAM.h)
#define ARDUCHIP_TEST1      0x00
#define ARDUCHIP_FIFO       0x04
#define FIFO_CLEAR_MASK     0x01
#define FIFO_START_MASK     0x02
#define FIFO_RDPTR_RST_MASK 0x10
#define FIFO_WRPTR_RST_MASK 0x20

#define ARDUCHIP_TRIG       0x41
#define CAP_DONE_MASK       0x08

#define BURST_FIFO_READ     0x3C
#define SINGLE_FIFO_READ    0x3D

// OV2640 sensor
#define OV2640_CHIPID_HIGH  0x0A
#define OV2640_CHIPID_LOW   0x0B

// ============================================================
// DUAL SPI BUS SETUP
// ============================================================

SPIClass SPIcam(FSPI);    // Secondary SPI peripheral for camera

// Camera pins
static const int PIN_CAM_CS   = 18;
static const int PIN_CAM_SCK  = 21;
static const int PIN_CAM_MISO = 20;
static const int PIN_CAM_MOSI = 19;

// I2C pins
static const int PIN_SDA = 22;
static const int PIN_SCL = 23;

// OV2640 I2C address
static const uint8_t OV2640_I2C_ADDR = 0x30;

// ============================================================
// CAMERA CONFIG
// ============================================================

static const uint16_t SRC_W = 320;
static const uint16_t SRC_H = 240;
static const uint32_t FRAME_BYTES = SRC_W * SRC_H * 2;  // RGB565 = 2 bytes/pixel

static const size_t DMA_CHUNK = 4096;

uint8_t frameBuf[FRAME_BYTES];
uint8_t dummyTx[DMA_CHUNK] = {0};

// ============================================================
// CAMERA SPI WRAPPER FUNCTIONS
// ============================================================

void camWriteReg(uint8_t addr, uint8_t data) {
    digitalWrite(PIN_CAM_CS, LOW);
    delayMicroseconds(1);  // Small CS settling time
    SPIcam.transfer(addr | 0x80);  // Write bit
    SPIcam.transfer(data);
    delayMicroseconds(1);
    digitalWrite(PIN_CAM_CS, HIGH);
    delayMicroseconds(10);  // CS recovery time
}

uint8_t camReadReg(uint8_t addr) {
    digitalWrite(PIN_CAM_CS, LOW);
    SPIcam.transfer(addr & 0x7F);  // Read bit
    uint8_t data = SPIcam.transfer(0x00);
    digitalWrite(PIN_CAM_CS, HIGH);
    return data;
}

void camSetBit(uint8_t addr, uint8_t bit) {
    uint8_t temp = camReadReg(addr);
    camWriteReg(addr, temp | bit);
}

void camClearBit(uint8_t addr, uint8_t bit) {
    uint8_t temp = camReadReg(addr);
    camWriteReg(addr, temp & (~bit));
}

uint8_t camGetBit(uint8_t addr, uint8_t bit) {
    uint8_t temp = camReadReg(addr);
    return temp & bit;
}

// ============================================================
// OV2640 I2C FUNCTIONS
// ============================================================

void ov2640_write_reg(uint8_t reg, uint8_t data) {
    Wire.beginTransmission(OV2640_I2C_ADDR);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();
    delayMicroseconds(100);
}

uint8_t ov2640_read_reg(uint8_t reg) {
    Wire.beginTransmission(OV2640_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom(OV2640_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

// ============================================================
// FORWARD DECLARATIONS
// ============================================================

void initCameraRGB565();
void captureFrameToBuffer();
void streamFrameToLcd(const uint8_t *src);

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("=== ESP32-C6 Dual-SPI ArduCAM → LCD DMA Stream ===");

    // -------------------------------
    // Init secondary SPI bus for CAMERA
    // -------------------------------
    SPIcam.begin(PIN_CAM_SCK, PIN_CAM_MISO, PIN_CAM_MOSI, PIN_CAM_CS);

    pinMode(PIN_CAM_CS, OUTPUT);
    digitalWrite(PIN_CAM_CS, HIGH);

    // ArduCAM SPI test
    delay(100);
    
    SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    camWriteReg(ARDUCHIP_TEST1, 0x55);
    delay(1);
    uint8_t test = camReadReg(ARDUCHIP_TEST1);
    SPIcam.endTransaction();

    if (test != 0x55) {
        Serial.print("ArduCAM SPI FAIL, got 0x");
        Serial.println(test, HEX);
        Serial.println("Check wiring and SPI pins!");
        while (1) delay(1000);
    }
    Serial.println("ArduCAM SPI OK on FSPI bus");

    // -------------------------------
    // I2C init for OV2640
    // -------------------------------
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);  // 100kHz for OV2640
    delay(100);

    // Check I2C camera connection
    Wire.beginTransmission(OV2640_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("ERROR: OV2640 not detected on I2C!");
    } else {
        Serial.println("OV2640 I2C detected");
        
        // Read chip ID
        uint8_t vid = ov2640_read_reg(OV2640_CHIPID_HIGH);
        uint8_t pid = ov2640_read_reg(OV2640_CHIPID_LOW);
        Serial.print("Chip ID: 0x");
        Serial.print(vid, HEX);
        Serial.println(pid, HEX);
    }

    // -------------------------------
    // LCD init (primary SPI)
    // -------------------------------
    Config_Init();
    LCD_Init();
    LCD_SetBacklight(100);
    LCD_Clear(BLACK);

    Serial.println("LCD ready (DMA active)");

    // -------------------------------
    // Camera configuration
    // -------------------------------
    initCameraRGB565();

    Serial.print("Frame buffer bytes: ");
    Serial.println(FRAME_BYTES);
    Serial.println("Starting capture loop...");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    uint32_t t0 = millis();
    captureFrameToBuffer();
    uint32_t t1 = millis();

    streamFrameToLcd(frameBuf);
    uint32_t t2 = millis();

    float fps = 1000.0f / (float)(t2 - t0);

    Serial.print("Capture=");
    Serial.print(t1 - t0);
    Serial.print(" ms, LCD=");
    Serial.print(t2 - t1);
    Serial.print(" ms, FPS=");
    Serial.println(fps, 2);

    delay(1);  // Prevent watchdog
}

// ============================================================
// CAMERA: INIT RGB565 (OV2640 Configuration)
// ============================================================

void initCameraRGB565() {
    Serial.println("Initializing camera for RGB565...");
    Serial.flush();
    
    // Reset CPLD
    Serial.print("Resetting CPLD...");
    Serial.flush();
    
    SPIcam.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CAM_CS, LOW);
    delayMicroseconds(1);
    SPIcam.transfer(0x07 | 0x80);
    SPIcam.transfer(0x80);
    delayMicroseconds(1);
    digitalWrite(PIN_CAM_CS, HIGH);
    SPIcam.endTransaction();
    
    delay(100);
    
    SPIcam.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CAM_CS, LOW);
    delayMicroseconds(1);
    SPIcam.transfer(0x07 | 0x80);
    SPIcam.transfer(0x00);
    delayMicroseconds(1);
    digitalWrite(PIN_CAM_CS, HIGH);
    SPIcam.endTransaction();
    
    delay(100);
    Serial.println("OK");
    Serial.flush();

    // Configure OV2640 for RGB565
    Serial.print("Configuring OV2640...");
    Serial.flush();
    
    // Software reset
    ov2640_write_reg(0xFF, 0x01);  // Sensor bank
    ov2640_write_reg(0x12, 0x80);  // Reset
    delay(100);
    
    // Basic initialization
    ov2640_write_reg(0xFF, 0x00);  // DSP bank
    ov2640_write_reg(0x2C, 0xFF);
    ov2640_write_reg(0x2E, 0xDF);
    
    ov2640_write_reg(0xFF, 0x01);  // Sensor bank
    ov2640_write_reg(0x3C, 0x32);
    ov2640_write_reg(0x11, 0x00);  // Clock prescaler
    ov2640_write_reg(0x09, 0x02);  // Drive capability
    ov2640_write_reg(0x04, 0x00);  // Mirror/flip
    ov2640_write_reg(0x13, 0xE5);  // Enable AGC, AWB, AEC
    ov2640_write_reg(0x14, 0x48);  // AGC control
    ov2640_write_reg(0x2C, 0x0C);
    ov2640_write_reg(0x33, 0x78);
    ov2640_write_reg(0x3A, 0x33);
    ov2640_write_reg(0x3B, 0xFB);
    ov2640_write_reg(0x3E, 0x00);
    ov2640_write_reg(0x43, 0x11);
    ov2640_write_reg(0x16, 0x10);
    
    ov2640_write_reg(0x4F, 0x80);
    ov2640_write_reg(0x50, 0x80);
    ov2640_write_reg(0x51, 0x00);
    ov2640_write_reg(0x52, 0x22);
    ov2640_write_reg(0x53, 0x5E);
    ov2640_write_reg(0x54, 0x80);
    ov2640_write_reg(0x58, 0x9E);
    
    ov2640_write_reg(0xFF, 0x00);  // DSP bank
    ov2640_write_reg(0xC0, 0xC8);  // HSIZE
    ov2640_write_reg(0xC1, 0x96);  // VSIZE
    ov2640_write_reg(0x8C, 0x00);
    ov2640_write_reg(0x86, 0x3D);
    ov2640_write_reg(0x50, 0x89);
    ov2640_write_reg(0x51, 0x90);
    ov2640_write_reg(0x52, 0x2C);
    ov2640_write_reg(0x53, 0x00);
    ov2640_write_reg(0x54, 0x00);
    ov2640_write_reg(0x55, 0x88);
    
    ov2640_write_reg(0x57, 0x00);
    ov2640_write_reg(0x5A, 0x50);  // ZMOW (320/4)
    ov2640_write_reg(0x5B, 0x3C);  // ZMOH (240/4)
    ov2640_write_reg(0x5C, 0x00);
    
    ov2640_write_reg(0xD3, 0x04);  // DVP output format
    
    ov2640_write_reg(0xE0, 0x00);  // No test pattern
    
    // Set RGB565 output format
    ov2640_write_reg(0xDA, 0x09);  // Image output format: RGB565
    
    // Set QVGA (320x240)
    ov2640_write_reg(0xFF, 0x01);
    ov2640_write_reg(0x12, 0x40);  // QVGA mode
    ov2640_write_reg(0x03, 0x0F);
    ov2640_write_reg(0x32, 0x09);
    ov2640_write_reg(0x17, 0x11);
    ov2640_write_reg(0x18, 0x43);
    ov2640_write_reg(0x19, 0x00);
    ov2640_write_reg(0x1A, 0x25);
    ov2640_write_reg(0x37, 0xC0);
    ov2640_write_reg(0x4F, 0xCA);
    ov2640_write_reg(0x50, 0xA8);
    ov2640_write_reg(0x5A, 0x23);
    ov2640_write_reg(0x6D, 0x00);
    ov2640_write_reg(0x3D, 0x38);
    
    ov2640_write_reg(0xFF, 0x00);
    ov2640_write_reg(0xE5, 0x7F);
    ov2640_write_reg(0xF9, 0xC0);
    ov2640_write_reg(0x41, 0x24);
    ov2640_write_reg(0xE0, 0x14);
    ov2640_write_reg(0x76, 0xFF);
    ov2640_write_reg(0x33, 0xA0);
    ov2640_write_reg(0x42, 0x20);
    ov2640_write_reg(0x43, 0x18);
    ov2640_write_reg(0x4C, 0x00);
    ov2640_write_reg(0x87, 0xD0);
    ov2640_write_reg(0x88, 0x3F);
    ov2640_write_reg(0xD7, 0x03);
    ov2640_write_reg(0xD9, 0x10);
    ov2640_write_reg(0xD3, 0x82);
    ov2640_write_reg(0xC8, 0x08);
    ov2640_write_reg(0xC9, 0x80);
    ov2640_write_reg(0x7C, 0x00);
    ov2640_write_reg(0x7D, 0x00);
    ov2640_write_reg(0x7C, 0x03);
    ov2640_write_reg(0x7D, 0x48);
    ov2640_write_reg(0x7D, 0x48);
    ov2640_write_reg(0x7C, 0x08);
    ov2640_write_reg(0x7D, 0x20);
    ov2640_write_reg(0x7D, 0x10);
    ov2640_write_reg(0x7D, 0x0E);
    
    Serial.println("OK");
    Serial.flush();
    
    delay(300);
    
    // Clear FIFO
    Serial.print("Clearing FIFO...");
    Serial.flush();
    
    SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    camWriteReg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
    SPIcam.endTransaction();
    
    Serial.println("OK");
    Serial.flush();
    
    Serial.println("Camera configured for 320x240 RGB565");
    Serial.flush();
}

// ============================================================
// CAMERA CAPTURE USING FSPI
// ============================================================

void captureFrameToBuffer() {
    SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    
    // Clear FIFO write done flag
    camWriteReg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
    
    SPIcam.endTransaction();
    delay(1);
    
    // Start capture
    SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    camWriteReg(ARDUCHIP_FIFO, FIFO_START_MASK);
    SPIcam.endTransaction();
    
    // Wait for capture done with timeout
    uint32_t timeout = millis() + 5000;
    
    while (true) {
        SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        uint8_t status = camGetBit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPIcam.endTransaction();
        
        if (status) break;
        
        if (millis() > timeout) {
            Serial.println("ERROR: Capture timeout!");
            return;
        }
        delay(5);
    }
    
    // Read FIFO length
    SPIcam.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    
    uint32_t fifo_len = 0;
    fifo_len = camReadReg(ARDUCHIP_FIFO) & 0x7F;
    fifo_len = (fifo_len << 8) | camReadReg(ARDUCHIP_FIFO + 1);
    fifo_len = (fifo_len << 8) | camReadReg(ARDUCHIP_FIFO + 2);
    
    SPIcam.endTransaction();
    
    if (fifo_len == 0) {
        Serial.println("ERROR: FIFO empty!");
        return;
    }
    
    Serial.print("FIFO=");
    Serial.print(fifo_len);
    Serial.print(" ");
    
    // High-speed burst read
    SPIcam.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    
    digitalWrite(PIN_CAM_CS, LOW);
    delayMicroseconds(1);
    SPIcam.transfer(BURST_FIFO_READ);
    
    // Read frame data
    uint32_t remaining = FRAME_BYTES;
    uint8_t *dst = frameBuf;
    
    while (remaining > 0) {
        size_t len = (remaining > DMA_CHUNK) ? DMA_CHUNK : remaining;
        SPIcam.transferBytes(dummyTx, dst, len);
        dst += len;
        remaining -= len;
    }
    
    // Discard extra bytes
    if (fifo_len > FRAME_BYTES) {
        uint32_t extra = fifo_len - FRAME_BYTES;
        while (extra > 0) {
            size_t len = (extra > DMA_CHUNK) ? DMA_CHUNK : extra;
            SPIcam.transferBytes(dummyTx, dummyTx, len);
            extra -= len;
        }
    }
    
    delayMicroseconds(1);
    digitalWrite(PIN_CAM_CS, HIGH);
    SPIcam.endTransaction();
}

// ============================================================
// LCD: ROTATE 320×240 → 240×320 (90° CLOCKWISE, NO SCALING)
// ============================================================

void streamFrameToLcd(const uint8_t *src) {
    uint8_t *dmaBuf = getDMABuffer();
    
    if (dmaBuf == nullptr) {
        Serial.println("ERROR: DMA buffer null!");
        return;
    }
    
    const size_t dmaPixels = DMA_BUFFER_SIZE / 2;

    LCD_SetCursor(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_DC_PIN, 1);

    // Rotate 90° clockwise: 320×240 camera → 240×320 LCD
    // Camera: 320 wide (X: 0-319), 240 tall (Y: 0-239)
    // LCD: 240 wide (X: 0-239), 320 tall (Y: 0-319)
    //
    // Rotation formula (90° clockwise):
    // LCD(lcdX, lcdY) = Camera(lcdY, 239 - lcdX)
    
    for (uint16_t lcdY = 0; lcdY < LCD_HEIGHT; lcdY++) {
        // Camera X comes from LCD Y (0-319)
        uint16_t camX = lcdY;
        
        // Skip if camera X is out of bounds
        if (camX >= SRC_W) continue;
        
        uint16_t x = 0;
        while (x < LCD_WIDTH) {
            size_t chunk = min((uint16_t)(LCD_WIDTH - x), (uint16_t)dmaPixels);

            for (size_t i = 0; i < chunk; i++) {
                uint16_t lcdX = x + i;
                
                // Camera Y comes from LCD X (reversed)
                // LCD X: 0-239 → Camera Y: 239-0
                uint16_t camY = (SRC_H - 1) - lcdX;
                
                // Calculate source pixel index
                uint32_t idx = ((uint32_t)camY * SRC_W + camX) * 2;

                if (idx + 1 < FRAME_BYTES) {
                    dmaBuf[2*i]     = src[idx];
                    dmaBuf[2*i + 1] = src[idx + 1];
                } else {
                    dmaBuf[2*i]     = 0;
                    dmaBuf[2*i + 1] = 0;
                }
            }

            DEV_SPI_Write_DMA(dmaBuf, chunk * 2);
            x += chunk;
        }
    }

    DEV_Digital_Write(DEV_CS_PIN, 1);
}