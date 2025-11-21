#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

static const int PIN_SDA    = 22;
static const int PIN_SCL    = 23;
static const int PIN_SCK    = 21;
static const int PIN_MISO   = 20;
static const int PIN_MOSI   = 19;
static const int PIN_CAM_CS = 18;
// --------------------------------------------------------

// Image size
static const uint16_t SRC_W = 320;
static const uint16_t SRC_H = 240;
static const uint32_t FRAME_BYTES = (uint32_t)SRC_W * SRC_H * 2; // 153600

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// Frame buffer in RAM (RGB565: 2 bytes per pixel)
static uint8_t frameBuf[FRAME_BYTES];

// Dummy TX buffer for SPI.transferBytes (all zeros)
static const size_t DMA_CHUNK = 4096;
static uint8_t dummyTx[DMA_CHUNK] = {0};

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println();
  Serial.println(F("=== ESP32-C6 + ArduCAM OV2640 RGB565 test (no serial pixel output) ==="));

  // I2C for camera config
  Wire.begin(PIN_SDA, PIN_SCL);

  // SPI for camera FIFO
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CAM_CS);

  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_CAM_CS, HIGH);

  // Simple SPI link test
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  uint8_t temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55) {
    Serial.print(F("ArduCAM SPI test FAILED, got 0x"));
    Serial.println(temp, HEX);
    while (1) {
      delay(1000);
    }
  }
  Serial.println(F("ArduCAM SPI OK"));

  // Set to BMP (RGB) mode, NOT JPEG
  myCAM.set_format(BMP);
  myCAM.InitCAM();

  // Resolution: 320x240 (affects sensor registers even in BMP mode)
  myCAM.OV2640_set_JPEG_size(OV2640_320x240);
  delay(500);

  Serial.println(F("Camera configured: OV2640 320x240 RGB565"));
  Serial.print(F("Frame bytes = "));
  Serial.println(FRAME_BYTES);

  Serial.println(F("Starting continuous capture into RAM buffer..."));
}

void loop() {
  captureFrameToBuffer();
  // No delay here: we want max FPS. Add delay(10) if you want to slow down.
}

void captureFrameToBuffer() {
  uint32_t t0 = millis();

  // 1. Trigger capture
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  // 2. Wait for capture complete
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    // you could add a timeout here if you want
  }
  uint32_t t_capture_done = millis();

  // 3. Read FIFO length
  uint32_t fifo_len = myCAM.read_fifo_length();
  if (fifo_len < FRAME_BYTES) {
    Serial.print(F("[WARN] FIFO length too small: "));
    Serial.println(fifo_len);
    myCAM.clear_fifo_flag();
    return;
  }

  // 4. Burst read FIFO into frameBuf using transferBytes (DMA-capable on ESP32)
  SPI.beginTransaction(SPISettings(20 * 1000000UL, MSBFIRST, SPI_MODE0));

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  uint32_t bytesToRead = FRAME_BYTES;
  uint8_t *dst = frameBuf;

  while (bytesToRead > 0) {
    size_t thisLen = (bytesToRead > DMA_CHUNK) ? DMA_CHUNK : bytesToRead;

    // Read 'thisLen' bytes from FIFO into dst
    SPI.transferBytes(dummyTx, dst, thisLen);

    dst         += thisLen;
    bytesToRead -= thisLen;
  }

  // If FIFO has a few extra bytes (padding), discard them
  uint32_t extra = fifo_len - FRAME_BYTES;
  while (extra > 0) {
    size_t thisLen = (extra > DMA_CHUNK) ? DMA_CHUNK : extra;
    // Reuse dummyTx as RX sink; we don't care about these bytes
    SPI.transferBytes(dummyTx, dummyTx, thisLen);
    extra -= thisLen;
  }

  myCAM.CS_HIGH();
  SPI.endTransaction();

  myCAM.clear_fifo_flag();

  uint32_t t_read_done = millis();

  // 5. Simple checksum of first 1024 bytes (for sanity)
  uint32_t checksum = 0;
  const uint32_t CHECK_BYTES = 1024;
  uint32_t limit = (FRAME_BYTES < CHECK_BYTES) ? FRAME_BYTES : CHECK_BYTES;
  for (uint32_t i = 0; i < limit; i++) {
    checksum += frameBuf[i];
  }

  // 6. Print timings
  uint32_t t_capture = t_capture_done - t0;
  uint32_t t_read    = t_read_done    - t_capture_done;
  uint32_t t_total   = t_read_done    - t0;

  float fps = (t_total > 0) ? (1000.0f / (float)t_total) : 0.0f;

  Serial.print(F("FIFO len="));
  Serial.print(fifo_len);
  Serial.print(F(" bytes, capture="));
  Serial.print(t_capture);
  Serial.print(F(" ms, read="));
  Serial.print(t_read);
  Serial.print(F(" ms, total="));
  Serial.print(t_total);
  Serial.print(F(" ms, approx FPS="));
  Serial.print(fps, 2);
  Serial.print(F(", checksum[0.."));
  Serial.print(limit - 1);
  Serial.print(F("]="));
  Serial.println(checksum);
}