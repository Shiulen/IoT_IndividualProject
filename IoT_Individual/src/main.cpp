#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduinoFFT.h>

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
double vReal[SAMPLES];
double vImag[SAMPLES];
volatile bool newDataReady = false; // Flag per indicare che nuovi dati sono pronti per l'FFT
volatile float currentSamplingFrequency = MAX_SAMPLING_FREQ;

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, MAX_SAMPLING_FREQ);

// TASKS
void TaskSampling(void *pvParameters);
void TaskDisplay(void *pvParameters);
void TaskFFT(void *pvParameters);

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
    1               // Core 1
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

    // Leggi il segnale
    sharedRawVal = analogRead(sensorPin);
    
    if(!newDataReady) {
      vReal[sampleIndex] = (double)sharedRawVal;
      vImag[sampleIndex] = 0.0;
      sampleIndex++;
      
      if (sampleIndex >= SAMPLES) {
        sampleIndex = 0;
        newDataReady = true;
      }
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS((int)(1000.0 / currentSamplingFrequency)));
  }
}

// --- TASK 2: VISUALIZZAZIONE ---
void TaskDisplay(void *pvParameters) {
static int x = 0;
  static int lastY = 32;
  for (;;) {
    int y = map(sharedRawVal, 0, 4095, 60, 3);
    display.drawLine(x - 1, lastY, x, y, SSD1306_WHITE);
    
    if (x % 2 == 0) {
        display.setCursor(0,0);
        display.fillRect(0,0,128,10, SSD1306_BLACK);
        display.print("Fs: "); display.print(currentSamplingFrequency); display.print("Hz");
        display.display();
    }
    lastY = y;
    x++;
    if (x >= 128) { x = 0; display.clearDisplay(); }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- TASK 3: ANALISI FFT ---
void TaskFFT(void *pvParameters) {
  for (;;) {
    if (newDataReady) {
      FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, (double)currentSamplingFrequency);

      // 1. Elaborazione FFT
      FFT.dcRemoval(); // Toglie l'offset di 1.65V
      FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.compute(FFT_FORWARD);
      FFT.complexToMagnitude();

      // 2. Trova la frequenza di picco
      double peakFreq = FFT.majorPeak();
      
      Serial.print("Peak Freq Rilevata: ");
      Serial.print(peakFreq);
      Serial.println(" Hz");

      // 3. LOGICA ADATTIVA (Nyquist: Fs = 2 * Fmax)
      // Aggiungiamo un margine di sicurezza (es. 2.5 invece di 2)
      float recommendedFreq = peakFreq * 2.5;
      
      // Limiti di sicurezza per l'hardware
      if (recommendedFreq < 15.0) {
          recommendedFreq = 15.0;
      }
      if (recommendedFreq > MAX_SAMPLING_FREQ) {
          recommendedFreq = MAX_SAMPLING_FREQ; // Non superare i 1000Hz
      }

      currentSamplingFrequency = recommendedFreq; // Aggiorna la frequenza di campionamento attuale
      
      Serial.print("Nuova Frequenza Adattata: ");
      Serial.println(currentSamplingFrequency);

      vTaskDelay(pdMS_TO_TICKS(3000)); // Analizza ogni 3 secondi
      newDataReady = false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}