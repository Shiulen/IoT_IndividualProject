#include "globals.h"

extern WiFiClient espClient;
extern PubSubClient client;

bool publishToMQTT(float average, char* payloadBuffer, size_t payloadSize, 
                   unsigned long &startPub, unsigned long &endPub) {
    snprintf(payloadBuffer, payloadSize, "Average: %.2f", average);
    
    startPub = micros();
    bool success = client.publish("esp32/average", payloadBuffer);
    endPub = micros();
    
    return success;
}

void TaskMQTT(void *pvParameters) {
    WiFi.begin(ssid, password);
    client.setServer(mqtt_server, 1883);

    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(3000));

        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) {
                Serial.println("Connessione al broker MQTT...");
                client.connect("ESP32Client");
            }

            if (client.connected()) {
                unsigned long startTime = micros();
                
                int count = 0;
                float average = getAggregatedAvg(count);

                unsigned long mathTime = micros() - startTime;

                if (count > 0) {
                    char payload[150];
                    unsigned long startPublishTime, endPublishTime;

                    publishToMQTT(average, payload, sizeof(payload), startPublishTime, endPublishTime);

                    unsigned long networkLatency = endPublishTime - startPublishTime;   
                    unsigned long publishLatency = endPublishTime - startTime;          
                    unsigned long generationLatency = endPublishTime - aggregationStartTime;

                    int rawOverSampled = realOversampledPerWindow * 4; 
                    int rawAdaptive = count * 4; 

                    // 4. Stampa Report
                    printPerformanceReport(average, count, mathTime, networkLatency, 
                                           publishLatency, generationLatency, 
                                           rawOverSampled, rawAdaptive, strlen(payload));
                }
            }
        } else {
            Serial.println("WiFi disconnesso");
        }
    }
}