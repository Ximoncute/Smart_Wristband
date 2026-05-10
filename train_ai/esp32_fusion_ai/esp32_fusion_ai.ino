#include <vong_tay_thong_minh_inferencing.h>
#include <BMI160Gen.h>
#include <Wire.h>

#define BUZZER_PIN 15

// Khai báo đối tượng Wire với chân I2C
TwoWire I2C_BMI = TwoWire(0);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Khởi tạo I2C với chân 21 (SDA), 22 (SCL)
  I2C_BMI.begin(21, 22, 400000);
  
  // Khởi tạo BMI160 với địa chỉ 0x69 - CÁCH ĐÚNG
  // LƯU Ý: tham số thứ 2 là object TwoWire, tham số thứ 3 là địa chỉ I2C
  if (!BMI160.begin(BMI160GenClass::I2C_MODE, I2C_BMI, 0x69)) {
    Serial.println("BMI160 init failed! Check wiring and address 0x69");
    while (1);
  }
  
  // Cấu hình tần số lấy mẫu 50 Hz
  BMI160.setAccelerometerRate(50);
  BMI160.setGyroRate(50);
  BMI160.setAccelerometerRange(2);  // ±2g
  BMI160.setGyroRange(250);         // ±250 dps

  Serial.println("BMI160 OK - Ready");
}

void loop() {
  int axRaw, ayRaw, azRaw;
  int gxRaw, gyRaw, gzRaw;

  // Đọc dữ liệu - DÙNG readGyro (không phải readGyroscope)
  BMI160.readAccelerometer(axRaw, ayRaw, azRaw);
  BMI160.readGyro(gxRaw, gyRaw, gzRaw);  // ← sửa thành readGyro

  // Chuyển đổi sang m/s² cho gia tốc
  float ax = (float)axRaw / 16384.0 * 9.80665;
  float ay = (float)ayRaw / 16384.0 * 9.80665;
  float az = (float)azRaw / 16384.0 * 9.80665;
  
  // Chuyển đổi sang rad/s cho gyro
  float gx = (float)gxRaw / 131.0 * 0.0174533;
  float gy = (float)gyRaw / 131.0 * 0.0174533;
  float gz = (float)gzRaw / 131.0 * 0.0174533;

  // Buffer cho Edge Impulse
  static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
  static size_t sample_index = 0;

  buffer[sample_index]     = ax;
  buffer[sample_index + 1] = ay;
  buffer[sample_index + 2] = az;
  buffer[sample_index + 3] = gx;
  buffer[sample_index + 4] = gy;
  buffer[sample_index + 5] = gz;

  sample_index += 6;

  if (sample_index >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
    sample_index = 0;

    signal_t signal;
    numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    
    ei_impulse_result_t result;
    run_classifier(&signal, &result, false);

    float fall_score = 0;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      Serial.print(result.classification[i].label);
      Serial.print(": ");
      Serial.println(result.classification[i].value);
      
      if (strcmp(result.classification[i].label, "fall") == 0) {
        fall_score = result.classification[i].value;
      }
    }

    if (fall_score > 0.7) {
      Serial.println("!!! FALL DETECTED !!!");
      digitalWrite(BUZZER_PIN, HIGH);
      delay(2000);
      digitalWrite(BUZZER_PIN, LOW);
    }
    Serial.println("---");
  }
  
  delay(10);
}