#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

// ================= UART =================
HardwareSerial mySerial(1);

// ================= PIN =================
#define SDA_PIN 8
#define SCL_PIN 9

#define LED_PIN 3

#define RXD1 20
#define TXD1 21

// ================= BUFFER =================
#define BUFFER_SIZE 50

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t validSPO2;

int32_t heartRate;
int8_t validHeartRate;

void setup()
{
    Serial.begin(115200);

    mySerial.begin(115200, SERIAL_8N1, RXD1, TXD1);

    pinMode(LED_PIN, OUTPUT);

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
    {
        Serial.println("MAX30102 NOT FOUND");

        while (1);
    }

    Serial.println("MAX30102 READY");

    // ===== BEST STABILITY CONFIG =====
    particleSensor.setup(
        60,   // brightness
        4,    // averaging
        2,    // RED + IR
        100,  // sample rate
        411,  // pulse width
        4096
    );

    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);

    Serial.println("PLACE FINGER");
}

void loop()
{
    // ===== READ SENSOR =====
    for (byte i = 0; i < BUFFER_SIZE; i++)
    {
        while (!particleSensor.available())
        {
            particleSensor.check();
        }

        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();

        particleSensor.nextSample();
    }

    // ===== CHECK FINGER =====
    if (irBuffer[BUFFER_SIZE - 1] < 50000)
    {
        Serial.println("NO FINGER DETECTED");

        mySerial.println("NO_FINGER");

        digitalWrite(LED_PIN, LOW);

        delay(2000);

        return;
    }

    // ===== CALCULATE =====
    maxim_heart_rate_and_oxygen_saturation(
        irBuffer,
        BUFFER_SIZE,
        redBuffer,
        &spo2,
        &validSPO2,
        &heartRate,
        &validHeartRate
    );

    // ===== FILTER INVALID =====
    bool bpmOK = false;
    bool spo2OK = false;

    if (validHeartRate &&
        heartRate >= 40 &&
        heartRate <= 180)
    {
        bpmOK = true;
    }

    if (validSPO2 &&
        spo2 >= 70 &&
        spo2 <= 100)
    {
        spo2OK = true;
    }

    // ===== DISPLAY =====
    Serial.print("BPM: ");

    if (bpmOK)
    {
        Serial.print(heartRate);
    }
    else
    {
        Serial.print("UNABLE TO MEASURE");
    }

    Serial.print(" | SPO2: ");

    if (spo2OK)
    {
        Serial.println(spo2);
    }
    else
    {
        Serial.println("UNABLE TO MEASURE");
    }

    // ===== UART SEND =====
    if (bpmOK && spo2OK)
    {
        mySerial.print(heartRate);
        mySerial.print(",");
        mySerial.println(spo2);

        Serial.println("UART SENT");
    }
    else
    {
        mySerial.println("UNABLE_TO_MEASURE");
    }

    // ===== WARNING LED =====
    bool warning = false;

    if (bpmOK && heartRate < 45)
    {
        warning = true;
    }

    if (spo2OK && spo2 < 90)
    {
        warning = true;
    }

    if (warning)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
    }

    delay(2000);
}