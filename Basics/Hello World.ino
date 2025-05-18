#include <GxEPD2_BW.h>

// Note: If the display doesn't work, try changing GxEPD2_213_B72 to GxEPD2_213_B73 or other variants
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=5*/ 5, /*DC=17*/ 17, /*RST=16*/ 16, /*BUSY=4*/ 4));

void setup() {
  display.init();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(1);  // Test 0, 1, 2, or 3 to make it upright
  display.setTextSize(3);
  display.setCursor(10, 10);
  display.println("Hello World");
//  display.setCursor(10, display.getCursorY());
  
 display.fillCircle(200, 60, 20, GxEPD_BLACK);
 display.drawRect(100,10,80,50,GxEPD_BLACK);

  display.display();
  display.powerOff();
}

void loop() {
  // Nothing to do here
}
