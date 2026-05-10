#include <Wire.h>
#include <DFRobot_BMI160.h>

DFRobot_BMI160 bmi160;

int16_t accelGyro[6];

// ===== OFFSET =====
float offsetX = 0;
float offsetY = 0;
float offsetZ = 0;

// ===== TIMESTAMP =====
unsigned long lastTime = 0;

void calibrateSensor()
{
    long sumX = 0;
    long sumY = 0;
    long sumZ = 0;

    Serial.println("CALIBRATING... KEEP SENSOR STILL");

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
    offsetZ = (sumZ / 500.0) - 16384;

    Serial.println("CALIBRATION DONE");
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(21, 22);

    delay(1000);

    if (bmi160.I2cInit(0x69) != BMI160_OK)
    {
        Serial.println("BMI160 FAIL");
        while (1);
    }

    Serial.println("BMI160 READY");

    calibrateSensor();

    // ===== CSV HEADER (Edge Impulse REQUIRE) =====
    Serial.println("timestamp,ax,ay,az,gx,gy,gz,label");

    lastTime = millis();
}

void loop()
{
    bmi160.getAccelGyroData(accelGyro);

    // ===== ACCEL =====
    float ax = (accelGyro[3] - offsetX) / 16384.0;
    float ay = (accelGyro[4] - offsetY) / 16384.0;
    float az = (accelGyro[5] - offsetZ) / 16384.0;

    // ===== GYRO =====
    float gx = accelGyro[0] / 131.0;
    float gy = accelGyro[1] / 131.0;
    float gz = accelGyro[2] / 131.0;

    // ===== NOISE FILTER =====
    if (abs(ax) < 0.02) ax = 0;
    if (abs(ay) < 0.02) ay = 0;
    if (abs(az) < 0.02) az = 0;

    // ===== TIMESTAMP =====
    unsigned long timestamp = millis() - lastTime;

    // ===== CSV OUTPUT =====
    Serial.print(timestamp);
    Serial.print(",");

    Serial.print(ax, 4);
    Serial.print(",");
    Serial.print(ay, 4);
    Serial.print(",");
    Serial.print(az, 4);
    Serial.print(",");

    Serial.print(gx, 4);
    Serial.print(",");
    Serial.print(gy, 4);
    Serial.print(",");

    Serial.println(gz, 4);


    delay(20); // ~50Hz
}