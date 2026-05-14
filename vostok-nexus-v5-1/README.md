# VOSTOK NEXUS v5.1 ULTRA PREMIUM

Профессиональная система телеметрии для рыбалки на базе ESP32-S3 и ESP32-C3.

## Архитектура
- **Base (ESP32-S3):** Центральный хаб. Обработка ESP-NOW, WiFi AP, WebSocket сервер, логирование на SD карту.
- **Rod (ESP32-C3):** Модуль на удочке. Сбор данных акселерометра и датчика Холла, передача через ESP-NOW.

## Инструкция по прошивке
Для прошивки используйте [ESP Web Flasher](https://espressif.github.io/esptool-js/):

1. Подключите ESP32 к USB.
2. Выберите нужные `.bin` файлы из раздела **Actions** этого репозитория:
   - `bootloader.bin` (адрес: 0x0000)
   - `partition-table.bin` (адрес: 0x8000)
   - `vostok-nexus-base.bin` или `vostok-nexus-rod.bin` (адрес: 0x10000)
3. Нажмите **Program**, чтобы начать процесс.

## Стек технологий
- **Firmware:** ESP-IDF v5.1 (Dual-Core, NVS, FreeRTOS).
- **Web UI:** PWA (HTML5, Glassmorphism 2.0, Chart.js, Leaflet).
- **CI/CD:** GitHub Actions (автоматическая сборка артефактов).
