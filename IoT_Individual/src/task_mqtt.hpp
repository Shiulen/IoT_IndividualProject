#include "globals.h"

volatile unsigned long lastPublishTime = 0;
volatile unsigned long rttNetworkLatency = 0;
volatile bool ackReceived = false;

extern WiFiClient espClient;
extern PubSubClient client;

// Callback for ACK reception from the edge server (for E2E latency measurement)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "server/ack") == 0) {
        unsigned long ackTime = micros();
        rttNetworkLatency = ackTime - lastPublishTime;
        ackReceived=true;
    }
}

// To publish avg on MQTT)
bool publishToMQTT(float average, char* payloadBuffer, size_t payloadSize) {
    snprintf(payloadBuffer, payloadSize, "Average: %.2f", average);
    
    Serial.println("\n>>> SENDING PACKET TO EDGE SERVER... >>>");
    lastPublishTime = micros();
    bool success = client.publish("esp32/average", payloadBuffer);
    
    return success;
}

// ====== MQTT TASK ======
void TaskMQTT(void *pvParameters) {
    WiFi.begin(ssid, password);
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);

    for(;;) {
        unsigned long startWait = millis();
        while(millis() - startWait < 5000) {
            client.loop();
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) {
                Serial.println("CONNECTING TO MQTT BROKER...");
                if(client.connect("ESP32Client")) {
                    client.subscribe("server/ack");

                    /* PRINT FOR ANALYSIS
                    static bool test = false;
                    if(!test){
                        runBandwidthStressTest();
                        test=true;
                    }
                    */
                }
            }

            if (client.connected()) {

                int count = 0;

                unsigned long startFromHere = aggregationStartTime;

                unsigned long startMath = micros();
                float average = getAggregatedAvg(count);
                unsigned long mathTime = micros() - startMath;


                if (count > 0) {
                    char payload[150];

                    ackReceived=false;
                    unsigned long startNetwork = micros();
                    publishToMQTT(average, payload, sizeof(payload));
                    unsigned long waitStart = millis();
                    while(!ackReceived && (millis()- waitStart < 1000)){
                        client.loop();
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                    unsigned long endNetwork = micros();


                    unsigned long NetworkLatency = 0;
                    if(ackReceived) NetworkLatency=rttNetworkLatency;
                    else{
                        NetworkLatency=endNetwork-startNetwork;
                    }

                    unsigned long publishLatency = endNetwork - startMath;
                    unsigned long generationLatency = endNetwork - startFromHere;

                    // PRINT FOR PERFORMANCE
                    printPerformanceReport(average,count,mathTime,NetworkLatency,publishLatency,generationLatency,realOversampledPerWindow*sizeof(int),count*sizeof(int),strlen(payload));
                
                }
            }
        } else {
            Serial.println("WiFi disconnesso");
        }
    }
}