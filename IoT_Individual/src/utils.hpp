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

    avgPlot = avg;

    return avg;
}


void printPerformanceReport(float average, int count, 
                            unsigned long mathTime, unsigned long networkLatency, 
                            unsigned long publishLatency, unsigned long generationLatency, 
                            int rawOverSampled, int rawAdaptive, int payloadSize) {

    Serial.println("\n=============================================================");
    Serial.println("                        REPORT PERFORMANCE");
    Serial.println("=============================================================");
    
    Serial.println("\n--- PER-WINDOW EXECUTION TIME ---");
    Serial.printf("FFT Execution:           %lu us (%.2f ms)\n", fftDurationUs, fftDurationUs/1000.0);
    Serial.printf("Mean Calculation:        %lu us\n", mathTime);
    Serial.printf("Calc + FFT:              %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
    
    Serial.println("\n--- LATENCY ANALYSIS ---");
    Serial.printf("Generation Latency E2E:  %lu us (%.2f ms)\n", generationLatency, generationLatency/1000.0);
    Serial.printf("Network Latency:         %lu us (%.2f ms)\n", networkLatency, networkLatency/1000.0);
    Serial.printf("Total Publish Latency:   %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
    
    Serial.println("\n--- DATA VOLUME COMPARISON ---");
    Serial.printf("Over-sampled:            %d Bytes\n", rawOverSampled);
    Serial.printf("Adaptive Sampling:       %d Bytes\n", rawAdaptive);
    Serial.printf("Payload size:            %d Bytes\n", payloadSize);
    
    float Adaptive = (1.0 - (float)rawAdaptive / (float)rawOverSampled) * 100.0;
    float Aggregation = (1.0 - (float)payloadSize / (float)rawAdaptive) * 100.0;
    float Total = (1.0 - (float)payloadSize / (float)rawOverSampled) * 100.0;
    
    Serial.println("\n--- DATA REDUCTION ---");
    Serial.printf("1. FFT Adaptive:         %.1f%% (Risparmiati %d Bytes)\n", Adaptive, rawOverSampled - rawAdaptive);
    Serial.printf("2. Aggregation:          %.1f%% (Risparmiati %d Bytes)\n", Aggregation, rawAdaptive - payloadSize);
    Serial.printf("3. Total reduction:      %.2f%% (Da %d a %d Bytes!)\n", Total, rawOverSampled, payloadSize);
    
    Serial.println("\n--- SAMPLING FREQUENCY & WINDOW ---");
    xSemaphoreTake(freqMutex, portMAX_DELAY);
    float fs_current = currentSamplingFrequency;
    xSemaphoreGive(freqMutex);
    
    float window_duration_ms = (SAMPLES / fs_current) * 1000.0;
    Serial.printf("Current Sampling Freq:   %.2f Hz\n", fs_current);
    Serial.printf("Window Duration FFT:     %.2f ms (per %d samples)\n", window_duration_ms, SAMPLES);
    Serial.printf("Samples aggregati:       %d\n", count);
    
    Serial.println("\n=============================================================\n");
}

void runBandwidthStressTest() {
    Serial.println("\n=============================================================");
    Serial.println("           AVVIO STRESS TEST TRASMISSIONE DATI MQTT          ");
    Serial.println("=============================================================");

    // Creiamo un pacchetto dummy da 200 Byte (il limite di sicurezza per PubSubClient)
    char chunk[200];
    memset(chunk, 'A', 199); // Riempe di lettere 'A'
    chunk[199] = '\0';       // Terminatore stringa

    // Creiamo il pacchetto intelligente da 16 Byte
    char smartPayload[20] = "Average: 1850.50";

    // Assicuriamoci che MQTT sia connesso prima di testare
    if (!client.connected()) {
        Serial.println("Errore: MQTT non connesso. Impossibile fare il test.");
        return;
    }

    // ---------------------------------------------------------
    // TEST 1: PAYLOAD EDGE COMPUTING (16 Byte)
    // ---------------------------------------------------------
    Serial.println("[TEST 1] Invio Payload Aggregato (16 Byte)...");
    unsigned long startPayload = micros();
    
    client.publish("esp32/stresstest", smartPayload);
    
    unsigned long timePayload = micros() - startPayload;
    Serial.printf("-> Tempo impiegato: %lu us (%.2f ms)\n\n", timePayload, timePayload/1000.0);
    delay(500); // Pausa per far respirare il router

    // ---------------------------------------------------------
    // TEST 2: ADAPTIVE SAMPLING (es. 400 Byte)
    // Richiede 2 invii da 200 Byte
    // ---------------------------------------------------------
    Serial.println("[TEST 2] Invio Adaptive Sampling (400 Byte in 2 pacchetti)...");
    unsigned long startAdaptive = micros();
    
    for(int i = 0; i < 2; i++) {
        client.publish("esp32/stresstest", chunk);
    }
    
    unsigned long timeAdaptive = micros() - startAdaptive;
    Serial.printf("-> Tempo impiegato: %lu us (%.2f ms)\n\n", timeAdaptive, timeAdaptive/1000.0);
    delay(500);

    // ---------------------------------------------------------
    // TEST 3: OVERSAMPLED RAW DATA (12000 Byte)
    // Richiede 60 invii continui da 200 Byte
    // ---------------------------------------------------------
    Serial.println("[TEST 3] Invio Dati Grezzi Oversampled (12000 Byte in 60 pacchetti)...");
    unsigned long startRaw = micros();
    
    for(int i = 0; i < 60; i++) {
        client.publish("esp32/stresstest", chunk);
        
        // È fondamentale chiamare il loop e inserire un micro-delay, 
        // altrimenti il buffer del chip WiFi si satura e il router ci disconnette (DDoS locale!)
        client.loop(); 
        delay(2); 
    }
    
    unsigned long timeRaw = micros() - startRaw;
    Serial.printf("-> Tempo impiegato: %lu us (%.2f ms)\n\n", timeRaw, timeRaw/1000.0);

    Serial.println("=============================================================");
    Serial.println("                 STRESS TEST COMPLETATO                      ");
    Serial.println("=============================================================\n");
}