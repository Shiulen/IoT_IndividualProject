#include <Arduino.h>

// Definizioni costanti
const int pwmPin = 3;
float t = 0;

// dt molto piccolo per avere una curva "densa" di punti
const float dt = 0.001; 

void setup() {
    Serial.begin(115200);
    pinMode(pwmPin, OUTPUT);
    
    // Portiamo la frequenza del PWM al massimo (31.4 kHz)
    // TCCR2B controlla il prescaler del Timer 2 (pin 3 e 11)
    TCCR2B = (TCCR2B & 0xF8) | 0x01; 
}

void loop() {
    // Calcolo della funzione somma: 2*sin(2*pi*3*t) + 4*sin(2*pi*2*t)
    // Ampiezza massima teorica: 2 + 4 = 6.0
    float signal = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 2.0 * t);

    // Mappatura 0-255 per il PWM
    // (signal + 6.0) porta il range da [-6, +6] a [0, 12]
    // Dividendo per 12.0 normalizziamo tra 0 e 1
    float normalized = (signal + 6.0) / 12.0; 
    int pwmValue = (int)(normalized * 255.0);

    // Clipping di sicurezza per evitare overflow
    if(pwmValue > 255) pwmValue = 255;
    if(pwmValue < 0) pwmValue = 0;

    analogWrite(pwmPin, pwmValue);

    // Invio al Serial Plotter ogni 5 cicli per non saturare il buffer
    static int skip = 0;
    if(skip++ % 5 == 0) {
        Serial.println(signal);
    }

    t += dt;
    
    // Ritardo per stabilizzare il campionamento
    delayMicroseconds(500); 
}