#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold18pt7b.h>   // Clean sans-serif
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <string>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h> 
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h> 
#include <TimeLib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Display definition
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));
// the size of the E Ink display is 250 × 122 pixels.

// Touch pin
const int touchPin = 12;
const int threshold = 40;
volatile bool touchDetected = false;

void IRAM_ATTR touchCallback() {
  touchDetected = true;
}

// State
enum State { IDLE, ACTIVE };
State currentState = IDLE;

// Web server
WebServer server(80);

// Variables
String urllink = "";
String currentText = "";
int currentFontSize = 1; // 1=small, 2=medium, 3=large
std::vector<String> textArray;
unsigned long intervalMs = 0;
unsigned long lastUpdateTime = 0;
int currentIndex = 0;
unsigned long timeBase = 0;
bool isTimeSet = false;
bool wifiConnecting = false;
int displayMode = 0; // 0: text, 1: time, 2: date, 3: time and date, 4: weather
String city = "";
String weatherInfo = "";
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastWeatherUpdateTime = 0;
int lastDay = -1;

// Wi-Fi credentials
String apSSID = "ESP32";
String staSSID = "";
String staPassword = "";

// HTML page with CSS, adding display mode and zip code or city sections 
const char* htmlPage = R"rawliteral(
<html>
<head>
<style>
body {
  font-family: Arial, sans-serif;
  margin: 10px;
  padding: 10px;
}
h1, h2 {
  color: #333;
}
div {
  margin-bottom: 20px;
}
input, select, textarea {
  width: 100%;
  padding: 8px;
  margin-top: 5px;
  margin-bottom: 10px;
  box-sizing: border-box;
}
button {
  padding: 10px 15px;
  background-color: #4CAF50;
  color: white;
  border: none;
  cursor: pointer;
}
button:hover {
  background-color: #45a049;
}
@media (max-width: 600px) {
  body {
    margin: 5px;
    padding: 5px;
  }
}
</style>
</head>
<body>
<h1>ESP32 Control</h1>

<div>
  <h2>Send Text</h2>
  <input type="text" id="textInput">
  <button onclick="sendText()">Send</button>
</div>

<div>
  <h2>Scheduled Texts</h2>
  <textarea id="texts" rows="5" cols="30"></textarea><br>
  Interval: <input type="number" id="interval" min="1">
  <select id="unit">
    <option value="minutes">Minutes</option>
    <option value="hours">Hours</option>
    <option value="days">Days</option>
    <option value="weeks">Weeks</option>
  </select><br>
  <button onclick="setSchedule()">Set Schedule</button>
</div>

<div>
  <h2>Set Time</h2>
  <input type="datetime-local" id="timeInput">
  <button onclick="setTime()">Set Time</button>
</div>

<div>
  <h2>Set Font Size</h2>
  <select id="fontSize">
    <option value="1">Small</option>
    <option value="2">Medium</option>
    <option value="3">Large</option>
    <option value="4">Date Or Time</option> // iadded this, it worked
    <option value="5">Date And Time</option>
  </select>
  <button onclick="setFontSize()">Set Size</button>
</div>

<div>
  <h2>Connect to Wi-Fi</h2>
  SSID: <input type="text" id="ssid"><br>
  Password: <input type="password" id="password"><br>
  <button onclick="setWiFi()">Connect</button>
</div>

<div>
  <h2>Set Display Mode</h2>
  <select id="displayMode">
    <option value="0">Text</option>
    <option value="1">Time</option>
    <option value="2">Date</option>
    <option value="3">Time and Date</option>
    <option value="4">Weather</option>
  </select>
  <button onclick="setDisplayMode()">Set Mode</button>
</div>

<div>
  <h2>Set City for Weather</h2>
  <input type="text" id="city" placeholder="e.g., Aventura">
  <button onclick="setCity()">Set City</button>
</div>

<script>
function sendText() {
  const text = document.getElementById('textInput').value;
  fetch('/setText', { method: 'POST', body: 'text=' + encodeURIComponent(text), headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setSchedule() {
  const texts = document.getElementById('texts').value;
  const interval = document.getElementById('interval').value;
  const unit = document.getElementById('unit').value;
  fetch('/setSchedule', { method: 'POST', body: 'texts=' + encodeURIComponent(texts) + '&interval=' + interval + '&unit=' + unit, headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setTime() {
  const time = document.getElementById('timeInput').value;
  fetch('/setTime', { method: 'POST', body: 'time=' + encodeURIComponent(time), headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setFontSize() {
  const size = document.getElementById('fontSize').value;
  fetch('/setFontSize', { method: 'POST', body: 'size=' + size, headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setWiFi() {
  const ssid = document.getElementById('ssid').value;
  const password = document.getElementById('password').value;
  fetch('/setWiFi', { method: 'POST', body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password), headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setDisplayMode() {
  const mode = document.getElementById('displayMode').value;
  fetch('/setDisplayMode', { method: 'POST', body: 'mode=' + mode, headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}

function setCity() {
  const city = document.getElementById('city').value;
  fetch('/setCity', { method: 'POST', body: 'city=' + encodeURIComponent(city), headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
    .then(response => response.text())
    .then(data => console.log(data));
}
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(1); // Adjust as needed
  touchAttachInterrupt(touchPin, touchCallback, threshold);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  lastDisplayUpdateTime = millis();
  lastWeatherUpdateTime = millis();
}

void loop() {
  server.handleClient();
  static unsigned long lastCheckTime = 0;
  if (millis() - lastCheckTime > 5000) {
    lastCheckTime = millis();
    Serial.println("WiFi status: " + String(WiFi.status()));
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString());
    }
  }
  if (touchDetected) {
    touchDetected = false;
    if (currentState == IDLE) {
      currentState = ACTIVE;
      startWiFi();
      digitalWrite(2, HIGH);
      displayLinkAndQR();
    }
  }

  if (currentState == ACTIVE) {
    if (wifiConnecting) {
  static unsigned long wifiStartTime = 0;
  if (wifiStartTime == 0) wifiStartTime = millis();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnecting = false;
    urllink = "http://" + WiFi.localIP().toString();
    displayLinkAndQR();
    wifiStartTime = 0;
  } else if (millis() - wifiStartTime > 10000) { // 10-second timeout
    wifiConnecting = false;
    WiFi.disconnect();
    updateDisplay("Wi-Fi failed");
    wifiStartTime = 0;
  }
}

    if (displayMode == 0 && textArray.size() > 0 && intervalMs > 0 && isTimeSet) {
      unsigned long currentTime = (millis() - timeBase) / 1000;
      if ((currentTime - lastUpdateTime) * 1000 >= intervalMs) {
        currentIndex = (currentIndex + 1) % textArray.size();
        updateDisplay(textArray[currentIndex]);
        lastUpdateTime = currentTime;
      }
    }

    if (displayMode == 2 && isTimeSet && day() != lastDay) {
      updateDisplayBasedOnMode();
      lastDay = day();
    } else if ((displayMode == 1 || displayMode == 3) && millis() - lastDisplayUpdateTime >= 60000) {
      updateDisplayBasedOnMode();
      lastDisplayUpdateTime = millis();
    }

    if (displayMode == 4 && millis() - lastWeatherUpdateTime >= 3600000) {
      fetchWeatherData();
      updateDisplay(weatherInfo);
      lastWeatherUpdateTime = millis();
    }
  }
}

void startWiFi() {
  WiFi.mode(WIFI_AP); // Set to AP mode explicitly
  WiFi.softAP(apSSID.c_str());
  IPAddress IP = WiFi.softAPIP();
  urllink = "http://" + IP.toString();

  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", htmlPage);
  });

  server.on("/setText", HTTP_POST, []() {
    if (server.hasArg("text")) {
      currentText = server.arg("text");
      updateDisplay(currentText);
      server.send(200, "text/plain", "Text updated");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setSchedule", HTTP_POST, []() {
    if (server.hasArg("texts") && server.hasArg("interval") && server.hasArg("unit")) {
      String texts = server.arg("texts");
      int intervalValue = server.arg("interval").toInt();
      String unit = server.arg("unit");
      if (intervalValue > 0 && (unit == "minutes" || unit == "hours" || unit == "days" || unit == "weeks")) {
        textArray.clear();
        int start = 0;
        int end = texts.indexOf('\n');
        while (end != -1) {
          textArray.push_back(texts.substring(start, end));
          start = end + 1;
          end = texts.indexOf('\n', start);
        }
        textArray.push_back(texts.substring(start));
        if (unit == "minutes") {
          intervalMs = intervalValue * 60 * 1000;
        } else if (unit == "hours") {
          intervalMs = intervalValue * 60 * 60 * 1000;
        } else if (unit == "days") {
          intervalMs = intervalValue * 24 * 60 * 60 * 1000;
        } else if (unit == "weeks") {
          intervalMs = intervalValue * 7 * 24 * 60 * 60 * 1000;
        }
        lastUpdateTime = (millis() - timeBase) / 1000;
        currentIndex = 0;
        if (textArray.size() > 0) updateDisplay(textArray[0]);
        server.send(200, "text/plain", "Schedule set");
      } else {
        server.send(400, "text/plain", "Invalid interval or unit");
      }
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setTime", HTTP_POST, []() {
    if (server.hasArg("time")) {
      String timeStr = server.arg("time");
      int year = timeStr.substring(0,4).toInt();
      int month = timeStr.substring(5,7).toInt();
      int day = timeStr.substring(8,10).toInt();
      int hour = timeStr.substring(11,13).toInt();
      int minute = timeStr.substring(14,16).toInt();
      setTime(hour, minute, 0, day, month, year);
      isTimeSet = true;
      server.send(200, "text/plain", "Time set");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setFontSize", HTTP_POST, []() {
    if (server.hasArg("size")) {
      currentFontSize = server.arg("size").toInt();
      if (currentText != "") updateDisplay(currentText);
      server.send(200, "text/plain", "Font size updated");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setWiFi", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      staSSID = server.arg("ssid");
      staPassword = server.arg("password");
      WiFi.softAPdisconnect(true); // Stop AP mode
      WiFi.mode(WIFI_STA); // Switch to STA mode
      WiFi.begin(staSSID.c_str(), staPassword.c_str());
      wifiConnecting = true;
      server.send(200, "text/plain", "Attempting to connect");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setDisplayMode", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      displayMode = server.arg("mode").toInt();
      if (displayMode == 4 && city != "" && WiFi.status() == WL_CONNECTED) {
        fetchWeatherData();
      }
      updateDisplayBasedOnMode();
      server.send(200, "text/plain", "Display mode set");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setCity", HTTP_POST, []() {
    if (server.hasArg("city")) {
      city = server.arg("city");
      server.send(200, "text/plain", "City set");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
}

void drawQRBitmap(int x, int y) {
  // Placeholder QR code bitmap - replace with your actual bitmap data
  const unsigned char qrBitmap [] PROGMEM = {
  // '93ad6a5b0fe8106ee936dd6882468a12, 100x100px
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 
  0xf8, 0xfe, 0x03, 0xc7, 0xff, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 0xf8, 0xfe, 0x03, 
  0xc7, 0xff, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 0xf8, 0xfe, 0x03, 0xc7, 0xff, 0xff, 
  0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x3f, 0xf8, 0x10, 0x00, 0x07, 0xff, 0xff, 0xf8, 0x00, 0x01, 
  0xe0, 0x00, 0x1e, 0x3f, 0xf8, 0x00, 0x00, 0x07, 0x80, 0x00, 0x78, 0x00, 0x01, 0xe0, 0x00, 0x1e, 
  0x3f, 0xf8, 0x00, 0x00, 0x07, 0x80, 0x00, 0x78, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x3f, 0xf8, 0x00, 
  0x00, 0x07, 0x80, 0x00, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x3f, 0xf8, 0xf1, 0xe3, 0xc7, 0x8f, 
  0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x3f, 0xf8, 0xf1, 0xe3, 0xc7, 0x8f, 0xfc, 0x78, 0x00, 
  0x01, 0xe3, 0xff, 0x1e, 0x3f, 0xf8, 0xf1, 0xe3, 0xc7, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 
  0x1e, 0x3f, 0xc7, 0x9f, 0xff, 0xc7, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x3f, 0x87, 
  0x0f, 0xff, 0xc7, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x3f, 0x87, 0x0f, 0xff, 0xc7, 
  0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x3f, 0xc7, 0x0f, 0xff, 0xc7, 0x8f, 0xfc, 0x78, 
  0x00, 0x01, 0xe3, 0xff, 0x1e, 0x00, 0x78, 0x0e, 0x00, 0x07, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 
  0xff, 0x1e, 0x00, 0x78, 0x0e, 0x00, 0x07, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x00, 
  0x78, 0x0e, 0x00, 0x07, 0x8f, 0xfc, 0x78, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x03, 0xf8, 0xff, 0xe0, 
  0x07, 0x80, 0x00, 0x78, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x03, 0xf8, 0xff, 0xe0, 0x07, 0x80, 0x00, 
  0x78, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x03, 0xf8, 0xff, 0xe0, 0x07, 0x80, 0x00, 0x78, 0x00, 0x01, 
  0xff, 0xff, 0xfe, 0x03, 0xf8, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 
  0x3c, 0x78, 0xf1, 0xe3, 0xc7, 0xff, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x3c, 0x78, 0xf1, 
  0xe3, 0xc7, 0xff, 0xff, 0xf8, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x3c, 0x78, 0xf1, 0xe3, 0xc7, 0xff, 
  0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x07, 0x01, 0xe3, 0xc0, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x3c, 0x07, 0x01, 0xe3, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x3c, 0x07, 0x01, 0xe3, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x0e, 0x3c, 0x78, 
  0x01, 0x3f, 0xff, 0x01, 0xff, 0xc0, 0x00, 0x01, 0xc0, 0x00, 0x0e, 0x3c, 0x78, 0x00, 0x1f, 0xff, 
  0x01, 0xff, 0xc0, 0x00, 0x01, 0xc0, 0x00, 0x0e, 0x3c, 0x78, 0x00, 0x1f, 0xff, 0x01, 0xff, 0xc0, 
  0x00, 0x01, 0xc0, 0x00, 0x0e, 0x3c, 0x78, 0x00, 0x1f, 0xff, 0x01, 0xff, 0xc0, 0x00, 0x01, 0xc0, 
  0x00, 0x00, 0x3f, 0x87, 0x0f, 0xff, 0xf8, 0x0f, 0xff, 0xc0, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x3f, 
  0x87, 0x0f, 0xff, 0xf8, 0x0f, 0xff, 0xc0, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x3f, 0x87, 0x0f, 0xff, 
  0xf8, 0x0f, 0xff, 0xc0, 0x00, 0x00, 0x3c, 0x78, 0xff, 0xff, 0xc0, 0x0f, 0xfc, 0x47, 0xf1, 0xe3, 
  0xf8, 0x00, 0x00, 0x3c, 0x78, 0xff, 0xff, 0x80, 0x0f, 0xfc, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x00, 
  0x3c, 0x78, 0xff, 0xff, 0x80, 0x0f, 0xfc, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x00, 0x3c, 0x78, 0xff, 
  0xff, 0xc0, 0x0f, 0xfc, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xfc, 0x00, 0x01, 0xc0, 0x78, 0xfe, 
  0x1f, 0xc0, 0x0f, 0xe0, 0x38, 0x00, 0x01, 0xfc, 0x00, 0x01, 0xc0, 0x78, 0xfe, 0x1f, 0xc0, 0x0f, 
  0xe0, 0x38, 0x00, 0x01, 0xfc, 0x00, 0x01, 0xc0, 0x78, 0xfe, 0x1f, 0xc0, 0x0f, 0xe0, 0x38, 0x00, 
  0x00, 0x03, 0xff, 0x0f, 0xe3, 0xc0, 0xfe, 0x03, 0xff, 0x00, 0x00, 0x38, 0x00, 0x00, 0x03, 0xff, 
  0x0f, 0xc3, 0x80, 0xfe, 0x03, 0xff, 0x00, 0x00, 0x38, 0x00, 0x00, 0x03, 0xff, 0x0f, 0xc3, 0x80, 
  0xfe, 0x03, 0xff, 0x00, 0x00, 0x38, 0x00, 0x00, 0x07, 0xff, 0x1f, 0xe3, 0xc0, 0xfe, 0x03, 0xff, 
  0x00, 0x00, 0x78, 0x00, 0x01, 0xff, 0x87, 0xf0, 0x3c, 0x78, 0x00, 0x1c, 0x00, 0x00, 0x03, 0xc0, 
  0x00, 0x01, 0xff, 0x87, 0xf0, 0x3c, 0x78, 0x00, 0x1c, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x01, 0xff, 
  0x87, 0xf0, 0x3c, 0x78, 0x00, 0x1c, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x01, 0xe3, 0xf8, 0xff, 0xff, 
  0xff, 0xf0, 0x1c, 0x78, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xc3, 0xf8, 0xff, 0xff, 0xff, 0xf0, 0x1c, 
  0x38, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xc3, 0xf8, 0xff, 0xff, 0xff, 0xf0, 0x1c, 0x38, 0xf1, 0xe3, 
  0xf8, 0x00, 0x01, 0xe3, 0xf8, 0xff, 0xff, 0xff, 0xf0, 0x1c, 0x78, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 
  0xc3, 0xff, 0x00, 0x3c, 0x07, 0x00, 0x03, 0xc0, 0x0e, 0x1c, 0x38, 0x00, 0x01, 0xc3, 0xff, 0x00, 
  0x3c, 0x07, 0x00, 0x03, 0xc0, 0x0e, 0x1c, 0x38, 0x00, 0x01, 0xc3, 0xff, 0x00, 0x3c, 0x07, 0x00, 
  0x03, 0xc0, 0x0e, 0x1c, 0x38, 0x00, 0x01, 0xc3, 0xff, 0xff, 0xff, 0x87, 0x00, 0x03, 0xff, 0xfe, 
  0x1c, 0x00, 0x00, 0x01, 0xc3, 0xff, 0xff, 0xff, 0x87, 0x00, 0x03, 0xff, 0xfe, 0x1c, 0x00, 0x00, 
  0x01, 0xc3, 0xff, 0xff, 0xff, 0x87, 0x00, 0x03, 0xff, 0xfe, 0x1c, 0x00, 0x00, 0x01, 0xc3, 0xff, 
  0xff, 0xff, 0xc7, 0x91, 0x23, 0xff, 0xff, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x80, 
  0xff, 0xff, 0xc0, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x80, 0xff, 0xff, 0xc0, 
  0x0e, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x80, 0xff, 0xff, 0xc0, 0x0e, 0x1c, 0x00, 
  0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 0x87, 0x00, 0x03, 0xc7, 0x0f, 0xe0, 0x38, 0x00, 0x01, 0xff, 
  0xff, 0xfe, 0x03, 0x87, 0x00, 0x03, 0xc7, 0x0f, 0xe0, 0x38, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 
  0x87, 0x00, 0x03, 0xc7, 0x0f, 0xe0, 0x38, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x03, 0xc0, 0xfe, 0x03, 
  0xc0, 0x0f, 0x03, 0xc0, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x03, 0x80, 0xfe, 0x03, 0xc0, 0x0e, 0x03, 
  0xc0, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x03, 0x80, 0xfe, 0x03, 0xc0, 0x0e, 0x03, 0xc0, 0x00, 0x01, 
  0xe0, 0x00, 0x1e, 0x03, 0xc0, 0xfe, 0x03, 0xc0, 0x0e, 0x03, 0xc0, 0x00, 0x01, 0xe3, 0xff, 0x1e, 
  0x03, 0xf8, 0xf1, 0xff, 0xff, 0xff, 0xfc, 0x38, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x03, 0xf8, 0xf1, 
  0xff, 0xff, 0xff, 0xfc, 0x38, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x03, 0xf8, 0xf1, 0xff, 0xff, 0xff, 
  0xfc, 0x38, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x03, 0xc0, 0x01, 0xe0, 0x47, 0xf1, 0xe3, 0xf8, 0x00, 
  0x01, 0xe3, 0xff, 0x1e, 0x03, 0x80, 0x01, 0xe0, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xe3, 0xff, 
  0x1e, 0x03, 0x80, 0x01, 0xe0, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x03, 0x80, 
  0x01, 0xe0, 0x07, 0xf1, 0xe3, 0xf8, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x00, 0x07, 0xf0, 0x00, 0x38, 
  0x00, 0x1c, 0x38, 0x00, 0x01, 0xe3, 0xff, 0x1e, 0x00, 0x07, 0xf0, 0x00, 0x38, 0x00, 0x1c, 0x38, 
  0x00, 0x01, 0xe3, 0xff, 0x1e, 0x00, 0x07, 0xf0, 0x00, 0x38, 0x00, 0x1c, 0x38, 0x00, 0x01, 0xe0, 
  0x00, 0x1e, 0x00, 0x7f, 0x81, 0xff, 0xc7, 0xfe, 0x00, 0x38, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x00, 
  0x7f, 0x01, 0xff, 0xc7, 0xfe, 0x00, 0x38, 0x00, 0x01, 0xe0, 0x00, 0x1e, 0x00, 0x7f, 0x01, 0xff, 
  0xc7, 0xfe, 0x00, 0x38, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x00, 0x7f, 0x01, 0xff, 0xc7, 0xff, 0x00, 
  0x78, 0x00, 0x01, 0xff, 0xff, 0xfe, 0x3c, 0x78, 0x0f, 0xff, 0xc7, 0x01, 0xe0, 0x38, 0x00, 0x01, 
  0xff, 0xff, 0xfe, 0x3c, 0x78, 0x0f, 0xff, 0xc7, 0x01, 0xe0, 0x38, 0x00, 0x01, 0xff, 0xff, 0xfe, 
  0x3c, 0x78, 0x0f, 0xff, 0xc7, 0x01, 0xe0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00
};
  display.drawBitmap(x, y, qrBitmap, 100, 100, GxEPD_BLACK);
}

void displayLinkAndQR() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    int qrX = 150;
    int qrY = 3;
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 40);
    display.println("Connect to");
    display.println("ESP32 WIFI");
    display.println("then go to");
    display.setCursor(10, 112);
    display.print(urllink);
    drawQRBitmap(qrX, qrY);
  } while (display.nextPage());
}

void updateDisplay(String text) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    if (currentFontSize == 1) display.setFont(&FreeMonoBold9pt7b);
    else if (currentFontSize == 2) display.setFont(&FreeMonoBold12pt7b);
    else if (currentFontSize == 3) display.setFont(&FreeMonoBold18pt7b);
    else if (currentFontSize == 4) display.setFont(&FreeSansBold24pt7b); // i added this. 
    else if (currentFontSize == 5) display.setFont(&FreeSansBold12pt7b); // i added this. 
    display.setCursor(10, 20);
    if (currentFontSize == 4) display.setCursor(50, 70); // OK THIS IS PERFECT!!!!!! 
    if (currentFontSize == 5) display.setCursor(30, 70); // ok great! this acctually gave me the ability to center the regular text too if you set the text to date or time. 
    display.print(text);
  } while (display.nextPage());
}


void updateDisplayBasedOnMode() {
  String displayText;
  if (displayMode == 0) {
    if (textArray.size() > 0) {
      displayText = textArray[currentIndex];
    } else {
      displayText = currentText;
    }
  } else if (displayMode == 1) {
    if (isTimeSet) {
        int h = hour();
        String period = (h >= 12) ? "PM" : "AM";
        if (h > 12) h -= 12;
        else if (h == 0) h = 12;
        displayText = String(h) + ":" + (minute() < 10 ? "0" : "") + String(minute()) + " " + period;
    } else {
        displayText = "Set time first";
    }
} else if (displayMode == 2) {
    if (isTimeSet) {
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        displayText = String(day()) + " " + months[month() - 1];
    } else {
        displayText = "Set time first";
    }
} else if (displayMode == 3) {
    if (isTimeSet) {
        int h = hour();
        String period = (h >= 12) ? "PM" : "AM";
        if (h > 12) h -= 12;
        else if (h == 0) h = 12;
        String timeStr = String(h) + ":" + (minute() < 10 ? "0" : "") + String(minute()) + " " + period;
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        String dateStr = String(day()) + " " + months[month() - 1];
        displayText = timeStr + " " + dateStr;
    } else {
        displayText = "Set time first";
    }
} else if (displayMode == 4) {
    if (city == "") {
      displayText = "Set city";
    } else if (WiFi.status() != WL_CONNECTED) {
      displayText = "Connect to Wi-Fi";
    } else {
      displayText = weatherInfo;
    }
  }
  updateDisplay(displayText);
}

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    weatherInfo = "Wi-Fi not connected";
    return;
  }
  HTTPClient testHttp;
  testHttp.begin("http://www.google.com");
  int testCode = testHttp.GET();
  if (testCode != 200) {
    weatherInfo = "No internet access";
    testHttp.end();
    return;
  }
  testHttp.end();

  HTTPClient http;
  String geoUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + city + "&count=1&format=json";
  http.begin(geoUrl);
  int httpCode = http.GET();
  if (httpCode != 200) {
    weatherInfo = "Geocoding failed";
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);
  if (!doc["results"][0]) {
    weatherInfo = "Invalid city";
    return;
  }
  float lat = doc["results"][0]["latitude"];
  float lon = doc["results"][0]["longitude"];

  String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 6) + "&longitude=" + String(lon, 6) + "&current_weather=true&temperature_unit=fahrenheit";
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode != 200) {
    weatherInfo = "Weather fetch failed";
    http.end();
    return;
  }
  payload = http.getString();
  http.end();

  deserializeJson(doc, payload);
  if (!doc["current_weather"]) {
    weatherInfo = "No weather data";
    return;
  }
  float temp = doc["current_weather"]["temperature"];
  int weatherCode = doc["current_weather"]["weathercode"];
  String description;
  if (weatherCode == 0) description = "Clear";
  else if (weatherCode >= 1 && weatherCode <= 3) description = "Cloudy";
  else if (weatherCode == 45 || weatherCode == 48) description = "Fog";
  else if (weatherCode >= 51 && weatherCode <= 55) description = "Drizzle";
  else if (weatherCode >= 61 && weatherCode <= 65) description = "Rain";
  else if (weatherCode >= 71 && weatherCode <= 75) description = "Snow";
  else if (weatherCode >= 80 && weatherCode <= 82) description = "Showers";
  else if (weatherCode == 95) description = "Thunderstorm";
  else description = "Unknown";
  
  int tempInt = round(temp);
  weatherInfo = String(tempInt) + " F " + description;
}
