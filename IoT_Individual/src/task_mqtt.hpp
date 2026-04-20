#include "globals.h"

volatile unsigned long lastPublishTime = 0;

extern WiFiClient espClient;
extern PubSubClient client;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "server/ack") == 0) {
        unsigned long ackTime = micros();
        unsigned long roundTripTime = ackTime - lastPublishTime;
        
        Serial.println("\n<<< [PING-PONG] RICEVUTO ACK DAL SERVER EDGE! <<<");
        Serial.printf("Tempo di Round-Trip (Andata + Ritorno): %.2f ms\n", roundTripTime / 1000.0);
    }
}

bool publishToMQTT(float average, char* payloadBuffer, size_t payloadSize) {
    snprintf(payloadBuffer, payloadSize, "Average: %.2f", average);
    
    Serial.println("\n>>> INVIO PACCHETTO AL SERVER EDGE... >>>");
    lastPublishTime = micros(); // FACCIO PARTIRE IL CRONOMETRO!
    bool success = client.publish("esp32/average", payloadBuffer);
    
    return success;
}

void TaskMQTT(void *pvParameters) {
    WiFi.begin(ssid, password);
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);

    for(;;) {
        unsigned long startWait = millis();
        while(millis() - startWait < 3000) {
            client.loop();
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) {
                Serial.println("Connessione al broker MQTT...");
                if(client.connect("ESP32Client")) {
                    client.subscribe("server/ack");
                }
            }

            if (client.connected()) {
                int count = 0;
                float average = getAggregatedAvg(count);

                if (count > 0) {
                    char payload[150];
                    publishToMQTT(average, payload, sizeof(payload));   
                }
            }
        } else {
            Serial.println("WiFi disconnesso");
        }
    }
}