# VOSTOK NEXUS v5.1 ULTRA PREMIUM

High-performance fishing telemetry system powered by ESP32-S3 and ESP32-C3.

## Features
- **Real-time Telemetry:** ESP-NOW protocol for sub-100ms latency.
- **AI Bite Predictor:** Advanced logic for detecting fish activity.
- **Glassmorphism UI:** Premium PWA with interactive maps and charts.
- **Environmental Monitoring:** BME280, DS18B20, and Open-Meteo integration.
- **Asynchronous Logging:** JSONL format on MicroSD card.

## Project Structure
- `main/`: Base Station firmware (ESP32-S3).
- `rod_firmware/`: Rod Station firmware (ESP32-C3).
- `web/`: Premium PWA frontend.

## Installation
1. Clone the repository.
2. Build and flash the Base Station:
   ```bash
   idf.py build flash monitor
   ```
3. Build and flash the Rod Station:
   ```bash
   cd rod_firmware && idf.py build flash monitor
   ```

## License
MIT - Premium Edition.
