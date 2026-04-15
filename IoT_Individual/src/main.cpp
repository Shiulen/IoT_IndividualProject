#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configurazione Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RST 21 
#define OLED_SDA 17
#define OLED_SCL 18
#define VEXT_PIN 19

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// Variabili globali per la comunicazione tra Task
volatile int sharedRawVal = 2048; // Valore ADC condiviso
const int sensorPin = 1;         // GPIO 1

// Prototipi dei Task
void TaskSampling(void *pvParameters);
void TaskDisplay(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Inizializzazione Hardware
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 failed"));
    for(;;);
  }
  display.clearDisplay();
  analogReadResolution(12);
  
  xTaskCreatePinnedToCore(
    TaskSampling,   // Funzione che legge l'ADC
    "Sampling",     // Nome
    2048,           // Stack size
    NULL,           // Parametri
    3,              // Priorità alta (fondamentale per precisione temporale)
    NULL,           // Handle
    1               // Core 1
  );

  xTaskCreatePinnedToCore(
    TaskDisplay,    // Funzione che disegna
    "Display",      // Nome
    4096,           // Stack size (più grande per GFX)
    NULL,           // Parametri
    1,              // Priorità bassa
    NULL,           // Handle
    0               // Core 0 (lasciamo il core 0 per WiFi/BT e grafica)
  );
}

void loop() {
  // Il loop rimane vuoto in FreeRTOS
}

// --- TASK 1: CAMPIONAMENTO PRECISO ---
void TaskSampling(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms = 100Hz

  for (;;) {
    // Leggi il segnale
    sharedRawVal = analogRead(sensorPin);
    
    // Attendi esattamente il prossimo ciclo
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// --- TASK 2: VISUALIZZAZIONE ---
void TaskDisplay(void *pvParameters) {
  static int x = 0;
  static int lastY = 32;

  for (;;) {
    int currentVal = sharedRawVal; // Copia locale del valore globale

    // Zoom verticale (modifica 0, 4095 se vuoi zoomare)
    int y = map(currentVal, 0, 4095, 63, 0);

    // Disegno
    display.drawLine(x - 1, lastY, x, y, SSD1306_WHITE);
    
    // Refresh ogni N pixel per non rallentare troppo il task
    if (x % 2 == 0) {
        display.display();
    }

    lastY = y;
    x++;

    if (x >= 128) {
      x = 0;
      display.clearDisplay();
    }

    // Piccola pausa per lasciare respiro al sistema
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}
