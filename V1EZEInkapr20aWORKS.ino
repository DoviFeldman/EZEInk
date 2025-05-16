#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <WebServer.h>
//#include <AsyncTCP.h>
//#include <ESPAsyncWebServer.h>
//#include <qrcode.h>
#include <vector>
#include <string>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

// Display definition
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));

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
//AsyncWebServer server(80);
WebServer server(80);

// QR code
// QRCode qrcode;

// Variables
String urllink = "";
String currentText = "";
int currentFontSize = 1; // 1=small, 2=medium, 3=large
std::vector<String> textArray;
unsigned long intervalMs = 0;
unsigned long lastUpdateTime = 0;
int currentIndex = 0;
unsigned long timeBase = 0;
bool timeSet = false;
bool wifiConnecting = false;

// Wi-Fi credentials
String apSSID = "ESP32";
//String apPassword = "password";
String staSSID = "";
String staPassword = "";



// HTML page
const char* htmlPage = R"rawliteral(
<html>
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
  Interval (minutes): <input type="number" id="interval" min="1"><br>
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
  </select>
  <button onclick="setFontSize()">Set Size</button>
</div>

<div>
  <h2>Connect to Wi-Fi</h2>
  SSID: <input type="text" id="ssid"><br>
  Password: <input type="password" id="password"><br>
  <button onclick="setWiFi()">Connect</button>
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
  fetch('/setSchedule', { method: 'POST', body: 'texts=' + encodeURIComponent(texts) + '&interval=' + interval, headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
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
}

void loop() {
  server.handleClient();
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
    if (wifiConnecting && WiFi.status() == WL_CONNECTED) {
      wifiConnecting = false;
      urllink = "http://" + WiFi.localIP().toString();
      displayLinkAndQR();
    }

    if (textArray.size() > 0 && intervalMs > 0 && timeSet) {
      unsigned long currentTime = (millis() - timeBase) / 1000; // Seconds since time set
      if ((currentTime - lastUpdateTime) * 1000 >= intervalMs) {
        currentIndex = (currentIndex + 1) % textArray.size();
        updateDisplay(textArray[currentIndex]);
        lastUpdateTime = currentTime;
      }
    }
  }
}

void startWiFi() {
  //WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  WiFi.softAP(apSSID.c_str());
  IPAddress IP = WiFi.softAPIP();
  urllink = "http://" + IP.toString();
//
//  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//    request->send(200, "text/html", htmlPage);
//  });

//server.on("/", HTTP_GET, [](){
//  server.send(200, "text/plain", "Hello, world!");
//});

server.on("/", HTTP_GET, [](){
  server.send(200, "text/html", htmlPage);
});

//
//  server.on("/setText", HTTP_POST, []( ) {
//    if (request->hasParam("text", true)) {
//      currentText = request->getParam("text", true)->value();
//      updateDisplay(currentText);
//      request->send(200, "text/plain", "Text updated");
//    } else {
//      request->send(400, "text/plain", "Bad Request");
//    }
//  });
//


  server.on("/setText", HTTP_POST, []( ) {
    if (server.hasArg("text")) {
      currentText =  server.arg("text");
      updateDisplay(currentText);
      server.send(200, "text/plain", "Text updated");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setSchedule", HTTP_POST, []( ) {
    if (server.hasArg("texts") && server.hasArg("interval")) {
      String texts =  server.arg("texts");
      int intervalMinutes =  server.arg("interval").toInt();
      if (intervalMinutes > 0) {
        textArray.clear();
        int start = 0;
        int end = texts.indexOf('\n');
        while (end != -1) {
          textArray.push_back(texts.substring(start, end));
          start = end + 1;
          end = texts.indexOf('\n', start);
        }
        textArray.push_back(texts.substring(start));
        intervalMs = intervalMinutes * 60 * 1000;
        lastUpdateTime = (millis() - timeBase) / 1000;
        currentIndex = 0;
        if (textArray.size() > 0) updateDisplay(textArray[0]);
        server.send(200, "text/plain", "Schedule set");
      } else {
        server.send(400, "text/plain", "Invalid interval");
      }
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setTime", HTTP_POST, []( ) {
    if (server.hasArg("time")) {
      // Simplified: using millis offset instead of parsing full datetime
      timeBase = millis();
      timeSet = true;
      server.send(200, "text/plain", "Time set");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setFontSize", HTTP_POST, []( ) {
    if (server.hasArg("size")) {
      currentFontSize =  server.arg("size").toInt();
      if (currentText != "") updateDisplay(currentText);
      server.send(200, "text/plain", "Font size updated");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setWiFi", HTTP_POST, []( ) {
    if (server.hasArg("ssid") && server.hasArg("password")) {
    //if (server.hasArg("ssid")) {
      staSSID =  server.arg("ssid");
      staPassword =  server.arg("password");
      WiFi.begin(staSSID.c_str(), staPassword.c_str());
      //WiFi.begin(staSSID.c_str());
      wifiConnecting = true;
      server.send(200, "text/plain", "Attempting to connect");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
}

void displayLinkAndQR() {
  display.setFullWindow();
  display.setCursor(30, 30);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
//
//    // Generate QR code
//    uint8_t qrcodeData[qrcode_getBufferSize(3)];
//    qrcode_initText(&qrcode, qrcodeData, 3, 0, urllink.c_str());
//    int qrSize = qrcode.size;
//    int pixelSize = 4;
//    int qrX = 10;
//    int qrY = 10;
//    for (int y = 0; y < qrSize; y++) {
//      for (int x = 0; x < qrSize; x++) {
//        if (qrcode_getModule(&qrcode, x, y)) {
//          display.fillRect(qrX + x * pixelSize, qrY + y * pixelSize, pixelSize, pixelSize, GxEPD_BLACK);
//        }
//      }
//    }

    // Display urllink
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    //display.setCursor(qrX + qrSize * pixelSize + 10, qrY + 20);
    display.print(urllink);
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
    display.setCursor(10, 20);
    display.print(text);
  } while (display.nextPage());
}
