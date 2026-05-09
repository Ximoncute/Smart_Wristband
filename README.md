# Smart Health Bracelet

## 📌 Introduction
Smart Health Bracelet is a wearable IoT system using ESP32 to monitor:

- Heart Rate (BPM)
- Blood Oxygen (SpO2)
- Hand Movement / Motion Detection

The system combines MAX30102 and BMI160 sensors with UART communication between two ESP32 boards.

---

## ⚙️ Hardware

### Main Components
- ESP32 x2
- MAX30102 Heart Rate & SpO2 Sensor
- BMI160 Motion Sensor
- LED Warning Indicator

---

## 🔥 Features

- Real-time BPM monitoring
- Real-time SpO2 monitoring
- 3-axis motion detection
- UART communication between ESP32 boards
- Warning LED for abnormal health conditions:
  - BPM < 45
  - SpO2 < 90

---

## 🔌 Communication

UART connection:

| Heart ESP32 | Motion ESP32 |
|---|---|
| TX21 | RX16 |
| RX20 | TX17 |
| GND | GND |

<img width="663" height="421" alt="image" src="https://github.com/user-attachments/assets/6f2e6440-f14c-41c5-958f-56911d1ae145" />

---

## 📊 Example Output

### Heart Sensor ESP32
```bash
BPM: 75 | SPO2: 98
UART SENT