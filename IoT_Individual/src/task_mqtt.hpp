#include "globals.h"

extern WiFiClient espClient;
extern PubSubClient client;

void TaskMQTT(void *pvParameters) {
  // Questo task si occuperà di inviare i dati rilevati al broker MQTT
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);

  for(;;){
    //finestra di 3 secondi
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()){
        Serial.println("Connessione al broker MQTT...");
        client.connect("ESP32Client");
      }

      if (client.connected()) {
        xSemaphoreTake(windowMutex, portMAX_DELAY);
        if (windowCount > 0) {
          unsigned long startTime = micros();
          // Calcola la media della finestra
          float sum = windowSum;
          int count = windowCount;
          windowSum = 0;
          windowCount = 0;
          xSemaphoreGive(windowMutex);
        
        float average = sum / count;

        int rawOverSampled = MAX_SAMPLING_FREQ * 3 * 4; // 1000Hz * 3s * 4bytes per intero
        int rawAdaptive = count * 4; // count * 4bytes per intero 

        char payload[150];
        snprintf(payload, sizeof(payload), "Average: %.2f", average);
        
        unsigned long startPublishTime = micros();
        client.publish("esp32/average", payload);
        unsigned long endPublishTime = micros();

        // === CALCOLO METRICHE COMPLETE ===
        unsigned long mathTime = startPublishTime - startTime;              // Solo il calcolo matematico
        unsigned long networkLatency = endPublishTime - startPublishTime;   // Solo il tempo di invio WiFi
        unsigned long publishLatency = endPublishTime - startTime;          // Math + Network
        unsigned long generationLatency = endPublishTime - windowStartTime; // Dal primo sample al publish (LATENZA REALE)

        // STAMPA SULLA SERIALE PER DEBUG
        Serial.printf("VALORE MEDIO: %.2f\n\n", average);
        Serial.printf("VALORE %d\n\n",sharedRawVal);

        // === REPORT PERFORMANCE COMPLETO ===
        Serial.println("\n=============================================================");
        Serial.println("          REPORT PERFORMANCE METRICHE COMPLETE");
        Serial.println("=============================================================");
        
        Serial.println("\n--- PER-WINDOW EXECUTION TIME ---");
        Serial.printf("FFT Execution:              %lu us (%.2f ms)\n", fftDurationUs, fftDurationUs/1000.0);
        Serial.printf("Media Calculation:          %lu us\n", mathTime);
        Serial.printf("Calcoli + Math:             %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
        
        Serial.println("\n--- LATENCY ANALYSIS ---");
        Serial.printf("Generation Latency:         %lu us (%.2f ms)\n", generationLatency, generationLatency/1000.0);
        Serial.println("  (Dal primo sample al publish MQTT)");
        Serial.println("  (Include: Sampling + FFT + Aggregation + Math)");
        Serial.printf("Network Latency (WiFi):     %lu us (%.2f ms)\n", networkLatency, networkLatency/1000.0);
        Serial.println("  (Solo tempo trasmissione MQTT publish)");
        Serial.printf("Total Publish Latency:      %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
        
        Serial.println("\n--- DATA VOLUME COMPARISON ---");
        Serial.printf("Over-sampled (1000Hz * 3s): %d Bytes\n", rawOverSampled);
        Serial.printf("Adaptive Sampling:          %d Bytes\n", rawAdaptive);
        Serial.printf("Payload MQTT (JSON):        %d Bytes\n", (int)strlen(payload));
        float dataReduction = (1.0 - (float)strlen(payload)/(float)rawOverSampled)*100.0;
        Serial.printf("Data Reduction:             %.1f%% (vs over-sampled)\n", dataReduction);
        Serial.printf("RISPARMIO BANDA:            -%d Bytes per finestra!\n", (rawOverSampled - (int)strlen(payload)));
        
        Serial.println("\n--- SAMPLING FREQUENCY & WINDOW ---");
        xSemaphoreTake(freqMutex, portMAX_DELAY);
        float fs_current = currentSamplingFrequency;
        xSemaphoreGive(freqMutex);
        
        float window_duration_ms = (SAMPLES / fs_current) * 1000.0;
        Serial.printf("Current Sampling Freq:      %.2f Hz\n", fs_current);
        Serial.printf("Window Duration:            %.2f ms (per %d samples)\n", window_duration_ms, SAMPLES);
        Serial.printf("Samples per 3s:             %d\n", count);
        
        Serial.println("\n=============================================================\n");
        
        }else {
          xSemaphoreGive(windowMutex);
        }
      }
    }
    else{
      Serial.println("WiFi disconnesso");
    }
  }
}