#include <Arduino.h>
#include <math.h>

#define DAC_PIN 25         // GPIO25 (DAC1)
#define LUT_SIZE 64        // Số điểm trong 1 chu kỳ sóng sin
#define FREQ_HZ 100        // Tần số mong muốn (Hz)

byte sineLUT[LUT_SIZE];    // Bảng tra sóng sin

void generateSineLUT() {
  for (int i = 0; i < LUT_SIZE; i++) {
    float angle = 2.0 * PI * i / LUT_SIZE;
    float sine_val = (sin(angle) + 1.0) / 2.0;  // Biến đổi sin(x) từ [-1,1] -> [0,1]
    sineLUT[i] = round(sine_val * 255.0);       // Đưa về giá trị DAC (0-255)
  }
}

void setup() {
  Serial.begin(115200);
  generateSineLUT();
  Serial.println("Sine wave generator started...");
}

void loop() {
  static int index = 0;

  dacWrite(DAC_PIN, sineLUT[index]);      // Ghi giá trị ra DAC
  index = (index + 1) % LUT_SIZE;

  // Tính toán delay giữa các mẫu: delay = 1 / (freq * LUT_SIZE)
  // Ví dụ: 100Hz * 64 mẫu = 6400 mẫu/giây -> mỗi mẫu cách nhau ~156us
  delayMicroseconds(1000000 / (FREQ_HZ * LUT_SIZE));
}
