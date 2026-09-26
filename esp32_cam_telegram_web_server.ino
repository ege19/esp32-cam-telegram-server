#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

#define FLASH_PIN 4

// ---------------------------
// User config
// ---------------------------
const char* WIFI_SSID = "Palabıyık";
const char* WIFI_PASSWORD = "19831983";
const char* BOT_TOKEN = "8827031277:AAHx9HntnJI8UJXuex-cm1T760fJTuFtDo4";
const char* CHAT_ID = "6798340496";

// ---------------------------
// Camera pins for AI-Thinker ESP32-CAM
// ---------------------------
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

struct CameraSettings {
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  int sharpness = 0;
  int quality = 10;
  int framesize = FRAMESIZE_VGA;
  bool hmirror = false;
  bool vflip = false;
};

CameraSettings cameraSettings;

String frameSizeName(int index) {
  switch (index) {
    case FRAMESIZE_96X96: return "96x96";
    case FRAMESIZE_QQVGA: return "160x120";
    case FRAMESIZE_QCIF: return "176x144";
    case FRAMESIZE_HQVGA: return "240x176";
    case FRAMESIZE_240X240: return "240x240";
    case FRAMESIZE_QVGA: return "320x240";
    case FRAMESIZE_CIF: return "400x296";
    case FRAMESIZE_HVGA: return "480x320";
    case FRAMESIZE_VGA: return "640x480";
    case FRAMESIZE_SVGA: return "800x600";
    case FRAMESIZE_XGA: return "1024x768";
    case FRAMESIZE_HD: return "1280x720";
    case FRAMESIZE_SXGA: return "1280x1024";
    case FRAMESIZE_UXGA: return "1600x1200";
    default: return "640x480";
  }
}

void applyCameraSettings() {
  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) return;

  sensor->set_brightness(sensor, cameraSettings.brightness);
  sensor->set_contrast(sensor, cameraSettings.contrast);
  sensor->set_saturation(sensor, cameraSettings.saturation);
  sensor->set_sharpness(sensor, cameraSettings.sharpness);
  sensor->set_quality(sensor, cameraSettings.quality);
  sensor->set_framesize(sensor, (framesize_t)cameraSettings.framesize);
  sensor->set_hmirror(sensor, cameraSettings.hmirror);
  sensor->set_vflip(sensor, cameraSettings.vflip);
}

void setFlash(bool enable) {
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, enable ? HIGH : LOW);
}

bool sendPhotoToTelegram(const uint8_t* payload, size_t payloadLen, const char* fileName) {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram connect failed");
    return false;
  }

  String boundary = "----ESP32CAMBoundary";
  String body = "";
  body += "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  body += String(CHAT_ID);
  body += "\r\n--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"photo\"; filename=\"" + String(fileName) + "\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";

  String endBoundary = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = body.length() + payloadLen + endBoundary.length();

  client.print("POST /bot");
  client.print(BOT_TOKEN);
  client.println("/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(String(contentLength));
  client.println();

  client.print(body);
  client.write(payload, payloadLen);
  client.print(endBoundary);

  delay(1000);

  String response = "";
  while (client.connected() || client.available()) {
    char c = client.read();
    if (c != -1) response += c;
    else break;
  }

  Serial.println("Telegram response:");
  Serial.println(response.substring(0, 300));

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

    input[type="range"] {
      width: 100%;
      accent-color: var(--accent);
    }

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
      grid-template-columns: repeat(2, minmax(120px, 1fr));
      gap: 10px;
    }

    button {
      border: none;
      padding: 12px 14px;
      border-radius: 12px;
      font-weight: 700;
      cursor: pointer;
      color: white;
      transition: transform 0.15s ease, opacity 0.15s ease;
      box-shadow: 0 10px 20px rgba(0,0,0,0.15);
    }

    button:hover { transform: translateY(-1px); }
    button:active { transform: translateY(1px); }

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
    }

    #liveFeed {
      width: 100%;
      max-height: 76vh;
      object-fit: contain;
      background: #000;
      border-radius: 16px;
      border: 1px solid rgba(255,255,255,0.08);
    }

    .status {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
      color: var(--muted);
      font-size: 0.85rem;
    }

    .pill {
      background: rgba(255,255,255,0.04);
      border: 1px solid rgba(255,255,255,0.06);
      border-radius: 999px;
      padding: 8px 12px;
    }

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
        <label>Kalite <span class="value" id="qualityValue">10</span></label>
        <input type="range" id="quality" min="10" max="63" value="10">
      </div>

      <div class="setting">
        <label>Piksel Boyutu</label>
        <select id="framesize">
          <option value="7">160x120</option>
          <option value="8">240x240</option>
          <option value="9">320x240</option>
          <option value="10">400x296</option>
          <option value="11">480x320</option>
          <option value="12">640x480</option>
          <option value="13">800x600</option>
          <option value="14">1024x768</option>
          <option value="15">1280x720</option>
          <option value="16">1280x1024</option>
          <option value="17">1600x1200</option>
        </select>
      </div>

      <div class="buttons">
        <button class="success" onclick="toggleFlash()">Flaş</button>
        <button class="primary" onclick="capturePhoto(false)">Fotoğraf Çek</button>
        <button class="warning" onclick="capturePhoto(true)">Flaşlı Foto</button>
        <button class="danger" onclick="refreshFeed()">Yenile</button>
      </div>
    </aside>

    <main class="panel viewport">
      <div class="status">
        <span class="pill">ESP32 CAM</span>
        <span class="pill" id="statusText">Canlı Yayın</span>
      </div>
      <div class="stream-wrap">
        <img id="liveFeed" src="/cam.jpg?ts=0" alt="Live camera stream">
      </div>
    </main>
  </div>

  <script>
    function updateValue(id, value) {
      const el = document.getElementById(id + 'Value');
      if (el) el.textContent = value;
    }

    function applySettings() {
      const params = [];
      ['brightness','contrast','saturation','sharpness','quality','framesize'].forEach((key) => {
        const el = document.getElementById(key);
        if (el) {
          updateValue(key, el.value);
          params.push(`${key}=${encodeURIComponent(el.value)}`);
        }
      });

      fetch('/settings?' + params.join('&'))
        .then(res => res.text())
        .catch(err => console.error(err));
    }

    [
      'brightness','contrast','saturation','sharpness','quality','framesize'
    ].forEach((id) => {
      const el = document.getElementById(id);
      if (el) {
        el.addEventListener('input', applySettings);
        el.addEventListener('change', applySettings);
      }
    });

    function refreshFeed() {
      const img = document.getElementById('liveFeed');
      img.src = '/cam.jpg?ts=' + Date.now();
    }

    function toggleFlash() {
      fetch('/flash?state=' + (document.getElementById('statusText').textContent.includes('Flaş açık') ? 0 : 1))
        .then(res => res.text())
        .then(() => {
          const s = document.getElementById('statusText');
          s.textContent = s.textContent.includes('Flaş açık') ? 'Canlı Yayın' : 'Flaş açık';
        });
    }

    function capturePhoto(flash) {
      const url = flash ? '/capture?flash=1' : '/capture';
      document.getElementById('statusText').textContent = 'Foto çekiliyor...';
      fetch(url)
        .then(res => res.text())
        .then(() => {
          document.getElementById('statusText').textContent = 'Fotoğraf gönderildi';
          setTimeout(refreshFeed, 300);
        })
        .catch(() => {
          document.getElementById('statusText').textContent = 'Hata';
        });
    }

    setInterval(refreshFeed, 200);
    applySettings();
  </script>
</body>
</html>
)HTML";

  server.send(200, "text/html", html);
}

void handleSettings() {
  if (server.hasArg("brightness")) cameraSettings.brightness = server.arg("brightness").toInt();
  if (server.hasArg("contrast")) cameraSettings.contrast = server.arg("contrast").toInt();
  if (server.hasArg("saturation")) cameraSettings.saturation = server.arg("saturation").toInt();
  if (server.hasArg("sharpness")) cameraSettings.sharpness = server.arg("sharpness").toInt();
  if (server.hasArg("quality")) cameraSettings.quality = server.arg("quality").toInt();
  if (server.hasArg("framesize")) {
    cameraSettings.framesize = server.arg("framesize").toInt();
  }
  applyCameraSettings();
  server.send(200, "text/plain", "OK");
}

void handleFlash() {
  if (server.hasArg("state")) {
    bool on = server.arg("state") == "1";
    setFlash(on);
    server.send(200, "text/plain", on ? "FLASH_ON" : "FLASH_OFF");
    return;
  }
  server.send(200, "text/plain", "NO_STATE");
}

void handleCamJpg() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(503, "text/plain", "Camera capture failed");
    return;
  }

  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  WiFiClient client = server.client();
  if (client.connected()) {
    client.write(fb->buf, fb->len);
  }
  esp_camera_fb_return(fb);
}

void handleCapture() {
  const bool flashRequested = server.hasArg("flash") && server.arg("flash") == "1";

  if (flashRequested) {
    setFlash(true);
    delay(120);
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    if (flashRequested) setFlash(false);
    server.send(503, "text/plain", "Camera capture failed");
    return;
  }

  bool sent = sendPhotoToTelegram(fb->buf, fb->len, "esp32cam_photo.jpg");

  if (flashRequested) {
    setFlash(false);
  }

  String result = sent ? "PHOTO_SENT" : "PHOTO_FAILED";
  server.send(200, "text/plain", result);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi did not connect");
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
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor) {
    sensor->set_vflip(sensor, 1);
    sensor->set_hmirror(sensor, 1);
    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 0);
    sensor->set_saturation(sensor, 0);
    sensor->set_sharpness(sensor, 0);
  }

  cameraSettings.framesize = FRAMESIZE_VGA;
  cameraSettings.quality = 10;
  applyCameraSettings();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/flash", HTTP_GET, handleFlash);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/cam.jpg", HTTP_GET, handleCamJpg);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
