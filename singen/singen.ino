/**
 * @file esp32_dac_sine_generator_fixed_v5.ino
 * @brief ESP32 Sine Wave Generator using DAC and Timer Interrupts (API Fixed v5 - Adjusted timerBegin signature)
 * @details Generates a sine wave on a DAC pin (GPIO25 or GPIO26)
 * with controllable frequency. The output is biased around VCC/2
 * (approx 1.65V) with a 1V peak-to-peak amplitude.
 * An external AC coupling capacitor is REQUIRED to shift the output
 * to +/- 0.5V centered around 0V.
 * Uses timerBegin(frequency), 2-argument timerAttachInterrupt, 4-argument timerAlarm.
 *
 * Hardware:
 * - ESP32 development board
 * - Connect DAC_PIN (GPIO25 or GPIO26) through a capacitor (e.g., 1uF - 10uF)
 * to the input of the circuit you want to test (e.g., LM2907 TACH+ pin).
 * - Connect ESP32 GND to the test circuit's GND.
 *
 * Control:
 * - Open Serial Monitor (baud rate 115200).
 * - Type a frequency in Hz (e.g., "100", "500") and press Enter to set the output frequency.
 */

#include <Arduino.h>
#include <math.h>

// --- Configuration ---
#define DAC_PIN 25       // Use GPIO25 for DAC output (DAC_CHANNEL_1)
// #define DAC_PIN 26    // Or use GPIO26 for DAC output (DAC_CHANNEL_2)

const int LUT_SIZE = 256; // Number of points in the sine wave lookup table (power of 2 is good)
const float V_REF = 3.3;  // Approximate DAC reference voltage for ESP32
const float V_OUT_PEAK_TO_PEAK = 1.0; // Desired Peak-to-Peak output voltage (for +/- 0.5V)
const float V_OUT_OFFSET = V_REF / 2.0; // Center the output at VCC/2 (approx 1.65V)

// --- Global Variables ---
byte sineLUT[LUT_SIZE];          // Lookup table for sine wave DAC values (0-255)
volatile int lutIndex = 0;       // Current index in the LUT (volatile for ISR access)
hw_timer_t *timer = NULL;        // Hardware timer object
volatile float targetFrequency = 100.0; // Initial target frequency in Hz
volatile uint64_t updatePeriodMicros = 0; // Timer alarm period in microseconds
volatile bool updateFrequency = false;  // Flag to signal frequency update needed

// --- Interrupt Service Routine (ISR) ---
void IRAM_ATTR onTimer() {
  dacWrite(DAC_PIN, sineLUT[lutIndex]);
  lutIndex++;
  if (lutIndex >= LUT_SIZE) {
    lutIndex = 0;
  }
}
// check this rule
// --- Helper Function: Generate Sine Lookup Table ---
void generateSineLUT() {
  int offset_dac = round((V_OUT_OFFSET / V_REF) * 255.0);
  int amplitude_dac = round(((V_OUT_PEAK_TO_PEAK / 2.0) / V_REF) * 255.0);
  Serial.printf("Generating LUT: Offset=%d, Amplitude=%d\n", offset_dac, amplitude_dac);
  for (int i = 0; i < LUT_SIZE; i++) {
    float sine_value = sin(2.0 * PI * (float)i / (float)LUT_SIZE);
    int dac_value = offset_dac + round(amplitude_dac * sine_value);
    if (dac_value < 0) dac_value = 0;
    else if (dac_value > 255) dac_value = 255;
    sineLUT[i] = (byte)dac_value;
  }
  Serial.println("LUT generation complete.");
}

// --- Helper Function: Calculate Timer Period ---
// Assumes timer clock is 1MHz (set via timerBegin(1000000))
uint64_t calculateUpdatePeriod(float freq) {
  if (freq <= 0 || LUT_SIZE <= 0) {
    return 0;
  }
  // Period (microseconds) = 1,000,000 / (freq * LUT_SIZE)
  uint64_t period = (uint64_t)(1000000.0 / (freq * (float)LUT_SIZE));
  return (period > 0) ? period : 1; // Return 1us minimum
}

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 DAC Sine Wave Generator (API Fixed v5)");
  Serial.printf("Output on GPIO %d\n", DAC_PIN);
  Serial.println("Enter target frequency (Hz) in Serial Monitor.");

  generateSineLUT();

  // Configure Timer
  // SỬA LỖI: Gọi timerBegin với 1 tham số là tần số clock mong muốn cho timer (1MHz = 1,000,000 Hz)
  // Theo đúng khai báo mà trình biên dịch báo lỗi: hw_timer_t *timerBegin(uint32_t frequency);
  // Hàm này có thể sẽ tự động chọn timer và tính prescaler (giả định là 80 từ 80MHz APB)
  timer = timerBegin(1000000); // <--- THAY ĐỔI CHÍNH Ở ĐÂY
  if (!timer) {
    Serial.println("Failed to initialize timer handle!");
    while(1);
  }

  // Attach the ISR function (Dùng 2 tham số)
  timerAttachInterrupt(timer, &onTimer);

  // Calculate initial timer period
  updatePeriodMicros = calculateUpdatePeriod(targetFrequency);
  if (updatePeriodMicros > 0) {
    Serial.printf("Setting initial frequency: %.2f Hz, Update Period: %llu us\n", targetFrequency, updatePeriodMicros);
    // Set the timer alarm period, enable auto-reload, set initial count = 0
    // Dùng timerAlarm với 4 tham số
    timerAlarm(timer, updatePeriodMicros, true, 0);
  } else {
     Serial.println("Initial frequency is zero or invalid. Timer interrupt not attached initially.");
     timerDetachInterrupt(timer);
  }
}

// --- Loop Function ---
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    float newFreq = input.toFloat();

    if (newFreq >= 0) {
      targetFrequency = newFreq;
      updateFrequency = true;
      Serial.printf("Received new frequency: %.2f Hz\n", targetFrequency);
    } else {
      Serial.println("Invalid frequency entered.");
    }
  }

  if (updateFrequency) {
    timerDetachInterrupt(timer); // Detach ISR safely

    updatePeriodMicros = calculateUpdatePeriod(targetFrequency);

    if (updatePeriodMicros > 0) {
       Serial.printf("Updating timer period to: %llu us\n", updatePeriodMicros);
       // Dùng timerAlarm với 4 tham số
       timerAlarm(timer, updatePeriodMicros, true, 0);
       // Re-attach the interrupt only if the timer should be running
       timerAttachInterrupt(timer, &onTimer);
    } else {
      Serial.println("Frequency set to zero or invalid. Timer interrupt detached.");
      dacWrite(DAC_PIN, round((V_OUT_OFFSET / V_REF) * 255.0));
    }
    updateFrequency = false;
  }

  delay(100);
}