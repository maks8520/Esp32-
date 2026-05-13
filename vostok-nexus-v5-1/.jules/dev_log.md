# VOSTOK NEXUS v5.1 ULTRA PREMIUM - Development Log

## Palette (UX & Accessibility)
- **Haptics:** Integrated `navigator.vibrate` for critical bite alerts (`[100, 50, 100]`) and UI confirmations.
- **Feedback:** Added loading states and spinners for the Windy map and AI Fish ID modules.
- **A11y:** Implemented semantic HTML5 tags, ARIA labels for all interactive elements, and `role="status"` for the connection indicator.
- **Robustness:** Added "Irreversible Action" confirmation modals for SD card erasing and system reboots.

## Bolt (Performance)
- **High-FPS:** Utilized `requestAnimationFrame` for all UI animations (SVG gauges, water ripples, rod wobbles) ensuring a smooth 60fps experience.
- **Network Thrift:** Implemented 300ms debouncing on settings sliders to prevent saturating the ESP32 WebSocket buffer.
- **Lazy Loading:** Windy Map assets (Leaflet JS/CSS) are only loaded upon navigating to the Meteo tab.
- **Firmware Multi-Core:**
  - Core 0: Web Server, WebSocket Handling, and SD Logging.
  - Core 1: ESP-NOW Packet Processing and Sensor Fusion.
- **Memory Management:** Large weather profile arrays are allocated in PSRAM via `MALLOC_CAP_SPIRAM`.

## Sentinel (Security & Robustness)
- **Secret Management:** Removed hardcoded API keys from the frontend. The Windy key is fetched securely from the ESP32's protected NVS on socket connection.
- **Data Integrity:** SD card logs use the JSONL format with Fletcher-32 checksums to ensure data can be validated after power loss.
- **Input Sanitization:** Added `maxlength` and type validation to all form fields and settings inputs.
- **Queue Protection:** Integrated lock-free queues between the ESP-NOW receiver (ISR-like context) and the processing tasks to prevent packet loss.
