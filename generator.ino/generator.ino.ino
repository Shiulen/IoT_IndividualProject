const int pwmPin = 3;
float t = 0;

// dt molto piccolo per avere una curva "densa" di punti
// 0.001 corrisponde a una risoluzione altissima per 3Hz e 5Hz
const float dt = 0.001; 

void setup() {
  Serial.begin(115200);
  pinMode(pwmPin, OUTPUT);
  
  // Portiamo la frequenza del PWM al massimo (31.4 kHz)
  // Questo rende il filtro RC molto più efficace nel "lisciare" l'onda
  TCCR2B = (TCCR2B & 0xF8) | 0x01; 
}

void loop() {
  // Calcolo della funzione somma
  float signal = 2.0 * sin(2.0 * PI * 3.0 * t) + 4.0 * sin(2.0 * PI * 2.0 * t);

  // Mappatura 0-255 per il PWM
  // Usiamo float per non perdere precisione nei passaggi intermedi
  float normalized = (signal + 6.0) / 12.0; 
  int pwmValue = (int)(normalized * 255.0);

  // Clipping di sicurezza
  if(pwmValue > 255) pwmValue = 255;
  if(pwmValue < 0) pwmValue = 0;

  analogWrite(pwmPin, pwmValue);

  // Invio al Serial Plotter per controllo
  // Usiamo un filtro software per non intasare la seriale
  static int skip = 0;
  if(skip++ % 5 == 0) {
    Serial.println(signal);
  }

  t += dt;
  
  // Un micro-ritardo per dare stabilità, ma molto più veloce di prima
  delayMicroseconds(500); 
}