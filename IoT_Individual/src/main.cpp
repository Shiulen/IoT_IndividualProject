
#define SET_GLOBALS 
#include "globals.h"
#include "utils.hpp"
#include "task_sampling.hpp"
#include "task_display.hpp"
#include "task_fft.hpp"
#include "task_lora.hpp"
#include "task_mqtt.hpp"

// ======= LORA CONFIG (via OOTA) =======
// not used cause problems with join / reset
//uint64_t deveui = 0x70B3D57ED0076FF9;
//uint64_t appeui = 0x0000000000000000;
//uint8_t appkey[16] = {0xFD, 0x02, 0x55, 0xEA, 0x0B, 0xC4, 0x98, 0xDA, 0xAA, 0xF0, 0x2D, 0x9A, 0x7F, 0x74, 0xB3, 0xCB};


// ======= LORA CONFIG (via ABP) =======
uint32_t devAddr=0x260B121E;
uint8_t appSKey[16]={0x93, 0x56, 0xFE, 0xF3, 0x5C, 0xF8, 0x5C, 0x2A, 0x35, 0x1E, 0x4F, 0x33, 0x55, 0xCF, 0xF0, 0x85};
uint8_t nwkSKey[16]={0x3C, 0x94, 0x56, 0x68, 0x36, 0x94, 0x8E, 0xD8, 0xBA, 0x05, 0x9C, 0xCC, 0x09, 0x0E, 0xF0, 0xCF};
bool useLoraMode = true; // true = LoRaWAN, false = WiFi/MQTT

// ======= GLOBAL VARIABLES =======
const int sensorPin = 1; // GPIO1 (ADC)
volatile int sharedRawVal = 2048; // starting point of the sensor reading (midpoint of 12-bit ADC)
volatile float currentSamplingFrequency = MAX_SAMPLING_FREQ; // intialized to max, for oversampling calibration
volatile float avgPlot = 0;

// ======= FFT CONFIG =======
// buffers for fft processing
double vReal0[SAMPLES];
double vReal1[SAMPLES];
double vImag0[SAMPLES];
double vImag1[SAMPLES];
double *procReal = nullptr;
double *procImag = nullptr;
double *fftReal = nullptr;
double *fftImag = nullptr;

// ======= LORA & MQTT =======
//mqtt server config
const char* ssid = "IoT";
const char* password = "12345678";
const char* mqtt_server = "192.168.137.1";

// setup for measurement aggregation and timing
volatile float windowSum = 0;
volatile int windowCount = 0;
volatile unsigned long aggregationStartTime = 0; 
volatile unsigned long fftStartTime = 0;
volatile unsigned long fftEndTime = 0;
volatile unsigned long fftDurationUs = 0;
int realOversampledPerWindow = 3000;

// LoRaWAN node and MQTT client
LoRaWANNode node(&radio, &EU868);
WiFiClient espClient;
PubSubClient client(espClient);

// ======= SEMAPHORES =======
SemaphoreHandle_t windowMutex;
SemaphoreHandle_t freqMutex;
SemaphoreHandle_t xFFTReady;
SemaphoreHandle_t xFFTFinished;

void setup() {
    heltec_setup();
    Serial.begin(115200);

    pinMode(36, OUTPUT);
    digitalWrite(36, LOW);
    delay(50);

    Serial.printf("\n--- AVVIO SISTEMA (MODALITA: %s) ---\n", useLoraMode ? "LoRaWAN" : "WiFi/MQTT");
    
    // ======= OVERSAMPLING CALIBRATION =======
    // to retrive data for comparison
    Serial.println("\n--- OVERSAMPLING CALIBRATION (10 SEC) ---");
    display.clear();
    display.drawString(0, 0, "Fs:" + String(currentSamplingFrequency, 2) + " Hz");
    display.drawString(0, 20, "Calibrazione Hardware");
    display.drawString(0, 35, "Attendere 10s...");
    display.display();

    unsigned long startCalib = millis();
    long totalSamplesIn10s = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (millis() - startCalib < 10000) {
        analogRead(sensorPin);
        totalSamplesIn10s++;
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1)); 
    }

    display.clear();
    display.display();

    realOversampledPerWindow = (totalSamplesIn10s / 10.0) * 3.0;

    Serial.println("Calibrazione completata!");
    Serial.printf("Campioni reali letti in 10s: %ld\n", totalSamplesIn10s);
    Serial.printf("Baseline reale per finestra (3s): %d campioni\n\n", realOversampledPerWindow);

    // buffer intialization for FFT processing
    procReal = vReal0; procImag = vImag0;
    fftReal = vReal1;  fftImag = vImag1;

    // Semaphores initialization
    windowMutex = xSemaphoreCreateMutex();
    freqMutex = xSemaphoreCreateMutex();
    xFFTReady = xSemaphoreCreateBinary();
    xFFTFinished = xSemaphoreCreateBinary();
    xSemaphoreGive(xFFTFinished);

    // Task creation
    xTaskCreatePinnedToCore(TaskSampling, "Sampling", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(TaskDisplay,  "Display",  4092, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(TaskFFT,      "FFT",      8192, NULL, 2, NULL, 0);

    // through the useLoraMode flag, we can choose to run either the LoRaWAN task or the MQTT task
    if (useLoraMode) {
        xTaskCreatePinnedToCore(TaskLora, "Lora", 8192, NULL, 2, NULL, 0);
    } else {
        xTaskCreatePinnedToCore(TaskMQTT, "MQTT", 8192, NULL, 2, NULL, 0);
    }
}

void loop() { vTaskDelay(portMAX_DELAY); }