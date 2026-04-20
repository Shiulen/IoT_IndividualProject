#include "globals.h"

void TaskSampling(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  int sampleIndex = 0;

  Serial.println("--- TASK SAMPLING AVVIATO ---");
  updateStatus("Campionamento in corso...");

  for (;;) {
    sharedRawVal = analogRead(sensorPin);

    xSemaphoreTake(windowMutex, portMAX_DELAY);
    if(windowCount == 0) aggregationStartTime = micros();
    windowSum += sharedRawVal;
    windowCount++;
    xSemaphoreGive(windowMutex);
    
    procReal[sampleIndex] = (double)sharedRawVal;
    procImag[sampleIndex] = 0.0;
    sampleIndex++;
    
    if (sampleIndex >= SAMPLES) {
        sampleIndex = 0;
        xSemaphoreTake(xFFTFinished, portMAX_DELAY);

        double *tempReal = procReal;
        double *tempImag = procImag;
        procReal = fftReal;
        procImag = fftImag;
        fftReal = tempReal;
        fftImag = tempImag;

        xSemaphoreGive(xFFTReady);
    }

    xSemaphoreTake(freqMutex, portMAX_DELAY);
    float currentFreq = currentSamplingFrequency;
    xSemaphoreGive(freqMutex);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS((int)(1000.0 / currentFreq)));
  }
}