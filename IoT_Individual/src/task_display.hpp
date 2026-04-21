#include "globals.h"

void TaskDisplay(void *pvParameters) {
  static int x = 0;
  static int lastY = 32;

  for (;;) {
    int y = map(sharedRawVal, 0, 4095, 63, 15);
    display.drawLine(x - 1, lastY, x, y);     

    if (x % 4 == 0) {
        
        display.setColor(BLACK);
        display.fillRect(0, 0, 128, 14);
        display.setColor(WHITE);
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 0, "Fs:" + String(currentSamplingFrequency, 2));
        display.drawString(30, 0, "Avg:" + String(avgPlot, 2));
        display.display();


    }
    lastY = y;
    x++;
    if (x >= 128) { x = 0; display.clear(); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}