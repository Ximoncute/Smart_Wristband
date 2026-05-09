#include <Wire.h>
#include <DFRobot_BMI160.h>

DFRobot_BMI160 bmi160;

// ================= UART =================
HardwareSerial mySerial(1);

// ===== UART PIN =====
#define RXD1 16
#define TXD1 17

int16_t accelGyro[6];

// ===== OFFSET =====
float offsetX = 0;
float offsetY = 0;
float offsetZ = 0;

String uartData = "";

void calibrateSensor()
{
    long sumX = 0;
    long sumY = 0;
    long sumZ = 0;

    Serial.println("CALIBRATING...");

    for (int i = 0; i < 500; i++)
    {
        bmi160.getAccelGyroData(accelGyro);

        sumX += accelGyro[3];
        sumY += accelGyro[4];
        sumZ += accelGyro[5];

        delay(5);
    }

    offsetX = sumX / 500.0;
    offsetY = sumY / 500.0;

    // Remove gravity
    offsetZ = (sumZ / 500.0) - 16384;

    Serial.println("CALIBRATION DONE");
}

void setup()
{
    Serial.begin(115200);

    // ===== UART =====
    mySerial.begin(115200, SERIAL_8N1, RXD1, TXD1);

    // ===== I2C =====
    Wire.begin(21, 22);

    // ===== INIT BMI160 =====
    if (bmi160.I2cInit(0x69) != BMI160_OK)
    {
        Serial.println("BMI160 FAIL");

        while (1);
    }

    Serial.println("BMI160 READY");

    calibrateSensor();
}

void loop()
{
    // ===== READ BMI160 =====
    bmi160.getAccelGyroData(accelGyro);

    float ax = (accelGyro[3] - offsetX) / 16384.0;
    float ay = (accelGyro[4] - offsetY) / 16384.0;
    float az = (accelGyro[5] - offsetZ) / 16384.0;

    // ===== FILTER SMALL NOISE =====
    if (abs(ax) < 0.02) ax = 0;
    if (abs(ay) < 0.02) ay = 0;
    if (abs(az) < 0.02) az = 0;

    // ===== RECEIVE UART =====
    if (mySerial.available())
    {
        uartData = mySerial.readStringUntil('\n');

        Serial.print("AX: ");
        Serial.print(ax, 3);

        Serial.print(" | AY: ");
        Serial.print(ay, 3);

        Serial.print(" | AZ: ");
        Serial.print(az, 3);

        Serial.print(" || ");

        // ===== UART MESSAGE =====
        if (uartData == "NO_FINGER")
        {
            Serial.println("NO FINGER DETECTED");
        }
        else if (uartData == "INVALID")
        {
            Serial.println("INVALID BPM/SPO2");
        }
        else
        {
            Serial.print("RECEIVED BPM/SPO2: ");
            Serial.println(uartData);
        }
    }

    delay(2000);
}