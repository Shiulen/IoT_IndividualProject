#include "globals.h"

float getAggregatedAvg(int &out){
    float avg = 0;

    xSemaphoreTake(windowMutex, portMAX_DELAY);
    if (windowCount > 0) {
        avg = windowSum / windowCount;
        out = windowCount;
        windowSum = 0;
        windowCount = 0;
    }
    xSemaphoreGive(windowMutex);

    return avg;
}


void printPerformanceReport(float average, int count, 
                            unsigned long mathTime, unsigned long networkLatency, 
                            unsigned long publishLatency, unsigned long generationLatency, 
                            int rawOverSampled, int rawAdaptive, int payloadSize) {
    
    Serial.printf("VALORE MEDIO: %.2f\n\n", average);
    Serial.printf("VALORE RAW CORRENTE: %d\n\n", sharedRawVal);

    Serial.println("\n=============================================================");
    Serial.println("          REPORT PERFORMANCE METRICHE COMPLETE");
    Serial.println("=============================================================");
    
    Serial.println("\n--- PER-WINDOW EXECUTION TIME ---");
    Serial.printf("FFT Execution:              %lu us (%.2f ms)\n", fftDurationUs, fftDurationUs/1000.0);
    Serial.printf("Media Calculation:          %lu us\n", mathTime);
    Serial.printf("Calcoli + Math:             %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
    
    Serial.println("\n--- LATENCY ANALYSIS ---");
    Serial.printf("Generation Latency E2E:     %lu us (%.2f ms)\n", generationLatency, generationLatency/1000.0);
    Serial.println("  (Dal primo sample acquisito al publish concluso)");
    Serial.printf("Network Latency (WiFi):     %lu us (%.2f ms)\n", networkLatency, networkLatency/1000.0);
    Serial.printf("Total Publish Latency:      %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
    
    Serial.println("\n--- DATA VOLUME COMPARISON ---");
    Serial.printf("Over-sampled (1000Hz * 3s): %d Bytes\n", rawOverSampled);
    Serial.printf("Adaptive Sampling:          %d Bytes\n", rawAdaptive);
    Serial.printf("Payload MQTT (JSON):        %d Bytes\n", payloadSize);
    
    float dataReduction = (1.0 - (float)payloadSize / (float)rawOverSampled) * 100.0;
    Serial.printf("Data Reduction:             %.1f%% (vs over-sampled)\n", dataReduction);
    Serial.printf("RISPARMIO BANDA:            -%d Bytes per finestra!\n", (rawOverSampled - payloadSize));
    
    Serial.println("\n--- SAMPLING FREQUENCY & WINDOW ---");
    xSemaphoreTake(freqMutex, portMAX_DELAY);
    float fs_current = currentSamplingFrequency;
    xSemaphoreGive(freqMutex);
    
    float window_duration_ms = (SAMPLES / fs_current) * 1000.0;
    Serial.printf("Current Sampling Freq:      %.2f Hz\n", fs_current);
    Serial.printf("Window Duration FFT:        %.2f ms (per %d samples)\n", window_duration_ms, SAMPLES);
    Serial.printf("Samples aggregati:          %d\n", count);
    
    Serial.println("\n=============================================================\n");
}