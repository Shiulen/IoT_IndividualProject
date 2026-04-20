#include <Arduino.h>

// Definiamo SET_GLOBALS solo qui, prima di includere l'header
#define SET_GLOBALS 
#include "globals.h"

#include "task_sampling.hpp"
#include "task_display.hpp"
#include "task_fft.hpp"
#include "task_lora.hpp"
#include "task_mqtt.hpp"

uint64_t deveui = 0x70B3D57ED0076FF9;
uint64_t appeui = 0x0000000000000000;
uint8_t appkey[16] = {0xFD, 0x02, 0x55, 0xEA, 0x0B, 0xC4, 0x98, 0xDA, 0xAA, 0xF0, 0x2D, 0x9A, 0x7F, 0x74, 0xB3, 0xCB};
bool useLoraMode = true;

const int sensorPin = 1;
volatile int sharedRawVal = 2048;
volatile float currentSamplingFrequency = MAX_SAMPLING_FREQ;
volatile char systemStatus[32] = "Inizializzazione...";

double vReal0[SAMPLES];
double vReal1[SAMPLES];
double vImag0[SAMPLES];
double vImag1[SAMPLES];
double *procReal = nullptr;
double *procImag = nullptr;
double *fftReal = nullptr;
double *fftImag = nullptr;

const char* ssid = "IoT";
const char* password = "12345678";
const char* mqtt_server = "192.168.137.1";

volatile float windowSum = 0;
volatile int windowCount = 0;
volatile unsigned long windowStartTime = 0;
volatile unsigned long fftStartTime = 0;
volatile unsigned long fftEndTime = 0;
volatile unsigned long fftDurationUs = 0;
volatile unsigned long generationLatencyUs = 0;

LoRaWANNode node(&radio, &EU868);
WiFiClient espClient;
PubSubClient client(espClient);

SemaphoreHandle_t windowMutex;
SemaphoreHandle_t freqMutex;
SemaphoreHandle_t statusMutex;
SemaphoreHandle_t xFFTReady;
SemaphoreHandle_t xFFTFinished;

void updateStatus(const char* newStatus) {
    if (statusMutex != NULL) {
        xSemaphoreTake(statusMutex, portMAX_DELAY);
        strncpy((char*)systemStatus, newStatus, sizeof(systemStatus) - 1);
        xSemaphoreGive(statusMutex);
    }
}

void setup() {
    heltec_setup(); // Inizializza display e radio una sola volta
    Serial.begin(115200);

    // Inizializzazione puntatori buffer
    procReal = vReal0; procImag = vImag0;
    fftReal = vReal1;  fftImag = vImag1;

    // Creazione Semafori
    windowMutex = xSemaphoreCreateMutex();
    freqMutex = xSemaphoreCreateMutex();
    statusMutex = xSemaphoreCreateMutex();
    xFFTReady = xSemaphoreCreateBinary();
    xFFTFinished = xSemaphoreCreateBinary();
    xSemaphoreGive(xFFTFinished);

    // Avvio Task comuni
    xTaskCreatePinnedToCore(TaskSampling, "Sampling", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskDisplay,  "Display",  4092, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(TaskFFT,      "FFT",      8192, NULL, 2, NULL, 0);

    if (useLoraMode) {
        xTaskCreatePinnedToCore(TaskLora, "Lora", 8192, NULL, 2, NULL, 0);
    } else {
        xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 8192, NULL, 2, NULL, 0);
    }
}

void loop() { vTaskDelay(portMAX_DELAY); }