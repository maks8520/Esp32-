# VOSTOK NEXUS v5.1 ULTRA PREMIUM

Production-quality telemetry system for professional fishing.

## Project Structure
- **/vostok-nexus-v5-1**: Main firmware for ESP32-S3 (Base Station).
- **/vostok-nexus-v5-1/rod_firmware**: Firmware for ESP32-C3 (Rod Units).
- **/vostok-nexus-v5-1/web**: Premium Glassmorphism 2.0 Web Dashboard (PWA).

## Build & Deployment
Builds are automated via GitHub Actions.
To build locally:
1. Base: `cd vostok-nexus-v5-1 && idf.py build`
2. Rod: `cd vostok-nexus-v5-1/rod_firmware && idf.py build`

## Hardware
Refer to `HARDWARE_MAPPING.md` for pinouts.
