#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduinoFFT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/semphr.h>

// Definizione Pin Heltec V3
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define VEXT_PIN 36

// Configurazione FFT
#define SAMPLES 128             // Numero di campioni
#define MAX_SAMPLING_FREQ 1000  // Frequenza di campionamento massima

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

const int sensorPin = 1;         // GPIO 1
volatile int sharedRawVal = 2048; // Variabile condivisa tra i task

// Double-buffering per FFT
double vReal0[SAMPLES], vReal1[SAMPLES];
double vImag0[SAMPLES], vImag1[SAMPLES];

double *procReal = vReal0;  // Buffer che Sampling sta riempiendo
double *procImag = vImag0;
double *fftReal = vReal1;   // Buffer che FFT sta elaborando
double *fftImag = vImag1;

volatile float currentSamplingFrequency = MAX_SAMPLING_FREQ;

// Configurazione WiFi e MQTT
const char* ssid = "IoT";
const char* password = "12345678";
const char* mqtt_server = "192.168.137.1"; // IP del hotspot del PC

WiFiClient espClient;
PubSubClient client(espClient);

// Variabili di aggregazione
volatile float windowSum;
volatile int windowCount;
SemaphoreHandle_t windowMutex;
SemaphoreHandle_t freqMutex;
SemaphoreHandle_t xFFTReady;  // Segnala che i campioni sono pronti
SemaphoreHandle_t xFFTFinished; // Segnala che FFT ha finito

// === METRICHE DI PERFORMANCE ===
volatile unsigned long windowStartTime = 0;      // Inizio finestra (primo campione)
volatile unsigned long fftStartTime = 0;         // Inizio FFT
volatile unsigned long fftEndTime = 0;           // Fine FFT
volatile unsigned long fftDurationUs = 0;        // Durata FFT in microsecond
volatile unsigned long generationLatencyUs = 0;  // Dal primo sample al publish


// TASKS
void TaskSampling(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskFFT(void *pvParameters);
void TaskMQTT(void *pvParameters);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- AVVIO SISTEMA ---");

  // 1. Alimentazione Display
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(500);

  // 3. Inizializzazione I2C con velocità standard (100kHz)
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000); 

  // 4. Inizializzazione Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERRORE: Schermo non trovato!");
  } else {
    Serial.println("SCHERMO: Trovato e Inizializzato!");
  }

  display.clearDisplay();
  analogReadResolution(12);

  // Creazione mutex e semafori
  windowMutex = xSemaphoreCreateMutex();
  freqMutex = xSemaphoreCreateMutex();
  xFFTReady = xSemaphoreCreateBinary();
  xFFTFinished = xSemaphoreCreateBinary();
  xSemaphoreGive(xFFTFinished); // Inizializza: FFT è "già finita" al boot
  
  xTaskCreatePinnedToCore(
    TaskSampling,   // Funzione che legge l'ADC
    "Sampling",
    4096,
    NULL,
    3,              // Priorità alta
    NULL,           
    1               // Core 1
  );

  xTaskCreatePinnedToCore(
    TaskDisplay,    // Funzione che disegna
    "Display",
    4092,
    NULL,
    1,              // Priorità bassa
    NULL,           
    0               // Core 0
  );

  xTaskCreatePinnedToCore(
    TaskFFT,        // Funzione che esegue FFT
    "FFT",
    8192,
    NULL,
    2,              // Priorità media (tra sampling e display)
    NULL,
    0               // Core 0
  );

  xTaskCreatePinnedToCore(
    TaskMQTT,       // Funzione che invia dati via MQTT
    "MQTT",
    8192,
    NULL,
    1,              // Priorità bassa
    NULL,
    0               // Core 0
  );
}


void loop() {
  // Il loop rimane vuoto in FreeRTOS
}

// --- TASK 1: CAMPIONAMENTO ---
void TaskSampling(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  int sampleIndex = 0;

  for (;;) {

    // === TRACCIA INIZIO FINESTRA (primo campione) ===
    if (sampleIndex == 0) {
      windowStartTime = micros();
    }

    // Leggi il segnale
    sharedRawVal = analogRead(sensorPin);

    xSemaphoreTake(windowMutex, portMAX_DELAY);
    windowSum += sharedRawVal;
    windowCount++;
    xSemaphoreGive(windowMutex);
    
    // Scrivi nel buffer di processo (procReal/procImag)
    procReal[sampleIndex] = (double)sharedRawVal;
    procImag[sampleIndex] = 0.0;
    sampleIndex++;
    
    if (sampleIndex >= SAMPLES) {
      sampleIndex = 0;
      // Aspetta che la FFT abbia finito col buffer precedente
      xSemaphoreTake(xFFTFinished, portMAX_DELAY);
      
      // Swap buffer pointers per il prossimo ciclo (true double-buffering)
      double *tempReal = procReal;
      double *tempImag = procImag;
      procReal = fftReal;
      procImag = fftImag;
      fftReal = tempReal;
      fftImag = tempImag;
      
      // Segnala al task FFT che i dati sono pronti
      xSemaphoreGive(xFFTReady);
    }

    xSemaphoreTake(freqMutex, portMAX_DELAY);
    float currentFreq = currentSamplingFrequency;
    xSemaphoreGive(freqMutex);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS((int)(1000.0 / currentFreq)));
  }
}

// --- TASK 2: VISUALIZZAZIONE ---
void TaskDisplay(void *pvParameters) {
  static int x = 0;
  static int lastY = 32;
  static unsigned long lastSerialPrint = 0;

  for (;;) {
    int y = map(sharedRawVal, 0, 4095, 63, 0);
    display.drawLine(x - 1, lastY, x, y, SSD1306_WHITE);
    
    if (x % 4 == 0) {
        xSemaphoreTake(freqMutex, portMAX_DELAY);
        display.setCursor(0,0);
        display.fillRect(0,0,128,10, SSD1306_BLACK);
        display.print("Fs: "); display.print(currentSamplingFrequency); display.print("Hz");
        display.display();
        xSemaphoreGive(freqMutex);
    }

    lastY = y;
    x++;
    if (x >= 128) { x = 0; display.clearDisplay(); }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- TASK 3: ANALISI FFT ---
void TaskFFT(void *pvParameters) {
  while (1) {
    if (xSemaphoreTake(xFFTReady, portMAX_DELAY) == pdTRUE) {
      
      xSemaphoreTake(freqMutex, portMAX_DELAY);
      double currentFreq = currentSamplingFrequency;
      xSemaphoreGive(freqMutex);

      // Re-link the library to the newly swapped proc pointers
      ArduinoFFT<double> FFT = ArduinoFFT<double>(fftReal, fftImag, SAMPLES, currentFreq);

      // === INIZIO MISURAZIONE FFT ===
      fftStartTime = micros();

      // 1. Elaborazione FFT
      FFT.dcRemoval();
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(FFT_FORWARD);
      FFT.complexToMagnitude();

      // === FINE MISURAZIONE FFT ===
      fftEndTime = micros();
      fftDurationUs = fftEndTime - fftStartTime;

      // Find Max Frequency (Shannon check)
      double threshold = 500.0;
      int topBin = 0;
      for (int i = (SAMPLES / 2) - 1; i >= 0; i--) {
        if (fftReal[i] > threshold) {
          topBin = i;
          break;
        }
      }
      
      double f_max = (topBin * currentFreq) / (double)SAMPLES;

      // 3. LOGICA ADATTIVA (Nyquist: Fs = 2 * Fmax)
      float recommendedFreq = f_max * 2.5;
      
      // Limiti di sicurezza per l'hardware
      if (recommendedFreq < 20.0) {
          recommendedFreq = 20.0;
      }
      if (recommendedFreq > MAX_SAMPLING_FREQ) {
          recommendedFreq = MAX_SAMPLING_FREQ;
      }

      Serial.printf("Detected Max Freq: %.2f Hz | Suggested Fs: %.2f Hz | Applied Fs: %.2f Hz\n", f_max, f_max * 2.5, recommendedFreq);

      xSemaphoreTake(freqMutex, portMAX_DELAY);
      currentSamplingFrequency = recommendedFreq;
      xSemaphoreGive(freqMutex);

      // --- THE HANDSHAKE ---
      // Tell the Sampler: "I am done, you can have the buffer back"
      xSemaphoreGive(xFFTFinished);
    }
  }
}


// --- TASK 4: MQTT ---
void TaskMQTT(void *pvParameters) {
  // Questo task si occuperà di inviare i dati rilevati al broker MQTT
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, 1883);

  for(;;){
    //finestra di 3 secondi
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()){
        Serial.println("Connessione al broker MQTT...");
        client.connect("ESP32Client");
      }

      if (client.connected()) {
        xSemaphoreTake(windowMutex, portMAX_DELAY);
        if (windowCount > 0) {
          unsigned long startTime = micros();
          // Calcola la media della finestra
          float sum = windowSum;
          int count = windowCount;
          windowSum = 0;
          windowCount = 0;
          xSemaphoreGive(windowMutex);
        
        float average = sum / count;

        int rawOverSampled = MAX_SAMPLING_FREQ * 3 * 4; // 1000Hz * 3s * 4bytes per intero
        int rawAdaptive = count * 4; // count * 4bytes per intero 

        char payload[150];
        snprintf(payload, sizeof(payload), "Average: %.2f", average);
        
        unsigned long startPublishTime = micros();
        client.publish("esp32/average", payload);
        unsigned long endPublishTime = micros();

        // === CALCOLO METRICHE COMPLETE ===
        unsigned long mathTime = startPublishTime - startTime;              // Solo il calcolo matematico
        unsigned long networkLatency = endPublishTime - startPublishTime;   // Solo il tempo di invio WiFi
        unsigned long publishLatency = endPublishTime - startTime;          // Math + Network
        unsigned long generationLatency = endPublishTime - windowStartTime; // Dal primo sample al publish (LATENZA REALE)

        // STAMPA SULLA SERIALE PER DEBUG
        Serial.printf("VALORE MEDIO: %.2f\n\n", average);
        Serial.printf("VALORE %d\n\n",sharedRawVal);

        // === REPORT PERFORMANCE COMPLETO ===
        Serial.println("\n=============================================================");
        Serial.println("          REPORT PERFORMANCE METRICHE COMPLETE");
        Serial.println("=============================================================");
        
        Serial.println("\n--- PER-WINDOW EXECUTION TIME ---");
        Serial.printf("FFT Execution:              %lu us (%.2f ms)\n", fftDurationUs, fftDurationUs/1000.0);
        Serial.printf("Media Calculation:          %lu us\n", mathTime);
        Serial.printf("Calcoli + Math:             %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
        
        Serial.println("\n--- LATENCY ANALYSIS ---");
        Serial.printf("Generation Latency:         %lu us (%.2f ms)\n", generationLatency, generationLatency/1000.0);
        Serial.println("  (Dal primo sample al publish MQTT)");
        Serial.println("  (Include: Sampling + FFT + Aggregation + Math)");
        Serial.printf("Network Latency (WiFi):     %lu us (%.2f ms)\n", networkLatency, networkLatency/1000.0);
        Serial.println("  (Solo tempo trasmissione MQTT publish)");
        Serial.printf("Total Publish Latency:      %lu us (%.2f ms)\n", publishLatency, publishLatency/1000.0);
        
        Serial.println("\n--- DATA VOLUME COMPARISON ---");
        Serial.printf("Over-sampled (1000Hz * 3s): %d Bytes\n", rawOverSampled);
        Serial.printf("Adaptive Sampling:          %d Bytes\n", rawAdaptive);
        Serial.printf("Payload MQTT (JSON):        %d Bytes\n", (int)strlen(payload));
        float dataReduction = (1.0 - (float)strlen(payload)/(float)rawOverSampled)*100.0;
        Serial.printf("Data Reduction:             %.1f%% (vs over-sampled)\n", dataReduction);
        Serial.printf("RISPARMIO BANDA:            -%d Bytes per finestra!\n", (rawOverSampled - (int)strlen(payload)));
        
        Serial.println("\n--- SAMPLING FREQUENCY & WINDOW ---");
        xSemaphoreTake(freqMutex, portMAX_DELAY);
        float fs_current = currentSamplingFrequency;
        xSemaphoreGive(freqMutex);
        
        float window_duration_ms = (SAMPLES / fs_current) * 1000.0;
        Serial.printf("Current Sampling Freq:      %.2f Hz\n", fs_current);
        Serial.printf("Window Duration:            %.2f ms (per %d samples)\n", window_duration_ms, SAMPLES);
        Serial.printf("Samples per 3s:             %d\n", count);
        
        Serial.println("\n=============================================================\n");
        
        }else {
          xSemaphoreGive(windowMutex);
        }
      }
    }
    else{
      Serial.println("WiFi disconnesso");
    }
  }
}
