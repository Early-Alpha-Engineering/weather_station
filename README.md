# ESP8266 Weather Station

A weather station project for ESP8266 NodeMCU that uses a DHT sensor for temperature and humidity readings and displays them on an OLED screen.

## Hardware Requirements

- ESP8266 NodeMCU board
- DHT11 or DHT22 temperature/humidity sensor
- SSD1306 OLED display (128x64)
- Jumper wires

## Pin Connections

- DHT Data Pin: D4
- DHT Pullup Pin: D5
- OLED SDA: D2 (GPIO4)
- OLED SCL: D1 (GPIO5)

## Features

- Temperature and humidity readings
- OLED display for data visualization
- WiFi connectivity using WiFiManager for easy configuration
- Support for multiple cities

## Libraries Used

- Wire
- DHT sensor library
- Adafruit Unified Sensor
- Adafruit GFX Library
- Adafruit BusIO
- SPI
- Adafruit SSD1306
- WiFiManager
- ESP8266WiFi
- ESP8266WebServer
- DNSServer
- ESP8266HTTPClient
- ArduinoJson
- EEPROM

## Build Instructions

The project includes a build script (`build.sh`) that automates the building and uploading process.

### Build Script Options

- `-p, --port PORT`: Specify the serial port (default: auto-detect)
- `-b, --board BOARD`: Board type (default: esp8266:esp8266:nodemcu)
- `-c, --cpu CPU`: CPU frequency (default: 80 MHz)
- `-u, --upload-speed BAUD`: Upload speed (default: 115200)
- `-d, --debug-level LEVEL`: Debug level (default: None____)
- `-f, --flash-size SIZE`: Flash size (default: 4M1M)
- `-i, --install-libs`: Install required libraries
- `-n, --no-upload`: Compile only, don't upload
- `-m, --monitor`: Start serial monitor after upload
- `-e, --erase`: Erase flash before uploading
- `-l, --list-ports`: List available serial ports
- `-h, --help`: Show help message

### Example Usage

Build and upload:
```
./build.sh -p /dev/ttyUSB0
```

Build only:
```
./build.sh -n
```

Install libraries and build:
```
./build.sh -i -n
```

## Recent Changes

- Fixed board configuration to use NodeMCU instead of generic ESP8266
- Set correct debug level formatting for ESP8266 Arduino Core
- Added WiFiManager for easy WiFi configuration
- Fixed pin definitions for NodeMCU board 