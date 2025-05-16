
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <ezTime.h>
// Replace with your specific E-ink display header, e.g.:
// #include <GxGDEW0154Z04.h> (adjust based on your display model)

//// Define the display (adjust pins as needed for your setup)
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=5*/ 5, /*DC=17*/ 17, /*RST=16*/ 16, /*BUSY=4*/ 4));
//
// WiFi credentials
const char* ssid = "";       // Replace with your WiFi SSID
const char* password = ""; // Replace with your WiFi password

// Timezone object for Eastern Time
Timezone tz;

void setup() {
  Serial.begin(115200);

  // Initialize the E-ink display
  display.init();
  display.setRotation(1); // Adjust rotation as needed (0-3)
  display.fillScreen(GxEPD_WHITE);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set the timezone to Eastern Time
  tz.setLocation("America/New_York");

  // Fetch and display the next launch from Florida
  fetchAndDisplayNextLaunch();
}

void loop() {
  // Update every hour (3600000 ms) for production
  fetchAndDisplayNextLaunch();
  delay(3600000); // 1 hour delay; use delay(10000) for testing
}

void fetchAndDisplayNextLaunch() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://fdo.rocketlaunch.live/json/launches/next/5");
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Parse JSON
      StaticJsonDocument<2048> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        displayError("JSON Error");
        return;
      }

      bool found = false;
      for (JsonObject launch : doc["result"].as<JsonArray>()) {
        String state = launch["pad"]["location"]["state"];
        if (state == "FL") {
          found = true;
          String name = launch["name"];
          String vehicle = launch["vehicle"]["name"];
          String location = launch["pad"]["location"]["name"];
          String launchTime;
          if (!launch["t0"].isNull()) {
            launchTime = launch["t0"].as<String>();
          } else if (!launch["win_open"].isNull()) {
            launchTime = launch["win_open"].as<String>();
          } else {
            launchTime = "TBD";
          }
          String displayTime = convertToLocalTime(launchTime);

          // Display the data
          display.fillScreen(GxEPD_WHITE);
          display.setTextColor(GxEPD_BLACK);
     
          display.setCursor(0, 20);
          display.println("Next FL Launch:");
          display.print("Name: ");
          display.println(name);
          display.print("Vehicle: ");
          display.println(vehicle);
          display.print("Location: ");
          display.println(location);
          display.print("Time: ");
          display.println(displayTime);
          display.display();           // Update the display to show the image
          display.powerOff(); 
          break; // Only display the next one
        }
      }
      if (!found) {
        displayError("No FL launches");
      }
    } else {
      Serial.print("HTTP request failed with code: ");
      Serial.println(httpCode);
      displayError("HTTP Error");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
    displayError("No WiFi");
  }
}

String convertToLocalTime(String utcTimeStr) {
  if (utcTimeStr == "TBD") {
    return "TBD";
  }
  int year, month, day, hour, minute;
  if (sscanf(utcTimeStr.c_str(), "%d-%d-%dT%d:%dZ", &year, &month, &day, &hour, &minute) != 5) {
    return "Invalid Time";
  }
  ezTime myTime(UTC);
  //UTC.now();
//  String utcTimeStr = "2023-04-15T12:00Z"; // Placeholder UTC time, to be replaced by actual input
  myTime.setTime(hour, minute, 0, day, month, year);
  // Convert to 12-hour format (e.g., "03:00 PM")
  String localTimeStr = myTime.dateTime("America/New_York", "%I:%M %p");
  return localTimeStr;
}

void displayError(String message) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  
  display.setCursor(0, 20);
  display.println("Error:");
  display.println(message);
  display.display();           // Update the display to show the image
  display.powerOff(); 
}
