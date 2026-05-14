#include <Wire.h>
#include <MAX30105.h>
#include "spo2_algorithm.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ================= PIN =================
#define SDA_PIN 4     // D2
#define SCL_PIN 5     // D1
#define BUZZER_PIN 13 // D7

// ================= BUFFER =================
#define BUFFER_SIZE 100

MAX30105 sensor;

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// ================= FILTER =================
float bpmFiltered = 0;
float spo2Filtered = 0;
float alpha = 0.3;

// ================= WIFI & MQTT =================
const char* ssid = "P502";
const char* password = "88888888";
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_user = "";
const char* mqtt_password = "";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ================= MQTT TOPICS =================
#define TOPIC_HEART_RATE "smart_watch/heart_rate"
#define TOPIC_SPO2 "smart_watch/spo2"
#define TOPIC_FINGER_STATUS "smart_watch/finger_status"
#define TOPIC_BUZZER_CONTROL "smart_watch/buzzer/control"
#define TOPIC_DEVICE_STATUS "smart_watch/status"

// ================= VARIABLES =================
bool buzzerManualMode = false;
bool fingerDetected = false;
bool warningTriggered = false;
unsigned long lastNoFingerPrint = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);

    if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("MAX30102 NOT FOUND");
        while (1);
    }

    Serial.println("MAX30102 READY");

    sensor.setup(0xFF, 8, 2, 400, 411, 4096);
    sensor.setPulseAmplitudeRed(0x1F);
    sensor.setPulseAmplitudeIR(0x1F);
    
    setupWiFi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);
    client.setSocketTimeout(1);
    
    Serial.println("Ready");
    Serial.println("=== SMART WATCH STARTED ===");
}

void setupWiFi() {
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(200);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
    } else {
        Serial.println("WiFi failed");
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == TOPIC_BUZZER_CONTROL) {
        if (message == "AUTO") {
            buzzerManualMode = false;
            digitalWrite(BUZZER_PIN, LOW);
            Serial.println("[MQTT] AUTO mode");
        } 
        else if (message == "ON") {
            buzzerManualMode = true;
            digitalWrite(BUZZER_PIN, HIGH);
            Serial.println("[MQTT] MANUAL ON");
        } 
        else if (message == "OFF") {
            buzzerManualMode = true;
            digitalWrite(BUZZER_PIN, LOW);
            Serial.println("[MQTT] MANUAL OFF");
        }
    }
}

void reconnectMQTT() {
    int retry = 0;
    while (!client.connected() && retry < 2) {
        String clientId = "ESP-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str())) {
            client.subscribe(TOPIC_BUZZER_CONTROL);
            client.publish(TOPIC_DEVICE_STATUS, "online");
        } else {
            retry++;
            delay(100);
        }
    }
}

bool readSensorData() {
    int samplesCollected = 0;
    
    for (int i = 0; i < BUFFER_SIZE; i++) {
        int timeout = 0;
        while (!sensor.available() && timeout < 20) {
            sensor.check();
            timeout++;
            if (timeout > 20) break;
        }
        
        if (sensor.available()) {
            redBuffer[i] = sensor.getRed();
            irBuffer[i] = sensor.getIR();
            sensor.nextSample();
            samplesCollected++;
        }
    }
    return (samplesCollected >= 50);
}

bool checkFinger() {
    long irSum = 0;
    int count = 0;
    for (int i = 0; i < BUFFER_SIZE; i += 5) {
        if (irBuffer[i] > 5000 && irBuffer[i] < 200000) {
            irSum += irBuffer[i];
            count++;
        }
    }
    if (count == 0) return false;
    
    long irAvg = irSum / count;
    return (irAvg >= 15000 && irAvg <= 180000);
}

void beepOnce() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
    // XỬ LÝ MQTT
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
    
    // ĐỌC CẢM BIẾN
    static unsigned long lastSensorRead = 0;
    
    if (millis() - lastSensorRead >= 1500) {
        lastSensorRead = millis();
        
        if (readSensorData()) {
            bool hasFinger = checkFinger();
            
            // Xử lý khi có/thay đổi ngón tay
            if (hasFinger != fingerDetected) {
                fingerDetected = hasFinger;
                if (fingerDetected) {
                    Serial.println(">>> FINGER DETECTED <<<");
                    client.publish(TOPIC_FINGER_STATUS, "detected");
                } else {
                    Serial.println(">>> NO FINGER <<<");
                    client.publish(TOPIC_FINGER_STATUS, "no_finger");
                    client.publish(TOPIC_HEART_RATE, "invalid");
                    client.publish(TOPIC_SPO2, "invalid");
                    warningTriggered = false;
                    bpmFiltered = 0;
                    spo2Filtered = 0;
                }
            }
            
            // HIỂN THỊ "NO FINGER" LIÊN TỤC
            if (!fingerDetected) {
                Serial.println(">>> NO FINGER <<<");
            }
            
            // Chỉ xử lý khi có ngón tay
            if (fingerDetected) {
                maxim_heart_rate_and_oxygen_saturation(
                    irBuffer, BUFFER_SIZE, redBuffer,
                    &spo2, &validSPO2, &heartRate, &validHeartRate
                );
                
                bool bpmOK = validHeartRate && heartRate >= 40 && heartRate <= 180;
                bool spo2OK = validSPO2 && spo2 >= 70 && spo2 <= 100;
                
                if (bpmOK && heartRate > 0) {
                    if (bpmFiltered == 0) bpmFiltered = heartRate;
                    else bpmFiltered = alpha * bpmFiltered + (1 - alpha) * heartRate;
                    client.publish(TOPIC_HEART_RATE, String((int)bpmFiltered).c_str());
                } else if (bpmFiltered > 0) {
                    client.publish(TOPIC_HEART_RATE, String((int)bpmFiltered).c_str());
                }
                
                if (spo2OK && spo2 > 0) {
                    if (spo2Filtered == 0) spo2Filtered = spo2;
                    else spo2Filtered = alpha * spo2Filtered + (1 - alpha) * spo2;
                    client.publish(TOPIC_SPO2, String((int)spo2Filtered).c_str());
                } else if (spo2Filtered > 0) {
                    client.publish(TOPIC_SPO2, String((int)spo2Filtered).c_str());
                }
                
                // LOGIC CÒI
                if (!buzzerManualMode && bpmFiltered > 0 && spo2Filtered > 0) {
                    bool needWarning = false;
                    String warnType = "";
                    
                    if (bpmFiltered < 55) {
                        needWarning = true;
                        warnType = "LOW BPM";
                    }
                    else if (bpmFiltered > 100) {
                        needWarning = true;
                        warnType = "HIGH BPM";
                    }
                    else if (spo2Filtered < 94) {
                        needWarning = true;
                        warnType = "LOW SPO2";
                    }
                    
                    if (needWarning && !warningTriggered) {
                        warningTriggered = true;
                        beepOnce();
                        Serial.print("!!! WARNING: ");
                        Serial.print(warnType);
                        Serial.print(" - BPM:");
                        Serial.print((int)bpmFiltered);
                        Serial.print(" SPO2:");
                        Serial.println((int)spo2Filtered);
                    }
                    else if (!needWarning && warningTriggered) {
                        warningTriggered = false;
                        Serial.println(">>> NORMAL <<<");
                    }
                }
                
                // In dữ liệu khi có ngón tay
                Serial.print("BPM:");
                Serial.print((int)bpmFiltered);
                Serial.print(" SPO2:");
                Serial.println((int)spo2Filtered);
            }
        }
    }
    
    delay(5);
}