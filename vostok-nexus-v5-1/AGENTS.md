# VOSTOK NEXUS v5.1 - Development Guide

## Logic Architecture
- **Base Station (S3):**
  - Task Pinned Core 0: Low-latency ESP-NOW, Sensor Interrupts.
  - Task Pinned Core 1: Network Stack (WS/HTTP), SD Logging, UI updates.
- **Rod Station (C3):**
  - MPU-6050 & Hall data via I2C/GPIO.
  - Periodic ESP-NOW telemetry to Base.

## Design System (Glassmorphism 2.0)
- Colors: Deep Navy (`#020617`), Lime (`#d4ff8f`), Cyan (`#7dd3fc`).
- Effects: 25px blur, noise grain, scanlines.
- Performance: requestAnimationFrame for gauge and animations.

## Hardware Config
Refer to `HARDWARE_MAPPING.md` for exact pinouts on S3 and C3.
