# VOSTOK NEXUS v5.1 Project Instructions

## Build and Flash
- Base Station: Use ESP-IDF v5.x to build and flash to ESP32-S3.
- Rod Station: Use ESP-IDF v5.x to build and flash to ESP32-C3.

## Web Interface
- The web interface is a PWA. To update, modify files in `web/` and refresh the cache.
- For deployment on the ESP32, the `web/` directory should be served using SPIFFS or embedded into the binary.

## Testing
- Use the built-in simulation in `script.js` to test the UI without hardware.
- Monitor serial output for ESP-NOW and WebSocket logs.
