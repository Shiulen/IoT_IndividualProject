#include "globals.h"

extern LoRaWANNode node;

void TaskLora(void *pvParameters) {
    Serial.println("[Task LoRa] Intialization...");

    int16_t state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[Task LoRa] ERROR SX1262 Initialization Failed!");
        vTaskDelete(NULL);
    }

    // ======= ABP =======
    Serial.println("[Task LoRa] Activation (ABP)...");

    state = node.beginABP(devAddr, NULL, NULL, nwkSKey, appSKey);
    state=node.activateABP();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
        Serial.println("[Task LoRa] Node activated successfully with ABP!");
    } else {
        Serial.printf("[Task LoRa] ERROR ABP: %d\n", state);
    }


    /* ======= OOTA =======
    persist.loadSession(&node);

    if (!node.isActivated()) {
        Serial.println("[Task LoRa] Richiesta JOIN a The Things Network (OTAA)...");
        node.beginOTAA(appeui, deveui, NULL, appkey);
        state = node.activateOTAA();

        if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
            Serial.println("[Task LoRa] +++ JOIN AVVENUTO CON SUCCESSO! +++");
            persist.saveSession(&node);
        } else {
        Serial.printf("[Task LoRa] FALLIMENTO JOIN (Codice %d). Riproverò dopo.\n", state);
        }
    }else {
      Serial.println("[Task LoRa] Nodo già attivato (Sessione recuperata).");
    }

    */


    // COMMENTED FOR DEBUGGING PURPOSES, TO AVOID WAITING FOR THE DUTY CYCLE IN THE TESTS
    //node.setDutyCycle(true, 1250);


    node.setDutyCycle(false); 
    TickType_t xLast = xTaskGetTickCount();

    for (;;) {
        Serial.printf("\n[Task LoRa] PAUSING ( DUTY CYCLE PURPOSE -> %d sec)...\n", MINIMUM_DELAY);
        vTaskDelayUntil(&xLast, pdMS_TO_TICKS(MINIMUM_DELAY * 1000));

        Serial.println("\n[Task LoRa] WAKING UP... Checking for data to send.");

        unsigned long startMath = micros();

        int cnt = 0;
        float avg = getAggregatedAvg(cnt);

        unsigned long mathTime = micros() - startMath;

        if (cnt > 0 && node.isActivated()) {

            Serial.printf("[Task LoRa] Data has been aggregated. Avg: %.2f (on %d samples)\n", avg, cnt);
            uint16_t valToSend = (uint16_t)avg;
            uint8_t payload[2];
            payload[0] = highByte(valToSend);
            payload[1] = lowByte(valToSend);

            Serial.println("[Task LoRa] SENDING PACKET...");

            unsigned long startNetwork = micros();

            state = node.sendReceive(payload, 2, 1);

            unsigned long endNetwork = micros();
            unsigned long networkLatency = endNetwork - startNetwork;
            unsigned long publishLatency = endNetwork - startMath;
            unsigned long generationLatency = endNetwork - aggregationStartTime;

            if (state == RADIOLIB_ERR_NONE) {
                Serial.println("[Task LoRa] SUCCESS: Packet confirmed (Downlink) by the Gateway!");
            } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
                Serial.println("[Task LoRa] SUCCESS: Packet sent (Unconfirmed, no response from the Gateway).");
            } else {
                Serial.printf("[Task LoRa] SEND ERROR: %d\n", state);
            }

            printPerformanceReport(avg,cnt,mathTime,networkLatency,publishLatency,generationLatency,realOversampledPerWindow*sizeof(int),cnt*sizeof(int),sizeof(payload));
        }else {
        Serial.println("[Task LoRa] No data to send or not connected to TTN.");
        }
    
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}