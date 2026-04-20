#pragma once

#include <Arduino.h>
#include <arduinoFFT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/semphr.h>
#include <heltec_unofficial.h>
#include <LoRaWAN_ESP32.h>

// ======== LORA CONFIG =======
extern uint64_t deveui;
extern uint64_t appeui;
extern uint8_t appkey[16];
extern LoRaWANNode node;
extern bool useLoraMode;
#define MINIMUM_DELAY 30

// ======== CONFIG COSTANTI =======
#define SAMPLES 128
#define MAX_SAMPLING_FREQ 1000.0
extern const int sensorPin;

// ======== GLOBAL VARS =======
extern volatile int sharedRawVal;
extern double vReal0[SAMPLES];
extern double vReal1[SAMPLES];
extern double vImag0[SAMPLES];
extern double vImag1[SAMPLES];
extern double *procReal;
extern double *procImag;
extern double *fftReal;
extern double *fftImag;
extern volatile float currentSamplingFrequency;
extern volatile char systemStatus[32];

// ======== WIFI & MQTT CONFIG =======
extern const char* ssid;
extern const char* password;
extern const char* mqtt_server;
extern WiFiClient espClient;
extern PubSubClient client;

// ======== AGGREGATION & METRICS =======
extern volatile float windowSum;
extern volatile int windowCount;
extern volatile unsigned long aggregationStartTime;
extern volatile unsigned long fftStartTime;
extern volatile unsigned long fftEndTime;
extern volatile unsigned long fftDurationUs;
extern int realOversampledPerWindow;

// ======== SEMAPHORES =======
extern SemaphoreHandle_t windowMutex;
extern SemaphoreHandle_t freqMutex;
extern SemaphoreHandle_t statusMutex;
extern SemaphoreHandle_t xFFTReady;
extern SemaphoreHandle_t xFFTFinished;

// ======== FUNCTION PROTOTYPES =======
void updateStatus(const char* newStatus);

// ======== TASKS =======
void TaskSampling(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskFFT(void *pvParameters);
void TaskMQTT(void *pvParameters);
void TaskLora(void *pvParameters);