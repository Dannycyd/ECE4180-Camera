/**************************************************************************
 * ESP32-S3 Dual-Core Stitch Cam with Gallery
 *
 * NEW FEATURES:
 * - Two-page website: Camera + Gallery
 * - Gallery shows all photos (3 per row)
 * - Click photo → full-screen lightbox with ◄ ► navigation
 * - Photos NOT auto-deleted (keep all captures)
 * - Cute Stitch theme throughout
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
uint32_t photoCounter = 1;  // For unique filenames

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

// ====================== HTML Pages ================

// Camera Page
const char camera_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Stitch Cam</title>
<style>
:root{--blue-dark:#0b3056;--blue-main:#2c6fff;--blue-soft:#74a9ff;--blue-pastel:#dbeaff;--white:#fff;--accent-pink:#ff87b7}
*{box-sizing:border-box}
body{margin:0;padding:0;font-family:system-ui,-apple-system,sans-serif;background:radial-gradient(circle at top,#4b91ff 0,#0b2140 45%,#020712 100%);color:var(--white);min-height:100vh;display:flex;flex-direction:column}
.bubble-bg{position:fixed;inset:0;overflow:hidden;pointer-events:none;z-index:-1}
.bubble{position:absolute;border-radius:50%;background:radial-gradient(circle,rgba(255,255,255,0.85) 0,rgba(116,169,255,0.1) 60%);opacity:0.15;animation:floatUp 18s linear infinite}
.bubble:nth-child(1){width:140px;height:140px;left:5%;bottom:-150px;animation-delay:0s}
.bubble:nth-child(2){width:90px;height:90px;left:75%;bottom:-120px;animation-delay:3s}
.bubble:nth-child(3){width:60px;height:60px;left:18%;bottom:-80px;animation-delay:6s}
.bubble:nth-child(4){width:190px;height:190px;left:55%;bottom:-200px;animation-delay:9s}
@keyframes floatUp{from{transform:translateY(0)}to{transform:translateY(-120vh)}}
.nav{display:flex;gap:10px;padding:15px 20px;background:rgba(12,42,82,0.8);backdrop-filter:blur(10px)}
.nav a{flex:1;padding:12px;text-align:center;background:linear-gradient(135deg,#1b3259,#10213b);border-radius:12px;color:#fff;text-decoration:none;font-weight:600;font-size:0.9rem;transition:0.2s}
.nav a:hover{background:linear-gradient(135deg,#2c6fff,#4b91ff);transform:translateY(-2px)}
.nav a.active{background:linear-gradient(135deg,var(--accent-pink),#ffd3f0);color:#151528}
.container{flex:1;display:flex;align-items:center;justify-content:center;padding:20px}
.card{width:min(420px,92vw);background:linear-gradient(145deg,rgba(12,42,82,0.98),rgba(21,65,120,0.98));border-radius:26px;padding:22px 20px 18px;box-shadow:0 20px 40px rgba(0,0,0,0.55),0 0 0 1px rgba(255,255,255,0.05);position:relative;overflow:hidden}
.card::before{content:"";position:absolute;inset:0;background:radial-gradient(circle at 0 0,rgba(255,255,255,0.14) 0,transparent 60%),radial-gradient(circle at 100% 100%,rgba(116,169,255,0.18) 0,transparent 58%);pointer-events:none}
.header{display:flex;align-items:center;gap:12px;margin-bottom:16px;position:relative;z-index:1}
.avatar{width:54px;height:54px;border-radius:50%;padding:3px;background:radial-gradient(circle at 20% 0,#fff 0,#9fd3ff 40%,#2558a8 100%);box-shadow:0 6px 15px rgba(0,0,0,0.35);overflow:hidden}
.avatar img{width:100%;height:100%;object-fit:cover;border-radius:50%}
.title-block h1{margin:0;font-size:1.3rem;letter-spacing:0.06em;text-transform:uppercase}
.title-block p{margin:2px 0 0;font-size:0.8rem;opacity:0.8}
.badge{margin-left:auto;padding:4px 10px;border-radius:999px;background:linear-gradient(135deg,var(--accent-pink),#ffd3f0);color:#151528;font-size:0.73rem;font-weight:600;box-shadow:0 4px 8px rgba(0,0,0,0.3)}
.preview{position:relative;border-radius:20px;background:#000;margin-bottom:16px;box-shadow:inset 0 0 0 1px rgba(255,255,255,0.35),0 12px 25px rgba(0,0,0,0.5);overflow:hidden;width:100%;aspect-ratio:4/3}
.preview-inner{position:absolute;inset:0;padding:4px}
#previewImg{width:100%;height:100%;object-fit:contain;border-radius:18px}
.countdown-display{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:4rem;font-weight:800;text-shadow:0 0 15px rgba(0,0,0,0.8);opacity:0;transform:scale(0.7);transition:opacity 0.2s,transform 0.2s;pointer-events:none}
.countdown-display.active{opacity:1;transform:scale(1)}
.controls-row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px;position:relative;z-index:1}
.toggle-group{display:flex;align-items:center;gap:8px}
.toggle-wrapper{display:inline-flex;flex-direction:column;gap:2px}
.toggle-label{font-size:0.8rem;text-transform:uppercase;letter-spacing:0.08em;opacity:0.9}
.toggle-sub{font-size:0.68rem;opacity:0.7}
.switch{position:relative;width:50px;height:26px;border-radius:999px;background:linear-gradient(135deg,#1b3259,#10213b);box-shadow:inset 0 0 0 1px rgba(255,255,255,0.14),0 4px 10px rgba(0,0,0,0.6);cursor:pointer;transition:0.2s}
.switch-knob{position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:radial-gradient(circle at 30% 0,#fff 0,#dbeaff 45%,#7fb0ff 100%);box-shadow:0 3px 6px rgba(0,0,0,0.5);transition:0.2s}
.switch.on{background:linear-gradient(135deg,#3f89ff,#9ed2ff)}
.switch.on .switch-knob{transform:translateX(24px)}
.capture-btn{flex:1;display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:10px 14px;border-radius:999px;border:none;font-size:0.9rem;font-weight:600;letter-spacing:0.07em;text-transform:uppercase;cursor:pointer;background:radial-gradient(circle at 30% 0,#fff 0,#dbeaff 40%,#2c6fff 100%);color:#082041;box-shadow:0 8px 18px rgba(0,0,0,0.6),0 0 0 1px rgba(255,255,255,0.45);transition:0.08s}
.capture-btn:active{transform:translateY(1px) scale(0.98);box-shadow:0 4px 8px rgba(0,0,0,0.5);filter:brightness(0.97)}
.capture-icon{width:18px;height:18px;border-radius:50%;border:2px solid #082041;display:inline-flex;align-items:center;justify-content:center}
.capture-icon::before{content:"";width:8px;height:8px;border-radius:50%;background:#082041}
.footer-row{display:flex;align-items:center;justify-content:space-between;gap:10px;font-size:0.72rem;opacity:0.85;position:relative;z-index:1}
.status-chip{padding:4px 10px;border-radius:999px;background:rgba(12,42,82,0.9);border:1px solid rgba(191,214,255,0.3);display:inline-flex;align-items:center;gap:6px}
.status-dot{width:8px;height:8px;border-radius:50%;background:#60ff9b;box-shadow:0 0 8px rgba(96,255,155,0.7)}
.status-chip.counting .status-dot{background:#ffe066;box-shadow:0 0 8px rgba(255,224,102,0.7)}
.pill{padding:2px 8px;border-radius:999px;border:1px solid rgba(191,214,255,0.35)}
</style>
</head>
<body>
<div class="bubble-bg">
<div class="bubble"></div>
<div class="bubble"></div>
<div class="bubble"></div>
<div class="bubble"></div>
</div>

<nav class="nav">
<a href="/" class="active">📷 Camera</a>
<a href="/gallery">🖼️ Gallery</a>
</nav>

<div class="container">
<main class="card">
<header class="header">
<div class="avatar">
<img src="https://www.pikpng.com/pngl/b/48-485933_stitch-png-clipart-stitch-cartoon-png-transparent-png.png" alt="Stitch">
</div>
<div class="title-block">
<h1>Stitch Cam</h1>
<p>Take cute photos!</p>
</div>
<div class="badge">LIVE</div>
</header>

<section class="preview">
<div class="preview-inner">
<img id="previewImg" src="" alt="Preview">
</div>
<div id="countdown" class="countdown-display">3</div>
</section>

<section class="controls-row">
<div class="toggle-group">
<div class="toggle-wrapper">
<span class="toggle-label">3s Countdown</span>
<span class="toggle-sub" id="modeLabel">Enabled</span>
</div>
<div id="modeSwitch" class="switch on" onclick="toggleMode()">
<div class="switch-knob"></div>
</div>
</div>
<button class="capture-btn" onclick="capture()">
<span class="capture-icon"></span>
<span>Capture</span>
</button>
</section>

<section class="footer-row">
<div id="statusChip" class="status-chip">
<span class="status-dot"></span>
<span id="statusText">Idle</span>
</div>
<div class="pill" id="photoCount">Photos: 0</div>
</section>
</main>
</div>

<script>
let countdownEnabled=true,busy=false;
function updateStatus(){
fetch('/status').then(r=>r.json()).then(d=>{
countdownEnabled=(d.mode==='Countdown');
document.getElementById('modeSwitch').className=countdownEnabled?'switch on':'switch';
document.getElementById('modeLabel').textContent=countdownEnabled?'Enabled':'Disabled';
document.getElementById('photoCount').textContent='Photos: '+d.photos;
document.getElementById('statusText').textContent=d.status;
}).catch(()=>{});}
function refreshStream(){
document.getElementById('previewImg').src='/stream?t='+Date.now();}
function toggleMode(){
fetch('/toggle').then(()=>setTimeout(updateStatus,300));}
function capture(){
if(busy)return;busy=true;
if(countdownEnabled){
let cd=document.getElementById('countdown'),c=3;
cd.textContent=c;cd.className='countdown-display active';
document.getElementById('statusChip').classList.add('counting');
fetch('/countdown_start');
let t=setInterval(()=>{c--;
if(c>0)cd.textContent=c;
else{clearInterval(t);cd.className='countdown-display';
document.getElementById('statusChip').classList.remove('counting');
setTimeout(()=>{updateStatus();busy=false;},1500);}
},1000);
}else{
fetch('/capture').then(()=>setTimeout(()=>{updateStatus();busy=false;},1000));
}}
updateStatus();
setInterval(updateStatus,3000);
refreshStream();
setInterval(refreshStream,500);
</script>
</body>
</html>
)rawliteral";

// Gallery Page
const char gallery_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Stitch Gallery</title>
<style>
:root{--blue-dark:#0b3056;--blue-main:#2c6fff;--blue-soft:#74a9ff;--blue-pastel:#dbeaff;--white:#fff;--accent-pink:#ff87b7}
*{box-sizing:border-box}
body{margin:0;padding:0;font-family:system-ui,-apple-system,sans-serif;background:radial-gradient(circle at top,#4b91ff 0,#0b2140 45%,#020712 100%);color:var(--white);min-height:100vh;display:flex;flex-direction:column}
.bubble-bg{position:fixed;inset:0;overflow:hidden;pointer-events:none;z-index:-1}
.bubble{position:absolute;border-radius:50%;background:radial-gradient(circle,rgba(255,255,255,0.85) 0,rgba(116,169,255,0.1) 60%);opacity:0.15;animation:floatUp 18s linear infinite}
.bubble:nth-child(1){width:140px;height:140px;left:5%;bottom:-150px;animation-delay:0s}
.bubble:nth-child(2){width:90px;height:90px;left:75%;bottom:-120px;animation-delay:3s}
.bubble:nth-child(3){width:60px;height:60px;left:18%;bottom:-80px;animation-delay:6s}
.bubble:nth-child(4){width:190px;height:190px;left:55%;bottom:-200px;animation-delay:9s}
@keyframes floatUp{from{transform:translateY(0)}to{transform:translateY(-120vh)}}
.nav{display:flex;gap:10px;padding:15px 20px;background:rgba(12,42,82,0.8);backdrop-filter:blur(10px)}
.nav a{flex:1;padding:12px;text-align:center;background:linear-gradient(135deg,#1b3259,#10213b);border-radius:12px;color:#fff;text-decoration:none;font-weight:600;font-size:0.9rem;transition:0.2s}
.nav a:hover{background:linear-gradient(135deg,#2c6fff,#4b91ff);transform:translateY(-2px)}
.nav a.active{background:linear-gradient(135deg,var(--accent-pink),#ffd3f0);color:#151528}
.container{flex:1;padding:20px;overflow-y:auto}
.header{text-align:center;margin-bottom:30px}
.header h1{margin:0 0 10px;font-size:2rem;text-shadow:0 2px 10px rgba(0,0,0,0.5)}
.header p{margin:0;opacity:0.8;font-size:1rem}
.gallery{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:15px;max-width:1200px;margin:0 auto}
.photo-card{aspect-ratio:4/3;border-radius:15px;overflow:hidden;background:linear-gradient(145deg,rgba(12,42,82,0.98),rgba(21,65,120,0.98));box-shadow:0 4px 15px rgba(0,0,0,0.4);cursor:pointer;transition:0.2s;position:relative}
.photo-card:hover{transform:translateY(-5px);box-shadow:0 8px 25px rgba(0,0,0,0.6)}
.photo-card img{width:100%;height:100%;object-fit:cover}
.photo-card .delete-btn{position:absolute;top:5px;right:5px;width:30px;height:30px;border-radius:50%;background:rgba(255,107,107,0.9);border:none;color:#fff;font-size:1.2rem;cursor:pointer;display:flex;align-items:center;justify-content:center;opacity:0;transition:0.2s}
.photo-card:hover .delete-btn{opacity:1}
.photo-card .delete-btn:hover{background:rgba(255,50,50,1);transform:scale(1.1)}
.empty{text-align:center;padding:60px 20px;opacity:0.6}
.empty-icon{font-size:4rem;margin-bottom:20px}
.lightbox{display:none;position:fixed;inset:0;background:rgba(0,0,0,0.95);z-index:1000;align-items:center;justify-content:center;padding:20px}
.lightbox.active{display:flex}
.lightbox-content{position:relative;max-width:90vw;max-height:90vh}
.lightbox-img{max-width:100%;max-height:90vh;border-radius:20px;box-shadow:0 10px 50px rgba(0,0,0,0.8)}
.nav-btn{position:absolute;top:50%;transform:translateY(-50%);width:50px;height:50px;border-radius:50%;background:linear-gradient(135deg,var(--accent-pink),#ffd3f0);border:none;color:#151528;font-size:1.5rem;cursor:pointer;box-shadow:0 4px 15px rgba(0,0,0,0.5);transition:0.2s;display:flex;align-items:center;justify-content:center;font-weight:bold}
.nav-btn:hover{transform:translateY(-50%) scale(1.1)}
.nav-btn.prev{left:-70px}
.nav-btn.next{right:-70px}
.close-btn{position:absolute;top:-50px;right:0;width:40px;height:40px;border-radius:50%;background:rgba(255,255,255,0.2);border:none;color:#fff;font-size:1.5rem;cursor:pointer;transition:0.2s}
.close-btn:hover{background:rgba(255,255,255,0.3);transform:scale(1.1)}
@media (max-width:768px){
.gallery{grid-template-columns:repeat(3,1fr);gap:10px}
.nav-btn{width:40px;height:40px;font-size:1.2rem}
.nav-btn.prev{left:10px}
.nav-btn.next{right:10px}
.close-btn{top:10px;right:10px}
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

<nav class="nav">
<a href="/">📷 Camera</a>
<a href="/gallery" class="active">🖼️ Gallery</a>
</nav>

<div class="container">
<div class="header">
<h1>✨ Stitch's Photo Album ✨</h1>
<p id="photoInfo">Loading photos...</p>
</div>
<div id="gallery" class="gallery"></div>
</div>

<div id="lightbox" class="lightbox" onclick="closeLightbox(event)">
<div class="lightbox-content">
<button class="close-btn" onclick="closeLightbox(event)">×</button>
<button class="nav-btn prev" onclick="prevPhoto(event)">◄</button>
<img id="lightboxImg" class="lightbox-img" src="" alt="">
<button class="nav-btn next" onclick="nextPhoto(event)">►</button>
</div>
</div>

<script>
let photos=[],currentIndex=0;
function loadGallery(){
fetch('/photos').then(r=>r.json()).then(d=>{
photos=d.photos;
document.getElementById('photoInfo').textContent=photos.length+' photo'+(photos.length!==1?'s':'');
const gallery=document.getElementById('gallery');
if(photos.length===0){
gallery.innerHTML='<div class="empty"><div class="empty-icon">📸</div><div>No photos yet!<br>Take some with Stitch Cam!</div></div>';
return;}
gallery.innerHTML='';
photos.forEach((photo,i)=>{
const card=document.createElement('div');
card.className='photo-card';
card.innerHTML=`<img src="/photo?file=${photo}" alt="Photo ${i+1}"><button class="delete-btn" onclick="deletePhoto(event,'${photo}')">×</button>`;
card.querySelector('img').onclick=()=>openLightbox(i);
gallery.appendChild(card);
});
}).catch(()=>{
document.getElementById('gallery').innerHTML='<div class="empty"><div class="empty-icon">⚠️</div><div>Failed to load photos</div></div>';
});}
function openLightbox(index){
currentIndex=index;
document.getElementById('lightboxImg').src='/photo?file='+photos[currentIndex];
document.getElementById('lightbox').className='lightbox active';}
function closeLightbox(e){
if(e.target.id==='lightbox'||e.target.className.includes('close-btn')){
document.getElementById('lightbox').className='lightbox';}}
function prevPhoto(e){
e.stopPropagation();
currentIndex=(currentIndex-1+photos.length)%photos.length;
document.getElementById('lightboxImg').src='/photo?file='+photos[currentIndex];}
function nextPhoto(e){
e.stopPropagation();
currentIndex=(currentIndex+1)%photos.length;
document.getElementById('lightboxImg').src='/photo?file='+photos[currentIndex];}
function deletePhoto(e,filename){
e.stopPropagation();
if(!confirm('Delete this photo?'))return;
fetch('/delete?file='+filename).then(()=>loadGallery());}
loadGallery();
</script>
</body>
</html>
)rawliteral";

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

// ====================== setup() ===================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║  Stitch Cam with Gallery                ║");
  Serial.println("╚══════════════════════════════════════════╝\n");

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON1), button1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON2), button2ISR, FALLING);

  initLED();
  setLED(true, false, false); delay(200);
  setLED(false, true, false); delay(200);
  setLED(false, false, true); delay(200);
  setLED(false, false, false);

  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_CAM_CS, HIGH);

  Serial.println("1. Init I2C...");
  Wire.begin(PIN_SDA, PIN_SCL);
  delay(100);
  Serial.println("   ✓ OK");

  Serial.println("2. Init SPI...");
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  delay(100);
  Serial.println("   ✓ OK");

  Serial.println("3. Init SD Card...");
  initSDCard();

  Serial.println("4. Test ArduCAM...");
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(1);
  uint8_t test = myCAM.read_reg(ARDUCHIP_TEST1);
  SPI.endTransaction();
  if (test != 0x55) {
    Serial.printf("   ✗ ERROR: 0x%02X\n", test);
    while (1) delay(1000);
  }
  Serial.println("   ✓ OK");

  Serial.println("5. Check OV2640...");
  Wire.beginTransmission(0x30);
  if (Wire.endTransmission() != 0) {
    Serial.println("   ✗ ERROR!");
    while (1) delay(1000);
  }
  Serial.println("   ✓ OK");

  Serial.println("6. Init Camera...");
  initCamera();

  Serial.println("7. Init LCD...");
  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  LCD_Clear(BLACK);
  Serial.println("   ✓ OK");

  Serial.println("8. Init JPEG Decoder...");
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);
  Serial.println("   ✓ OK");

  Serial.println("9. WiFi...");
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
    Serial.printf("   ✓ IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("   ✗ Failed");
  }

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
  Serial.println("   ✓ HTTP server started");

  Serial.println("\n10. Start camera task...");
  xTaskCreatePinnedToCore(cameraTask, "CameraTask", 8192, NULL, 1, &cameraTaskHandle, 0);

  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║        READY                             ║");
  Serial.println("╚══════════════════════════════════════════╝\n");
}

// ====================== loop() (Core 1) ===========
void loop() {
  server.handleClient();
}

// ====================== cameraTask (Core 0) =======
void cameraTask(void* parameter) {
  static uint32_t frameCount = 0;
  static uint32_t lastPrintTime = 0;

  for (;;) {
    if (webCaptureRequested) {
      webCaptureRequested = false;
      if (currentMode == MODE_INSTANT) {
        handleCaptureInstant();
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

    if (button2Pressed) {
      button2Pressed = false;
      currentMode = (currentMode == MODE_INSTANT) ? MODE_COUNTDOWN : MODE_INSTANT;
      Serial.printf("\n[MODE] %s\n", (currentMode == MODE_INSTANT) ? "INSTANT" : "COUNTDOWN");
      setLED(false, false, true);
      delay(300);
      setLED(false, false, false);
    }

    if (button1Pressed) {
      button1Pressed = false;
      if (currentMode == MODE_INSTANT) {
        handleCaptureInstant();
      } else {
        handleCaptureCountdown();
      }
      totalPhotosSaved++;
    }

    if (!captureJpegToBuffer()) {
      delay(50);
      continue;
    }

    if (!decodeJpegToRGB565()) {
      delay(50);
      continue;
    }

    streamFrameToLcd_Optimized(frameBuf);
    frameCount++;

    if (millis() - lastPrintTime > 5000) {
      lastPrintTime = millis();
      Serial.printf("[Camera] Frame:%d | Photos:%d\n", frameCount, totalPhotosSaved);
    }

    vTaskDelay(1);
  }
}

// ====================== Web Handlers ==============

void handleRoot() {
  server.send_P(200, "text/html", camera_html);
}

void handleGallery() {
  server.send_P(200, "text/html", gallery_html);
}

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
  String json = "{\"mode\":\"" + mode + "\",\"status\":\"" + lastCaptureStatus + "\",\"photos\":" + String(totalPhotosSaved) + "}";
  server.send(200, "application/json", json);
}

void handleStream() {
  uint32_t len = jpegLen;
  if (len == 0 || len > MAX_JPEG_SIZE) {
    server.send(503, "text/plain", "No frame");
    return;
  }
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)jpegBuf, len);
}

void handlePhotoList() {
  if (!sdCardAvailable) {
    server.send(200, "application/json", "{\"photos\":[]}");
    return;
  }

  digitalWrite(PIN_CAM_CS, HIGH);

  String json = "{\"photos\":[";
  File root = SD.open("/photos");
  if (root) {
    File file = root.openNextFile();
    bool first = true;
    while (file) {
      if (!file.isDirectory()) {
        if (!first) json += ",";
        json += "\"" + String(file.name()) + "\"";
        first = false;
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handlePhoto() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }

  String filename = "/photos/" + server.arg("file");
  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.exists(filename)) {
    server.send(404, "text/plain", "Not found");
    return;
  }

  File file = SD.open(filename);
  if (!file) {
    server.send(500, "text/plain", "Failed to open");
    return;
  }

  server.streamFile(file, "image/jpeg");
  file.close();
}

void handleDeletePhoto() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }

  String filename = "/photos/" + server.arg("file");
  digitalWrite(PIN_CAM_CS, HIGH);

  if (SD.remove(filename)) {
    totalPhotosSaved = (totalPhotosSaved > 0) ? totalPhotosSaved - 1 : 0;
    server.send(200, "text/plain", "Deleted");
  } else {
    server.send(500, "text/plain", "Failed");
  }
}

// ====================== Capture Functions =========

void handleCaptureInstant() {
  Serial.println("\n*** INSTANT CAPTURE ***");
  lastCaptureStatus = "Capturing...";

  if (sdCardAvailable) {
    showSaveMessage();

    if (savePhoto()) {
      Serial.printf("✓ Saved photo_%d.jpg\n", photoCounter - 1);
      lastCaptureStatus = "Saved!";
    } else {
      Serial.println("✗ Failed");
      lastCaptureStatus = "Failed";
    }

    delay(1000);
    lastCaptureStatus = "Idle";
  }
}

void handleCaptureCountdown() {
  Serial.println("\n*** COUNTDOWN ***");
  lastCaptureStatus = "Countdown...";

  for (int second = 3; second >= 1; second--) {
    for (int blink = 0; blink < 3; blink++) {
      setLED(true, false, false);
      uint32_t t = millis();
      while (millis() - t < 167) {
        if (captureJpegToBuffer() && decodeJpegToRGB565()) {
          streamFrameToLcd_Optimized(frameBuf);
        }
      }
      setLED(false, false, false);
      t = millis();
      while (millis() - t < 167) {
        if (captureJpegToBuffer() && decodeJpegToRGB565()) {
          streamFrameToLcd_Optimized(frameBuf);
        }
      }
    }
  }

  if (captureJpegToBuffer()) {
    setLED(false, true, false);

    if (sdCardAvailable) {
      showSaveMessage();

      if (savePhoto()) {
        Serial.printf("✓ Saved photo_%d.jpg\n", photoCounter - 1);
        lastCaptureStatus = "Saved!";
      } else {
        Serial.println("✗ Failed");
        lastCaptureStatus = "Failed";
      }

      delay(1000);
    }
  }

  setLED(false, false, false);
  lastCaptureStatus = "Idle";
}

// ====================== LED & Display =============

void initLED() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  setLED(false, false, false);
}

void setLED(bool red, bool green, bool blue) {
  digitalWrite(PIN_LED_RED, red ? LOW : HIGH);
  digitalWrite(PIN_LED_GREEN, green ? LOW : HIGH);
  digitalWrite(PIN_LED_BLUE, blue ? LOW : HIGH);
}

void showSaveMessage() {
  const char* text = "SAVING...";
  UBYTE* oldImage = Paint.Image;
  Paint.Image = frameBuf;
  Paint.WidthMemory = FRAME_W;
  Paint.HeightMemory = FRAME_H;
  Paint.Width = FRAME_W;
  Paint.Height = FRAME_H;
  Paint.WidthByte = FRAME_W;
  Paint.HeightByte = FRAME_H;
  Paint.Color = 0;
  Paint.Rotate = ROTATE_0;
  Paint.Mirror = MIRROR_NONE;

  Paint_DrawString_EN(110, 150, text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(111, 150, text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(110, 151, text, &Font24, BLACK, WHITE);
  Paint_DrawString_EN(111, 151, text, &Font24, BLACK, WHITE);

  Paint.Image = oldImage;
  streamFrameToLcd_Optimized(frameBuf);
}

// ====================== SD Card ==================

bool savePhoto() {
  if (!sdCardAvailable) return false;

  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }

  char filename[32];
  snprintf(filename, sizeof(filename), "/photos/photo_%d.jpg", photoCounter++);

  File file = SD.open(filename, FILE_WRITE);
  if (!file) return false;

  size_t written = file.write(jpegBuf, jpegLen);
  file.close();

  return (written == jpegLen);
}

void initSDCard() {
  digitalWrite(PIN_CAM_CS, HIGH);

  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("   ✗ SD init failed");
    sdCardAvailable = false;
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("   ✗ No card");
    sdCardAvailable = false;
    return;
  }

  Serial.printf("   Size: %llu MB\n", SD.cardSize() / (1024 * 1024));

  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }

  // Count existing photos
  File root = SD.open("/photos");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String name = file.name();
        if (name.startsWith("photo_") && name.endsWith(".jpg")) {
          int num = name.substring(6, name.length() - 4).toInt();
          if (num >= photoCounter) {
            photoCounter = num + 1;
          }
        }
        totalPhotosSaved++;
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  sdCardAvailable = true;
  Serial.printf("   ✓ SD ready (%d photos)\n", totalPhotosSaved);
}

// ====================== Camera ====================

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
  myCAM.read_fifo_length();
  myCAM.clear_fifo_flag();
  SPI.endTransaction();

  Serial.println("   ✓ Camera OK");
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

  if (len == 0 || len > MAX_JPEG_SIZE) return false;

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

  jpegLen = len;
  return (jpegBuf[0] == 0xFF && jpegBuf[1] == 0xD8);
}

bool decodeJpegToRGB565() {
  memset(frameBuf, 0, FRAME_BYTES);
  uint16_t w, h;
  if (TJpgDec.getJpgSize(&w, &h, jpegBuf, jpegLen) != JDR_OK) return false;
  return (TJpgDec.drawJpg(0, 0, jpegBuf, jpegLen) == JDR_OK);
}

void streamFrameToLcd_Optimized(const uint8_t *src) {
  uint8_t *dmaBuf = getDMABuffer();
  const uint16_t LCD_W = 240;
  const uint16_t LCD_H = 320;

  LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
  DEV_SPI_Write_Bulk_Start();

  for (uint16_t lcdRow = 0; lcdRow < LCD_H; lcdRow++) {
    uint16_t bufIdx = 0;
    for (uint16_t lcdCol = 0; lcdCol < LCD_W; lcdCol++) {
      uint16_t srcRow = LCD_W - 1 - lcdCol;
      uint16_t srcCol = lcdRow;
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
