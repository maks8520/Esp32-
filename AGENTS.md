# VOSTOK NEXUS v5.1 ULTRA PREMIUM - Developer Guide

## Architecture
- **Base (ESP32-S3):** Dual-core. Core 0 handles sensors and ESP-NOW. Core 1 handles WiFi AP and WebSocket Server. Logs are saved to SD card in JSONL format.
- **Rod (ESP32-C3):** Low-power. Polls MPU-6050 and Hall sensor. Sends data via ESP-NOW to Base MAC.
- **Web Interface:** High-fidelity PWA. Uses Glassmorphism CSS for premium look. WebSocket for real-time telemetry.

## Build Instructions
1. **Base:** `cd firmware/base_s3 && idf.py build`
2. **Rod:** `cd firmware/rod_c3 && idf.py build`
3. **Web:** `cd web && node server.js` (or serve `public/` via any static host)

## Verification
- Checked ESP-NOW ack delivery.
- Verified WebSocket broadcast concurrency.
- Validated PWA manifest and service worker.
