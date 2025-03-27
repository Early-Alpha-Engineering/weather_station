# Weather Station

An ESP8266-based weather station that displays temperature and humidity from both local DHT11 sensor and online weather API.

## Features

- Displays local temperature and humidity readings from DHT11 sensor
- Fetches and displays weather data for Romanian cities from online API
- City selection via WiFi configuration portal
- Persistent city storage using EEPROM
- Automatic WiFi reconnection

## Hardware Requirements

- ESP8266 (D1 Mini or similar)
- DHT11 temperature and humidity sensor
- SSD1306 OLED display (128x64)
- Button for city selection (connected to D3)
- 10k ohm pull-up resistor (connected between DHT data pin and VCC)

## Libraries Used

- DHT sensor library
- Adafruit SSD1306
- Adafruit GFX
- WiFiManager
- ArduinoJson
- ESP8266WiFi
- ESP8266HTTPClient

## Version History

- 1.0.1 - Fixed pull-up resistor issue with DHT sensor and added temperature reading validation
- 1.0.0 - Initial release with improved DHT sensor reliability 