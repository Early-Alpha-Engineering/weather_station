# Changelog

All notable changes to the Weather Station project will be documented in this file.

## [1.0.1] - 2024-03-27

### Fixed
- Changed pull-up resistor pin from D3 to D5 to avoid conflict with the button
- Implemented software pull-up for DHT sensor data line
- Improved DHT sensor initialization with proper delays

### Added
- Enhanced temperature reading reliability with multiple readings and averaging
- Added validation to ensure readings are within reasonable ranges (0-50°C for DHT11)
- Added more detailed diagnostic information in serial output

## [1.0.0] - 2024-03-27

### Added
- Initial release of the Weather Station
- Local temperature and humidity readings from DHT11 sensor
- Online weather data for Romanian cities
- City selection via WiFi configuration portal
- Persistent city storage using EEPROM
- Automatic WiFi reconnection 