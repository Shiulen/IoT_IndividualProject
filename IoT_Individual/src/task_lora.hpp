#include "globals.h"

extern LoRaWANNode node;

void TaskLora(void *pvParameters) {
    Serial.println("[Task LoRa] Inizializzazione in corso...");
    updateStatus("Radio Init...");

    int16_t state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[Task LoRa] Errore durante l'inizializzazione della radio");
        updateStatus("Radio Error");
        vTaskDelete(NULL);
    }
    Serial.println("[Task LoRa] Modulo SX1262 Pronto.");

    // ======= ABP =======
    Serial.println("[Task LoRa] Attivazione in corso (ABP)...");
    updateStatus("ABP Activation...");

    state = node.beginABP(devAddr, NULL, NULL, nwkSKey, appSKey);
    state=node.activateABP();

    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
        Serial.println("[Task LoRa] +++ NODO ATTIVO IN MODALITA' ABP! +++");
        updateStatus("TTN Connesso!");
    } else {
        Serial.printf("[Task LoRa] Errore critico ABP: %d\n", state);
    }
    /* ======= OOTA =======
    persist.loadSession(&node);

    if (!node.isActivated()) {
        Serial.println("[Task LoRa] Richiesta JOIN a The Things Network (OTAA)...");
        updateStatus("Join TTN in corso...");
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

    //node.setDutyCycle(true, 1250);
    node.setDutyCycle(false); 
    TickType_t xLast = xTaskGetTickCount();

    for (;;) {
        Serial.printf("\n[Task LoRa] Pausa per rispetto Duty Cycle (%d secondi)...\n", MINIMUM_DELAY);
        vTaskDelayUntil(&xLast, pdMS_TO_TICKS(MINIMUM_DELAY * 1000));

        Serial.println("\n[Task LoRa] Risveglio: Preparazione Trasmissione.");
        updateStatus("Preparazione TX LoRa...");

        int cnt = 0;
        float avg = getAggregatedAvg(cnt);

        uint32_t tempoDiAttesa = node.timeUntilUplink();
        if (tempoDiAttesa > 0) {
            Serial.printf("[Task LoRa] ATTENZIONE: Duty Cycle attivo. Per legge devo aspettare ancora %lu millisecondi!\n", tempoDiAttesa);
        }

        if (cnt > 0 && node.isActivated()) {
            Serial.printf("[Task LoRa] Dati aggregati. Media: %.2f (su %d campioni)\n", avg, cnt);
            updateStatus("TX...");
            uint16_t valToSend = (uint16_t)avg;
            uint8_t payload[2];
            payload[0] = highByte(valToSend);
            payload[1] = lowByte(valToSend);

            Serial.println("[Task LoRa] SPEDIZIONE PACCHETTO NELL'ARIA...");

            state = node.sendReceive(payload, 2, 1);

            if (state == RADIOLIB_ERR_NONE) {
                Serial.println("[Task LoRa] SUCCESSO: Pacchetto confermato (Downlink) dal Gateway!");
                updateStatus("TX Confermato OK!");
            } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
                Serial.println("[Task LoRa] SUCCESSO: Pacchetto Inviato (Unconfirmed, nessuna risposta dal Gateway).");
                updateStatus("TX Inviato OK!");
            } else {
                Serial.printf("[Task LoRa] ERRORE DI INVIO: %d\n", state);
                updateStatus("ERR: TX Fallita");
            }
        }else {
        Serial.println("[Task LoRa] Nessun dato da inviare o non connesso a TTN.");
        }
    
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        updateStatus("Campionamento...");
    }
}