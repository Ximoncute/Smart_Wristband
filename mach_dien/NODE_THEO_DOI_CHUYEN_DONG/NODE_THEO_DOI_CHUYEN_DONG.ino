#include <Wire.h>
#include <BMI160Gen.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <vong_tay_thong_minh_inferencing.h>

// ================= I2C BMI160 =================
#define SDA_PIN 8
#define SCL_PIN 9

// ================= LED =================
#define LED_PIN 5

// ================= WIFI & MQTT =================
const char* ssid = "P502";
const char* password = "88888888";
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_user = "";
const char* mqtt_password = "";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ================= AI BUFFER =================
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static size_t feature_ix = 0;

// ================= AI SCORE =================
float fallScore = 0.0;
float idleScore = 0.0;

// ================= MQTT TOPICS =================
#define TOPIC_FALL_STATUS "smart_watch/fall_status"
#define TOPIC_FALL_SCORE "smart_watch/fall_score"
#define TOPIC_LED_CONTROL "smart_watch/led/control"
#define TOPIC_DEVICE_STATUS_ESP32 "smart_watch/esp32/status"

// ================= VARIABLES =================
bool ledManualMode = false;
bool ledState = false;
unsigned long lastSendTime = 0;
unsigned long lastFallTime = 0;
bool fallDetected = false;
unsigned long lastAIrun = 0;

void setup() {
    Serial.begin(115200);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);

    if (!BMI160.begin(BMI160GenClass::I2C_MODE, Wire, 0x69)) {
        Serial.println("BMI160 FAIL");
        while (1);
    }
    Serial.println("BMI160 OK");

    BMI160.setAccelerometerRate(50);
    BMI160.setGyroRate(50);
    BMI160.setAccelerometerRange(2);
    BMI160.setGyroRange(250);

    setupWiFi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);
    
    Serial.println("AI READY");
}

void setupWiFi() {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 50) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println(WiFi.localIP());
    }
}

// MQTT CALLBACK - XỬ LÝ NGAY LẬP TỨC
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == TOPIC_LED_CONTROL) {
        Serial.print("MQTT LED command: ");
        Serial.println(message);
        
        if (message == "AUTO") {
            ledManualMode = false;
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED -> AUTO mode");
        } 
        else if (message == "ON") {
            ledManualMode = true;
            ledState = true;
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED -> ON (immediate)");
        } 
        else if (message == "OFF") {
            ledManualMode = true;
            ledState = false;
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED -> OFF (immediate)");
        }
    }
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("MQTT connecting...");
        String clientId = "ESP32-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println("connected");
            client.subscribe(TOPIC_LED_CONTROL);
            client.publish(TOPIC_DEVICE_STATUS_ESP32, "online");
        } else {
            Serial.print("failed");
            delay(2000);
        }
    }
}

void runAI() {
    int axRaw, ayRaw, azRaw;
    int gxRaw, gyRaw, gzRaw;

    BMI160.readAccelerometer(axRaw, ayRaw, azRaw);
    BMI160.readGyro(gxRaw, gyRaw, gzRaw);

    features[feature_ix++] = ((float)axRaw / 16384.0f) * 9.80665f;
    features[feature_ix++] = ((float)ayRaw / 16384.0f) * 9.80665f;
    features[feature_ix++] = ((float)azRaw / 16384.0f) * 9.80665f;
    features[feature_ix++] = ((float)gxRaw / 131.0f) * 0.0174533f;
    features[feature_ix++] = ((float)gyRaw / 131.0f) * 0.0174533f;
    features[feature_ix++] = ((float)gzRaw / 131.0f) * 0.0174533f;

    if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        feature_ix = 0;

        signal_t signal;
        numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        
        ei_impulse_result_t result;
        EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

        if (res == EI_IMPULSE_OK) {
            for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                String label = result.classification[i].label;
                float value = result.classification[i].value;
                
                if (label == "fall") {
                    fallScore = value;
                } else if (label == "idle") {
                    idleScore = value;
                }
            }
            
            bool currentFall = (fallScore > 0.2);
            
            if (currentFall && !fallDetected) {
                fallDetected = true;
                lastFallTime = millis();
                Serial.println("FALL DETECTED!");
            } else if (!currentFall) {
                fallDetected = false;
            }
            
            // AUTO mode: LED sáng 3 giây khi phát hiện ngã
            if (!ledManualMode) {
                if (fallDetected && (millis() - lastFallTime < 3000)) {
                    digitalWrite(LED_PIN, HIGH);
                } else if (!ledManualMode) {
                    digitalWrite(LED_PIN, LOW);
                }
            }
            
            Serial.print("Fall: ");
            Serial.print(fallScore, 2);
            Serial.print(" | Idle: ");
            Serial.println(idleScore, 2);
        }
    }
}

void loop() {
    // 1. LUÔN DUY TRÌ KẾT NỐI MQTT
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();  // Xử lý MQTT ngay lập tức
    
    // 2. CHẠY AI MỖI 20ms
    if (millis() - lastAIrun >= 20) {
        lastAIrun = millis();
        runAI();
    }
    
    // 3. GỬI DỮ LIỆU LÊN MQTT MỖI 1 GIÂY
    if (millis() - lastSendTime >= 1000) {
        String status = (fallScore > 0.2) ? "fall" : "idle";
        client.publish(TOPIC_FALL_STATUS, status.c_str());
        client.publish(TOPIC_FALL_SCORE, String(fallScore).c_str());
        lastSendTime = millis();
    }
    
    // Delay nhỏ để CPU không bị treo
    delay(5);
}