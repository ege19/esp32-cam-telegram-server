#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

#define FLASH_PIN 4

const char* WIFI_SSID = "Palabıyık";
const char* WIFI_PASSWORD = "19831983";
const char* BOT_TOKEN = "8827031277:AAHx9HntnJI8UJXuex-cm1T760fJTuFtDo4";
const char* CHAT_ID = "6798340496";

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

WebServer server(80);
bool flashOn = false;

struct CameraSettings {
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  int sharpness = 0;
  int quality = 20;
  int framesize = FRAMESIZE_VGA;
};

CameraSettings cameraSettings;
unsigned long uptime = 0;
uint32_t frameCounter = 0;
unsigned long lastFrameTime = 0;

void applyCameraSettings() {
  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) return;
  sensor->set_brightness(sensor, cameraSettings.brightness);
  sensor->set_contrast(sensor, cameraSettings.contrast);
  sensor->set_saturation(sensor, cameraSettings.saturation);
  sensor->set_sharpness(sensor, cameraSettings.sharpness);
  sensor->set_quality(sensor, cameraSettings.quality);
  sensor->set_framesize(sensor, (framesize_t)cameraSettings.framesize);
}

void setFlash(bool enable) {
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, enable ? HIGH : LOW);
  flashOn = enable;
}

bool sendPhotoToTelegram(const uint8_t* payload, size_t payloadLen) {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect("api.telegram.org", 443)) return false;

  String boundary = "----ESP32CAM";
  String body = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String endBoundary = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = body.length() + payloadLen + endBoundary.length();

  client.print("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\nHost: api.telegram.org\r\nContent-Type: multipart/form-data; boundary=" + boundary + "\r\nConnection: close\r\nContent-Length: " + String(contentLength) + "\r\n\r\n");
  client.print(body);
  client.write(payload, payloadLen);
  client.print(endBoundary);

  delay(1000);
  String response = "";
  while (client.available()) response += (char)client.read();
  return response.indexOf("\"ok\":true") > -1;
}

void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 CAM</title>
  <style>
    :root {
      --bg: #0b1220;
      --panel: #111c2d;
      --panel2: #18283d;
      --text: #edf6ff;
      --muted: #9cb2c9;
      --accent: #66d9ef;
      --accent2: #8ef6d6;
      --danger: #ff5b7f;
      --warning: #ffc857;
      --shadow: rgba(0,0,0,0.3);
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: linear-gradient(145deg, #07111d, #101b2c 60%, #172b46);
      color: var(--text);
      font-family: Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 24px;
    }

    .app {
      width: min(1200px, 100%);
      display: grid;
      grid-template-columns: 320px 1fr;
      gap: 22px;
      background: rgba(18, 27, 41, 0.8);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 24px;
      box-shadow: 0 20px 60px var(--shadow);
      padding: 18px;
      backdrop-filter: blur(6px);
    }

    .panel {
      background: linear-gradient(180deg, var(--panel), var(--panel2));
      border-radius: 18px;
      padding: 18px;
      border: 1px solid rgba(255,255,255,0.06);
    }

    .controls {
      display: flex;
      flex-direction: column;
      gap: 16px;
      max-height: 90vh;
      overflow-y: auto;
    }

    h2 {
      margin: 0 0 8px 0;
      font-size: 1.1rem;
      letter-spacing: 0.03em;
    }

    .setting {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }

    .setting label {
      color: var(--muted);
      font-size: 0.9rem;
      display: flex;
      justify-content: space-between;
      gap: 16px;
    }

    .setting span.value {
      color: var(--accent2);
      font-weight: 700;
    }

    input[type="range"] { width: 100%; accent-color: var(--accent); }
    select {
      width: 100%;
      background: #1d2d42;
      color: var(--text);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 10px;
      padding: 10px 12px;
      font-size: 0.95rem;
    }

    .buttons {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    button {
      border: none;
      padding: 12px 14px;
      border-radius: 12px;
      font-weight: 700;
      cursor: pointer;
      color: white;
      transition: transform 0.15s ease;
      box-shadow: 0 10px 20px rgba(0,0,0,0.15);
      font-size: 0.85rem;
    }

    button:hover { transform: translateY(-1px); }
    button:active { transform: translateY(1px); }
    button:disabled { opacity: 0.5; cursor: not-allowed; }

    .primary { background: linear-gradient(135deg, var(--accent), #3ea5ff); }
    .warning { background: linear-gradient(135deg, var(--warning), #ff9d5c); color: #1d1d1d; }
    .danger { background: linear-gradient(135deg, var(--danger), #ff7b6d); }
    .success { background: linear-gradient(135deg, var(--accent2), #5bb89d); color: #0f1a18; }

    .viewport {
      display: flex;
      flex-direction: column;
      gap: 14px;
      min-width: 0;
    }

    .stream-wrap {
      position: relative;
      background: #030c16;
      border-radius: 20px;
      padding: 14px;
      border: 1px solid rgba(255,255,255,0.08);
      box-shadow: inset 0 0 20px rgba(255,255,255,0.03);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 520px;
      overflow: hidden;
    }

    #liveFeed {
      width: 100%;
      height: 100%;
      max-height: 76vh;
      object-fit: contain;
      background: #000;
      border-radius: 16px;
      border: 1px solid rgba(255,255,255,0.08);
      display: block;
    }

    .status {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
      color: var(--muted);
      font-size: 0.8rem;
    }

    .pill {
      background: rgba(255,255,255,0.04);
      border: 1px solid rgba(255,255,255,0.06);
      border-radius: 999px;
      padding: 6px 12px;
      font-size: 0.8rem;
    }

    .info-panel {
      background: rgba(255,255,255,0.02);
      border: 1px solid rgba(255,255,255,0.04);
      border-radius: 12px;
      padding: 12px;
      font-size: 0.75rem;
      color: var(--muted);
      margin-top: 8px;
    }

    .info-row {
      display: flex;
      justify-content: space-between;
      padding: 4px 0;
      border-bottom: 1px solid rgba(255,255,255,0.02);
    }

    .info-row:last-child { border-bottom: none; }

    @media (max-width: 860px) {
      .app { grid-template-columns: 1fr; }
      .stream-wrap { min-height: 350px; }
    }
  </style>
</head>
<body>
  <div class="app">
    <aside class="panel controls">
      <h2>Kamera Ayarları</h2>

      <div class="setting">
        <label>Parlaklık <span class="value" id="brightnessValue">0</span></label>
        <input type="range" id="brightness" min="-2" max="2" value="0">
      </div>

      <div class="setting">
        <label>Kontrast <span class="value" id="contrastValue">0</span></label>
        <input type="range" id="contrast" min="-2" max="2" value="0">
      </div>

      <div class="setting">
        <label>Doygunluk <span class="value" id="saturationValue">0</span></label>
        <input type="range" id="saturation" min="-2" max="2" value="0">
      </div>

      <div class="setting">
        <label>Keskinlik <span class="value" id="sharpnessValue">0</span></label>
        <input type="range" id="sharpness" min="-2" max="2" value="0">
      </div>

      <div class="setting">
        <label>Kalite <span class="value" id="qualityValue">20</span></label>
        <input type="range" id="quality" min="10" max="63" value="20">
      </div>

      <div class="setting">
        <label>Piksel Boyutu</label>
        <select id="framesize">
          <option value="12" selected>640x480</option>
          <option value="9">320x240</option>
          <option value="7">160x120</option>
          <option value="13">800x600</option>
          <option value="15">1280x720</option>
        </select>
      </div>

      <div class="buttons">
        <button class="success" id="flashBtn" onclick="toggleFlash()">Flaş</button>
        <button class="primary" onclick="capturePhoto(false)">Fotoğraf</button>
        <button class="warning" onclick="capturePhoto(true)">Flaşlı</button>
        <button class="danger" onclick="location.reload()">Yenile</button>
      </div>

      <div class="info-panel">
        <div class="info-row">
          <span>Uptime:</span>
          <span id="uptime">--</span>
        </div>
        <div class="info-row">
          <span>FPS:</span>
          <span id="fps">--</span>
        </div>
        <div class="info-row">
          <span>RAM:</span>
          <span id="ram">--</span>
        </div>
        <div class="info-row">
          <span>WiFi:</span>
          <span id="wifi">--</span>
        </div>
      </div>
    </aside>

    <main class="panel viewport">
      <div class="status">
        <span class="pill">ESP32 CAM</span>
        <span class="pill" id="statusText">Canlı Yayın</span>
      </div>
      <div class="stream-wrap">
        <img id="liveFeed" src="/stream.mjpg" alt="Live camera stream">
      </div>
    </main>
  </div>

  <script>
    let settingsTimer = null;
    
    function updateValue(id, value) {
      const el = document.getElementById(id + 'Value');
      if (el) el.textContent = value;
    }

    function applySettings() {
      clearTimeout(settingsTimer);
      settingsTimer = setTimeout(() => {
        const params = [];
        ['brightness','contrast','saturation','sharpness','quality','framesize'].forEach((key) => {
          const el = document.getElementById(key);
          if (el) {
            updateValue(key, el.value);
            params.push(`${key}=${el.value}`);
          }
        });
        fetch('/settings?' + params.join('&')).catch(err => {});
      }, 300);
    }

    ['brightness','contrast','saturation','sharpness','quality','framesize'].forEach((id) => {
      const el = document.getElementById(id);
      if (el) {
        el.addEventListener('input', applySettings);
        el.addEventListener('change', applySettings);
      }
    });

    function toggleFlash() {
      const btn = document.getElementById('flashBtn');
      btn.disabled = true;
      fetch('/flash?state=' + (document.getElementById('statusText').textContent.includes('Flaş') ? 0 : 1))
        .then(() => {
          const s = document.getElementById('statusText');
          s.textContent = s.textContent.includes('Flaş') ? 'Canlı Yayın' : 'Flaş Açık';
          btn.disabled = false;
        }).catch(() => { btn.disabled = false; });
    }

    function capturePhoto(flash) {
      const url = flash ? '/capture?flash=1' : '/capture';
      document.getElementById('statusText').textContent = 'Çekiliyor...';
      fetch(url).then(() => {
        document.getElementById('statusText').textContent = 'Gönderildi ✓';
        setTimeout(() => { document.getElementById('statusText').textContent = 'Canlı Yayın'; }, 2000);
      }).catch(() => {
        document.getElementById('statusText').textContent = 'Hata!';
        setTimeout(() => { document.getElementById('statusText').textContent = 'Canlı Yayın'; }, 2000);
      });
    }

    setInterval(() => {
      fetch('/info').then(r => r.json()).then(d => {
        document.getElementById('uptime').textContent = d.uptime || '--';
        document.getElementById('fps').textContent = d.fps || '--';
        document.getElementById('ram').textContent = d.ram || '--';
        document.getElementById('wifi').textContent = d.wifi + ' dBm' || '--';
      }).catch(() => {});
    }, 1000);

    applySettings();
  </script>
</body>
</html>
)HTML";

  server.send(200, "text/html", html);
}

void handleStream() {
  server.sendHeader("Content-Type", "multipart/x-mixed-replace;boundary=frame");
  server.send(200);
  
  WiFiClient client = server.client();
  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      delay(5);
      continue;
    }

    String head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fb->len) + "\r\n\r\n";
    client.write((uint8_t*)head.c_str(), head.length());
    client.write(fb->buf, fb->len);
    client.write((uint8_t*)"\r\n", 2);
    
    esp_camera_fb_return(fb);
    frameCounter++;
    lastFrameTime = millis();

    if (!client.connected()) break;
  }
}

void handleSettings() {
  if (server.hasArg("brightness")) cameraSettings.brightness = server.arg("brightness").toInt();
  if (server.hasArg("contrast")) cameraSettings.contrast = server.arg("contrast").toInt();
  if (server.hasArg("saturation")) cameraSettings.saturation = server.arg("saturation").toInt();
  if (server.hasArg("sharpness")) cameraSettings.sharpness = server.arg("sharpness").toInt();
  if (server.hasArg("quality")) cameraSettings.quality = server.arg("quality").toInt();
  if (server.hasArg("framesize")) cameraSettings.framesize = server.arg("framesize").toInt();
  applyCameraSettings();
  server.send(200, "text/plain", "OK");
}

void handleFlash() {
  if (server.hasArg("state")) {
    bool on = server.arg("state") == "1";
    setFlash(on);
  }
  server.send(200, "text/plain", flashOn ? "ON" : "OFF");
}

void handleCapture() {
  bool flashRequested = server.hasArg("flash") && server.arg("flash") == "1";
  if (flashRequested) {
    setFlash(true);
    delay(120);
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    if (flashRequested) setFlash(false);
    server.send(503, "text/plain", "Capture failed");
    return;
  }

  bool sent = sendPhotoToTelegram(fb->buf, fb->len);
  if (flashRequested) setFlash(false);

  server.send(200, "text/plain", sent ? "SENT" : "FAILED");
  esp_camera_fb_return(fb);
}

void handleInfo() {
  unsigned long now = millis();
  unsigned long uptime = now / 1000;
  int hours = uptime / 3600;
  int mins = (uptime % 3600) / 60;
  int secs = uptime % 60;
  
  float fps = frameCounter > 0 ? (frameCounter * 1000.0f) / now : 0;
  uint32_t freeMem = esp_get_free_heap_size();
  uint32_t totalMem = esp_get_heap_size();
  int memPercent = (freeMem * 100) / totalMem;
  int rssi = WiFi.RSSI();

  String json = "{\"uptime\":\"" + String(hours) + "h " + String(mins) + "m\",";
  json += "\"fps\":" + String((int)fps) + ",";
  json += "\"ram\":\"" + String(memPercent) + "%\",";
  json += "\"wifi\":" + String(rssi) + "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 20;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_camera_init(&config);
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_vflip(sensor, 1);
    sensor->set_hmirror(sensor, 1);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream.mjpg", HTTP_GET, handleStream);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/info", HTTP_GET, handleInfo);

  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}
