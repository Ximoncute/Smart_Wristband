#include <Wire.h>
#include <BMI160Gen.h>
#include <vong_tay_thong_minh_inferencing.h>

// =====================================================
// I2C BMI160
// =====================================================

#define SDA_PIN 8
#define SCL_PIN 9

// =====================================================
// LED
// =====================================================

#define LED_PIN 5

// =====================================================
// AI BUFFER
// =====================================================

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static size_t feature_ix = 0;

// =====================================================
// AI SCORE
// =====================================================

float fallScore = 0.0;
float idleScore = 0.0;

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(
    LED_PIN,
    OUTPUT
  );

  digitalWrite(
    LED_PIN,
    LOW
  );

  // =====================================================
  // I2C
  // =====================================================

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  Wire.setClock(400000);

  // =====================================================
  // BMI160
  // =====================================================

  if (!BMI160.begin(
        BMI160GenClass::I2C_MODE,
        Wire,
        0x69
      )) {

    Serial.println("BMI160 FAIL");

    while (1);
  }

  Serial.println("BMI160 OK");

  // =====================================================
  // CONFIG
  // =====================================================

  BMI160.setAccelerometerRate(50);

  BMI160.setGyroRate(50);

  BMI160.setAccelerometerRange(2);

  BMI160.setGyroRange(250);

  Serial.println("AI READY");
}

// =====================================================
// RUN AI
// =====================================================

void runAI() {

  int axRaw, ayRaw, azRaw;

  int gxRaw, gyRaw, gzRaw;

  // =====================================================
  // READ BMI160
  // =====================================================

  BMI160.readAccelerometer(
    axRaw,
    ayRaw,
    azRaw
  );

  BMI160.readGyro(
    gxRaw,
    gyRaw,
    gzRaw
  );

  // =====================================================
  // CONVERT
  // =====================================================

  features[feature_ix++] =
    ((float)axRaw / 16384.0f) * 9.80665f;

  features[feature_ix++] =
    ((float)ayRaw / 16384.0f) * 9.80665f;

  features[feature_ix++] =
    ((float)azRaw / 16384.0f) * 9.80665f;

  features[feature_ix++] =
    ((float)gxRaw / 131.0f) * 0.0174533f;

  features[feature_ix++] =
    ((float)gyRaw / 131.0f) * 0.0174533f;

  features[feature_ix++] =
    ((float)gzRaw / 131.0f) * 0.0174533f;

  // =====================================================
  // FULL BUFFER
  // =====================================================

  if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {

    feature_ix = 0;

    signal_t signal;

    numpy::signal_from_buffer(
      features,
      EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
      &signal
    );

    ei_impulse_result_t result;

    EI_IMPULSE_ERROR res;

    res = run_classifier(
      &signal,
      &result,
      false
    );

    if (res != EI_IMPULSE_OK) {

      Serial.println("AI ERROR");

      return;
    }

    // =====================================================
    // GET SCORE
    // =====================================================

    for (size_t i = 0;
         i < EI_CLASSIFIER_LABEL_COUNT;
         i++) {

      String label =
        result.classification[i].label;

      float value =
        result.classification[i].value;

      if (label == "fall") {

        fallScore = value;
      }

      else if (label == "idle") {

        idleScore = value;
      }
    }

    // =====================================================
    // RESULT
    // =====================================================

    if (fallScore > 0.2) {

      digitalWrite(
        LED_PIN,
        HIGH
      );

      Serial.print("FALL : ");

      Serial.print(fallScore, 2);

      Serial.print(" IDLE : ");

      Serial.print(idleScore, 2);

      Serial.println(" -> FALL");
    }
    else {

      digitalWrite(
        LED_PIN,
        LOW
      );

      Serial.print("FALL : ");

      Serial.print(fallScore, 2);

      Serial.print(" IDLE : ");

      Serial.print(idleScore, 2);

      Serial.println(" -> IDLE");
    }
  }
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  static unsigned long lastRun = 0;

  if (millis() - lastRun >= 20) {

    lastRun = millis();

    runAI();
  }
}