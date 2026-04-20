# IoT_IndividualProject
The goal of the assignment is to create an IoT system that collects information from a sensor, analyses the data locally and communicates to a nearby server an aggregated value of the sensor readings. The IoT system adapts the sampling frequency in order to save energy and reduce communication overhead. The IoT device will be based on an ESP32 prototype board and the firmware will be developed using the FreeRTOS. You are free to use IoT-Lab or real devices.

## The assignment
The requirements needed are:

- Identify the maximum sampling frequency of the device
- Compute correctly the max freq of the input signal
- Compute correctly the optimal freq of the input signal dynamically
- Compute correctly the aggregate function over a window
- Evaluate correctly the saving in energy and bandwidth
- Evaluate correctly the communication cost
- Transmit the result to the edge server via MQTT+WIFI 
- Transmit the result to the cloud server via LoRaWAN + TTN

## Technical Details

The project relies on a **FreeRTOS task-based architecture** running on an ESP32 (Heltec WiFi LoRa 32 V3). To ensure smooth compilation and avoid linker errors with hardware libraries, the code uses a "Unity Build" pattern, where tasks are separated in `.hpp` files and included at the bottom of the `main.cpp`.

### Input Signal
Instead of a simulated sine wave, the input signal is acquired physically from the ADC of the ESP32 board using `analogRead()` on GPIO 1, with a 12-bit resolution (values from 0 to 4095).

### Maximum Sampling Frequency
The Maximum Sampling Frequency of the ESP32 ADC can be very high, but to maintain a stable and realistic IoT application, a theoretical maximum boundary of `1000 Hz` is set (`MAX_SAMPLING_FREQ`). 
The sampling rate is precisely controlled using FreeRTOS delays:

```cpp
vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS((int)(1000.0 / currentFreq)));
```
This guarantees that the task wakes up exactly at the required intervals, compensating for the execution time of the analogRead() instruction.

### Identify optimal Sampling Frequency (Adaptive FFT)
To identify the maximum frequency of the input signal and adapt the sampling rate, a True Double-Buffering mechanism is implemented.
The TaskSampling fills a 128-sample buffer (procReal and procImag). Once filled, the pointers are swapped instantly with a secondary buffer (fftReal and fftImag), and a binary semaphore (xFFTReady) triggers the TaskFFT. This ensures no data is lost while the FFT is being computed.

The ArduinoFFT library calculates the magnitudes. We isolate the highest significant frequency bin (skipping the DC component) using a magnitude threshold of 500.0:
```cpp
double f_max = (topBin * currentFreq) / (double)SAMPLES;
```

Based on the Nyquist-Shannon Sampling Theorem, the optimal sampling frequency must be at least twice the maximum frequency of the signal. To ensure a safety margin and a good reconstruction, we set:
```cpp
float recommendedFreq = f_max * 2.5;
```
The new frequency is safely shared back to the sampling task using a Mutex (freqMutex).


### Compute aggregate function over a window
The aggregate function calculates the average of the ADC values over a tumbling window.
In the TaskSampling, every read value is added to a global accumulator (windowSum) protected by windowMutex.
Every 30 seconds (the minimum delay enforced to respect Duty Cycle), the communication task locks the mutex, extracts the sum and the sample count, computes the average, and resets the variables for the next window:
```cpp
float average = windowSum / windowCount;
```

### Communicate the aggregate value to the cloud (LoRaWAN)
By setting the global flag useLoraMode = true, the system activates TaskLora.
Using the heltec_unofficial and RadioLib libraries, the board connects to The Things Network (TTN) via OTAA (Over-The-Air Activation).
To strictly minimize bandwidth and airtime, the floating-point average is cast to an unsigned 16-bit integer and sent as a minimal 2-byte payload:
```cpp
uint16_t valToSend = (uint16_t)average;
uint8_t payload[2] = { highByte(valToSend), lowByte(valToSend) };
node.sendReceive(payload, 2, 1);
```

The task respects the LoRaWAN Fair Use Policy by enforcing a MINIMUM_DELAY of 30 seconds between each transmission.

### Communicate the aggregate value to the nearby server (MQTT)
If the LoRa coverage is absent or local testing is required, setting useLoraMode = false skips the LoRa initialization and starts TaskMQTT instead.
The board connects to a local WiFi hotspot and uses PubSubClient to publish a JSON or formatted string payload to the MQTT broker on the topic esp32/average.

### On-Device UI (OLED Display)
A dedicated low-priority task (TaskDisplay) visualizes the data in real-time. It plots the raw ADC signal acting as a mini-oscilloscope, while the top header displays the currently applied sampling frequency and the system state (e.g., "Join TTN in corso...", "TX Confermato OK!").

## Performance of the system
### Energy Consumption
Instead of using Deep Sleep (which would clear the RAM and reset the FreeRTOS scheduler), energy efficiency is achieved through the RTOS Idle Task. By using vTaskDelayUntil() and semaphores (portMAX_DELAY), the CPU is yielded to the Idle state whenever tasks are waiting for data or timeouts.

### Communication cost & Bandwidth Savings
The Adaptive Sampling logic provides massive bandwidth savings compared to sending raw data.
Instead of sending over-sampled data (e.g., 1000 Hz * 30 seconds = 30,000 samples per window), the system processes the signal locally (Edge Computing).
Furthermore, in LoRaWAN mode, the payload is compressed into exactly 2 Bytes. This drastically reduces the Time-on-Air (ToA) of the LoRa modulation, minimizing packet collisions and drastically lowering the radio power consumption.

### End-to-end Latency of the system
The internal latency (Generation Latency) from the first sample to the payload transmission is tracked using micros(). The FFT computation (fftDurationUs) takes only a few milliseconds, ensuring the system can easily sustain the maximum 1000 Hz sampling rate without bottlenecks.

## Setup Guide
### Hardware Requirements
1. Heltec WiFi LoRa 32 V3 Development Board.

2. Analog sensor or Potentiometer connected to GPIO 1.

3. Antenna connected to the IPEX connector (CRITICAL for LoRa transmission).

### Hardware Requirements
1. PlatformIO IDE

2. Libraries required in platformio.ini:

    - kosme/arduinoFFT @ ^2.0.4
    - ropg/Heltec_ESP32_LoRa_v3 @ ^0.9.2
    - jgromes/RadioLib
    - ropg/LoRaWAN_ESP32
    - knolleary/PubSubClient @ ^2.8


### Execution Steps
1. Clone the repository.

2. Configure your TTN credentials (deveui, appeui, appkey) in main.cpp.

3. Choose the communication mode:
    - Set bool useLoraMode = true; to use TTN.

    - Set bool useLoraMode = false; to use WiFi/MQTT (remember to update ssid and mqtt_server).

4. Build and upload the code using PlatformIO.

5. Open the Serial Monitor at 115200 baud to see the RTOS task execution.