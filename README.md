# ESP32 Temperature and Humidity Sensor Email App

This project uses an ESP32 development board and an SHT31-D sensor to monitor temperature and humidity. The ESP32 connects to WiFi, synchronizes time via NTP, and sends email alerts with sensor readings using an SMTP server (e.g., Gmail). It also supports configuration via a captive portal and stores settings in flash memory.

## Features

- Reads temperature and humidity from an SHT31-D sensor via I2C.
- Sends email alerts with readings at scheduled times or when high temperature is detected.
- WiFi configuration via [WiFiManager](https://github.com/tzapu/WiFiManager) captive portal.
- Stores SMTP and timezone settings in flash using LittleFS.
- Time synchronization using NTP servers.
- Manual reading and email trigger via serial commands.
- RS-232 serial output for integration with legacy systems.

## Hardware Required

- ESP32 development board
- SHT31-D temperature and humidity sensor (I2C)
- RS-232 TTL to RS232 Module (optional, for Serial2)
- Pushbutton or jumper for RESET_PIN (GPIO 23)

## Libraries Used

- [Adafruit_SHT31](https://github.com/adafruit/Adafruit_SHT31)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ESP Mail Client](https://github.com/mobizt/ESP-Mail-Client)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- LittleFS (built-in with ESP32 Arduino core)

## Configuration

On first boot or after a reset, the ESP32 will start a WiFi access point named `TempSensorAP`. Connect to it and configure:

- WiFi credentials
- Time zone string (e.g., `PST8PDT,M3.2.0,M11.1.0`)
- SMTP server, port, sender/recipient email, app password, subject, and sender name

Settings are saved to flash and persist across reboots.

## Usage

- The device sends emails automatically at 9:00, 13:00, and 16:00, or if the temperature exceeds 82°F.
- You can manually trigger a reading and email by sending `r` or `R` via the USB serial monitor or RS-232 serial.
- To factory reset (clear all settings), hold GPIO 23 (RESET_PIN) LOW during boot.

## File Structure

- [`src/main.cpp`](src/main.cpp): Main application code.
- `/config.json`: Configuration file stored in LittleFS.

## Example Email Content

```
Temperature: 75.23 F | Humidity: 45.67 %
Time of reading: 2024-05-01 13:00:00
```

## Notes

- For Gmail SMTP, you must use an App Password (not your main password).
- Ensure your SHT31-D sensor is connected to the default I2C pins (GPIO 21 SDA, GPIO 22 SCL).
- Serial2 (UART2) uses GPIO 17 (TX) and GPIO 16 (RX) by default.

## License

MIT License

---
