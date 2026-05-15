# VOSTOK NEXUS v5.1 - Hardware Mapping

## Base Station (ESP32-S3 DevKit)

| Component | Interface | Pins (S3) | Description |
|-----------|-----------|-----------|-------------|
| BME280    | I2C       | SDA: 4, SCL: 5 | Temp, Humidity, Pressure |
| NEO-7M GPS| UART1     | TX: 1, RX: 2  | Global Positioning |
| MicroSD   | SPI       | MOSI: 11, MISO: 13, CLK: 12, CS: 10 | Data Logging (JSONL) |
| DS18B20   | OneWire   | 14        | Water Temperature |
| WS2812    | RMT       | 48        | Status LED (Onboard) |
| Buzzer    | PWM       | 45        | Active Alarm |
| Button 1  | GPIO      | 0         | Mode Switch |
| Button 2  | GPIO      | 21        | Reset / Action |

## Rod Station (ESP32-C3)

| Component | Interface | Pins (C3) | Description |
|-----------|-----------|-----------|-------------|
| MPU-6050  | I2C       | SDA: 8, SCL: 9 | Accelerometer / Gyro |
| Hall 3144E| GPIO      | 3         | Reel Revolution Counter |
| WS2812    | RMT       | 2         | Bite Alert LED |

## Communication
- Protocol: ESP-NOW (2.4GHz)
- Encryption: None (for latency)
- Channel: 1 (shared with AP)
