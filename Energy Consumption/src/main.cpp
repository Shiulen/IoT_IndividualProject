#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

void setup(void) 
{
  Serial.begin(115200);
  while (!Serial) {
      // will pause Zero, Leonardo, etc until serial console opens
      delay(1);
  }

  uint32_t currentFrequency;

  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();
  Serial.println("Measuring voltage and current with INA219 ...");
}

void loop(void) 
{  // 1. Leggiamo i dati dal sensore
  float busvoltage_V = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  float power_mW = ina219.getPower_mW();
  
  // Opzionale: calcolo della tensione sul carico
  // float loadvoltage_V = busvoltage_V + (ina219.getShuntVoltage_mV() / 1000.0);

  // 2. Stampiamo su Teleplot in modo a prova di bomba!
  // Costruiamo la stringa un pezzo alla volta senza usare printf
  
  Serial.print('>');
  Serial.printf("BusVoltage: %f", busvoltage_V);
  Serial.print(',');
  Serial.printf("Current: %f", current_mA);
  Serial.print(',');
  Serial.printf("Power: %f", power_mW);
  Serial.println();

  // Pausa di 100ms per fare grafici puliti e non intasare la seriale
  delay(100);
}
