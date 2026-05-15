# VOSTOK NEXUS v5.1 - Hardware Mapping

## Base Station (ESP32-S3 DevKit)
| Component | Protocol | Pins (SDA/TX/MOSI) | Pins (SCL/RX/CLK) | CS/Other |
|-----------|----------|-------------------|-------------------|----------|
| BME280    | I2C      | GPIO 8            | GPIO 9            | -        |
| NEO-7M GPS| UART     | GPIO 43           | GPIO 44           | -        |
| MicroSD   | SPI      | GPIO 11           | GPIO 13           | CLK:12, CS:10|
| DS18B20   | OneWire  | GPIO 14           | -                 | -        |
| WS2812 RGB| PWM      | GPIO 48           | -                 | -        |
| Buzzer    | PWM      | GPIO 21           | -                 | Active   |
| Button 1  | Input    | GPIO 0            | -                 | Boot     |
| Button 2  | Input    | GPIO 1            | -                 | -        |

## Rod Unit (ESP32-C3)
| Component | Protocol | Pins (SDA/TX) | Pins (SCL/RX) | Other |
|-----------|----------|---------------|---------------|-------|
| MPU-6050  | I2C      | GPIO 4        | GPIO 5        | -     |
| Hall 3144E| Analog   | GPIO 6        | -             | -     |
| WS2812 RGB| PWM      | GPIO 7        | -             | -     |

## Software Requirements
- ESP-IDF v5.1+
- Node.js (for web preview)
- WebSocket client support in browser
