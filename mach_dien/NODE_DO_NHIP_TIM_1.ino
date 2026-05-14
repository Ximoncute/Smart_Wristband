#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 sensor;

// ================= PIN =================
#define SDA_PIN 4     // D2
#define SCL_PIN 5     // D1
#define BUZZER_PIN 13 // D7

// ================= BUFFER =================
#define BUFFER_SIZE 50

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t validSPO2;

int32_t heartRate;
int8_t validHeartRate;

// ================= FILTER =================
float bpmFiltered = 0;
float spo2Filtered = 0;
float alpha = 0.75;   

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 NOT FOUND");
        while (1);
    }

    Serial.println("MAX30102 READY");

    sensor.setup(
        0xFF,  // LED brightness
        4,     // averaging
        2,     // Red + IR
        100,   // sample rate
        411,   // pulse width
        4096   // ADC range
    );

    sensor.setPulseAmplitudeRed(0x2F);
    sensor.setPulseAmplitudeIR(0x2F);
}

void loop() {

    // ================= COLLECT DATA =================
    for (int i = 0; i < BUFFER_SIZE; i++) {

        while (!sensor.available()) {
            sensor.check();
        }

        redBuffer[i] = sensor.getRed();
        irBuffer[i]  = sensor.getIR();

        sensor.nextSample();

        delay(10); 
    }

    // ================= CHECK FINGER (TRUNG BÌNH) =================
    long irSum = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        irSum += irBuffer[i];
    }

    if (irSum / BUFFER_SIZE < 30000) {
        Serial.println("NO FINGER DETECTED");
        digitalWrite(BUZZER_PIN, LOW);
        delay(1000);
        return;
    }

    // ================= CALCULATE =================
    maxim_heart_rate_and_oxygen_saturation(
        irBuffer,
        BUFFER_SIZE,
        redBuffer,
        &spo2,
        &validSPO2,
        &heartRate,
        &validHeartRate
    );

    // ================= VALIDATION =================
    bool bpmOK  = validHeartRate && heartRate > 40 && heartRate < 180;
    bool spo2OK = validSPO2 && spo2 > 70 && spo2 <= 100;

    // ================= FILTER (GIỐNG CODE CHUẨN) =================
    if (bpmOK) {
        bpmFiltered = alpha * bpmFiltered + (1 - alpha) * heartRate;
    }

    if (spo2OK) {
        spo2Filtered = alpha * spo2Filtered + (1 - alpha) * spo2;
    }

    // ================= PRINT =================
    Serial.print("BPM: ");
    if (bpmOK) Serial.print((int)bpmFiltered);
    else Serial.print("INVALID");

    Serial.print(" | SPO2: ");
    if (spo2OK) Serial.println((int)spo2Filtered);
    else Serial.println("INVALID");

    // ================= WARNING LOGIC =================
    bool warning = false;

    if (bpmOK && bpmFiltered < 50) warning = true;
    if (spo2OK && spo2Filtered < 92) warning = true;

    if (warning) {
        Serial.println("WARNING!");
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }

    delay(500);
}