#include "globals.h"

void TaskFFT(void *pvParameters) {
  Serial.println("[Task FFT] In attesa di dati...");
  while (1) {
    if (xSemaphoreTake(xFFTReady, portMAX_DELAY) == pdTRUE) {
      
      updateStatus("Analisi FFT...");

      xSemaphoreTake(freqMutex, portMAX_DELAY);
      double currentFreq = currentSamplingFrequency;
      xSemaphoreGive(freqMutex);

      ArduinoFFT<double> FFT = ArduinoFFT<double>(fftReal, fftImag, SAMPLES, currentFreq);

      fftStartTime = micros();

      FFT.dcRemoval();
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(FFT_FORWARD);
      FFT.complexToMagnitude();

      fftEndTime = micros();
      fftDurationUs = fftEndTime - fftStartTime;

      double threshold = 500.0;
      int topBin = 0;
      for (int i = (SAMPLES / 2) - 1; i >= 0; i--) {
        if (fftReal[i] > threshold) {
          topBin = i;
          break;
        }
      }
      
      double f_max = (topBin * currentFreq) / (double)SAMPLES;
      float recommendedFreq = f_max * 2.5;
      
      if (recommendedFreq < 15.0) {
          recommendedFreq = 15.0;
      }
      if (recommendedFreq > MAX_SAMPLING_FREQ) {
          recommendedFreq = MAX_SAMPLING_FREQ;
      }

      Serial.printf("Detected Max Freq: %.2f Hz | Suggested Fs: %.2f Hz | Applied Fs: %.2f Hz\n", f_max, f_max * 2.5, recommendedFreq);

      xSemaphoreTake(freqMutex, portMAX_DELAY);
      currentSamplingFrequency = recommendedFreq;
      xSemaphoreGive(freqMutex);

      updateStatus("Campionamento...");
      xSemaphoreGive(xFFTFinished);
    }
  }
}
