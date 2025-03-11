#include "Wire.h"
#include "DHT.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h> // Add EEPROM library for persistent storage

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define DHTPIN D4

#define DHTTYPE DHT11

#define OLED_RESET -1 

// Super-robust EEPROM configuration
#define EEPROM_SIZE 32  // Increase EEPROM size for even more redundancy
#define EEPROM_MAGIC 0xAB  // Magic byte to verify EEPROM is initialized
#define EEPROM_CRC_SEED 0x1D // CRC seed value

// Main storage area
#define CITY_ADDR1 0
#define VERIFY_ADDR1 1
#define MAGIC_ADDR1 2
#define CRC_ADDR1 3

// Redundant storage areas
#define CITY_ADDR2 4
#define VERIFY_ADDR2 5
#define MAGIC_ADDR2 6
#define CRC_ADDR2 7

#define CITY_ADDR3 8
#define VERIFY_ADDR3 9
#define MAGIC_ADDR3 10
#define CRC_ADDR3 11

#define CITY_ADDR4 12
#define VERIFY_ADDR4 13
#define MAGIC_ADDR4 14
#define CRC_ADDR4 15

// List of supported cities (must match dropdown options)
const char* cityList[] = {
    "Bucharest", "Cluj-Napoca", "Timisoara", "Iasi", "Constanta",
    "Craiova", "Brasov", "Galati", "Ploiesti", "Oradea",
    "Sibiu", "Arad", "Pitesti", "Baia Mare", "Buzau",
    "Satu Mare", "Botosani", "Ramnicu Valcea", "Suceava", "Piatra Neamt",
    "Drobeta-Turnu Severin", "Targu Mures", "Targoviste", "Focsani", "Tulcea",
    "Alba Iulia", "Giurgiu", "Hunedoara", "Bistrita", "Resita"
};
const int NUM_CITIES = 30;

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

DHT dht(DHTPIN, DHTTYPE);

float temperature;
float humidity;
float windspeed;

unsigned long previousMillis = 0;
const long interval = 10000;  // 10 seconds
bool showDHTData = true;

String cityName;
int cityIndex = 0; // Default to Bucharest (index 0)

// Modify the WiFiManager parameter to use a proper ID
// Instead of using complex HTML, use a simple parameter
WiFiManagerParameter custom_city_param("city", "City Index", "0", 2);

// Add a function to calculate a simple 8-bit CRC
byte calculateCRC(byte data) {
    byte crc = EEPROM_CRC_SEED;
    byte val = data;
    
    for (int i = 0; i < 8; i++) {
        if ((crc & 0x01) ^ (val & 0x01)) {
            crc = (crc >> 1) ^ 0x8C;
        } else {
            crc >>= 1;
        }
        val >>= 1;
    }
    
    return crc;
}

void saveCityToEEPROMSafe(int index) {
    // Sanity check for valid index
    if (index < 0 || index >= NUM_CITIES) {
        Serial.println("ERROR: Attempt to save invalid city index, defaulting to 0");
        index = 0;
    }
    
    Serial.print("SAVING CITY INDEX TO EEPROM with MAXIMUM RELIABILITY: ");
    Serial.println(index);
    
    // Calculate CRC
    byte crc = calculateCRC(index);
    
    // Begin EEPROM session
    EEPROM.begin(EEPROM_SIZE);
    
    // Prepare data for all locations before writing
    
    // First location
    EEPROM.write(CITY_ADDR1, index);
    delay(5);  // Add delay between operations
    EEPROM.write(VERIFY_ADDR1, 255 - index);
    delay(5);
    EEPROM.write(MAGIC_ADDR1, EEPROM_MAGIC);
    delay(5);
    EEPROM.write(CRC_ADDR1, crc);
    delay(5);
    
    // Second location
    EEPROM.write(CITY_ADDR2, index);
    delay(5);
    EEPROM.write(VERIFY_ADDR2, 255 - index);
    delay(5);
    EEPROM.write(MAGIC_ADDR2, EEPROM_MAGIC);
    delay(5);
    EEPROM.write(CRC_ADDR2, crc);
    delay(5);
    
    // Third location
    EEPROM.write(CITY_ADDR3, index);
    delay(5);
    EEPROM.write(VERIFY_ADDR3, 255 - index);
    delay(5);
    EEPROM.write(MAGIC_ADDR3, EEPROM_MAGIC);
    delay(5);
    EEPROM.write(CRC_ADDR3, crc);
    delay(5);
    
    // Fourth location
    EEPROM.write(CITY_ADDR4, index);
    delay(5);
    EEPROM.write(VERIFY_ADDR4, 255 - index);
    delay(5);
    EEPROM.write(MAGIC_ADDR4, EEPROM_MAGIC);
    delay(5);
    EEPROM.write(CRC_ADDR4, crc);
    delay(5);
    
    // Commit changes with a forced flush
    bool success = EEPROM.commit();
    
    // Wait a substantial amount of time to ensure write completes
    delay(200);
    
    // Re-read all values to verify
    int readBack1 = EEPROM.read(CITY_ADDR1);
    int readBack2 = EEPROM.read(CITY_ADDR2);
    int readBack3 = EEPROM.read(CITY_ADDR3);
    int readBack4 = EEPROM.read(CITY_ADDR4);
    
    // Verify CRCs
    byte readCRC1 = EEPROM.read(CRC_ADDR1);
    byte readCRC2 = EEPROM.read(CRC_ADDR2);
    byte readCRC3 = EEPROM.read(CRC_ADDR3);
    byte readCRC4 = EEPROM.read(CRC_ADDR4);
    
    // End EEPROM session
    EEPROM.end();
    
    // Verify all locations
    bool verify1 = (readBack1 == index && readCRC1 == crc);
    bool verify2 = (readBack2 == index && readCRC2 == crc);
    bool verify3 = (readBack3 == index && readCRC3 == crc);
    bool verify4 = (readBack4 == index && readCRC4 == crc);
    
    Serial.println("EEPROM SAVE VERIFICATION RESULTS:");
    Serial.print("Location 1: Read=");
    Serial.print(readBack1);
    Serial.print(", CRC=");
    Serial.print(readCRC1, HEX);
    Serial.print(" (Success: ");
    Serial.print(verify1 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 2: Read=");
    Serial.print(readBack2);
    Serial.print(", CRC=");
    Serial.print(readCRC2, HEX);
    Serial.print(" (Success: ");
    Serial.print(verify2 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 3: Read=");
    Serial.print(readBack3);
    Serial.print(", CRC=");
    Serial.print(readCRC3, HEX);
    Serial.print(" (Success: ");
    Serial.print(verify3 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 4: Read=");
    Serial.print(readBack4);
    Serial.print(", CRC=");
    Serial.print(readCRC4, HEX);
    Serial.print(" (Success: ");
    Serial.print(verify4 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Overall EEPROM commit successful: ");
    Serial.println(success ? "YES" : "NO");
    
    // If any verification failed, try again
    if (!(verify1 && verify2 && verify3 && verify4)) {
        Serial.println("EEPROM VERIFICATION FAILED! Retrying after delay...");
        delay(500); // Wait longer before retry
        
        // Recursive call to try again
        saveCityToEEPROMSafe(index);
    } else {
        Serial.println("EEPROM WRITE COMPLETELY VERIFIED!");
    }
}

int loadCityFromEEPROMSafe() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read from all four storage locations
    int index1 = EEPROM.read(CITY_ADDR1);
    int verify1 = EEPROM.read(VERIFY_ADDR1);
    int magic1 = EEPROM.read(MAGIC_ADDR1);
    int crc1 = EEPROM.read(CRC_ADDR1);
    
    int index2 = EEPROM.read(CITY_ADDR2);
    int verify2 = EEPROM.read(VERIFY_ADDR2);
    int magic2 = EEPROM.read(MAGIC_ADDR2);
    int crc2 = EEPROM.read(CRC_ADDR2);
    
    int index3 = EEPROM.read(CITY_ADDR3);
    int verify3 = EEPROM.read(VERIFY_ADDR3);
    int magic3 = EEPROM.read(MAGIC_ADDR3);
    int crc3 = EEPROM.read(CRC_ADDR3);
    
    int index4 = EEPROM.read(CITY_ADDR4);
    int verify4 = EEPROM.read(VERIFY_ADDR4);
    int magic4 = EEPROM.read(MAGIC_ADDR4);
    int crc4 = EEPROM.read(CRC_ADDR4);
    
    EEPROM.end();
    
    // Check validity of each storage location
    bool valid1 = (verify1 == (255 - index1) && magic1 == EEPROM_MAGIC && crc1 == calculateCRC(index1));
    bool valid2 = (verify2 == (255 - index2) && magic2 == EEPROM_MAGIC && crc2 == calculateCRC(index2));
    bool valid3 = (verify3 == (255 - index3) && magic3 == EEPROM_MAGIC && crc3 == calculateCRC(index3));
    bool valid4 = (verify4 == (255 - index4) && magic4 == EEPROM_MAGIC && crc4 == calculateCRC(index4));
    
    Serial.println("EEPROM LOAD VERIFICATION:");
    Serial.print("Location 1: Index=");
    Serial.print(index1);
    Serial.print(", Verify=");
    Serial.print(verify1);
    Serial.print(", Magic=");
    Serial.print(magic1, HEX);
    Serial.print(", CRC=");
    Serial.print(crc1, HEX);
    Serial.print(" (Expected CRC=");
    Serial.print(calculateCRC(index1), HEX);
    Serial.print(") (Valid: ");
    Serial.print(valid1 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 2: Index=");
    Serial.print(index2);
    Serial.print(", Verify=");
    Serial.print(verify2);
    Serial.print(", Magic=");
    Serial.print(magic2, HEX);
    Serial.print(", CRC=");
    Serial.print(crc2, HEX);
    Serial.print(" (Expected CRC=");
    Serial.print(calculateCRC(index2), HEX);
    Serial.print(") (Valid: ");
    Serial.print(valid2 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 3: Index=");
    Serial.print(index3);
    Serial.print(", Verify=");
    Serial.print(verify3);
    Serial.print(", Magic=");
    Serial.print(magic3, HEX);
    Serial.print(", CRC=");
    Serial.print(crc3, HEX);
    Serial.print(" (Expected CRC=");
    Serial.print(calculateCRC(index3), HEX);
    Serial.print(") (Valid: ");
    Serial.print(valid3 ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Location 4: Index=");
    Serial.print(index4);
    Serial.print(", Verify=");
    Serial.print(verify4);
    Serial.print(", Magic=");
    Serial.print(magic4, HEX);
    Serial.print(", CRC=");
    Serial.print(crc4, HEX);
    Serial.print(" (Expected CRC=");
    Serial.print(calculateCRC(index4), HEX);
    Serial.print(") (Valid: ");
    Serial.print(valid4 ? "YES" : "NO");
    Serial.println(")");
    
    // Count valid locations with the same index
    int countBucharest = 0;
    int countIasi = 0;
    int countBrasov = 0;
    int countArad = 0;
    int countOther = 0;
    
    // Tally valid data
    if (valid1) {
        if (index1 == 0) countBucharest++;
        else if (index1 == 3) countIasi++; // Iasi
        else if (index1 == 6) countBrasov++; // Brasov
        else if (index1 == 11) countArad++; // Arad
        else countOther++;
    }
    
    if (valid2) {
        if (index2 == 0) countBucharest++;
        else if (index2 == 3) countIasi++;
        else if (index2 == 6) countBrasov++;
        else if (index2 == 11) countArad++;
        else countOther++;
    }
    
    if (valid3) {
        if (index3 == 0) countBucharest++;
        else if (index3 == 3) countIasi++;
        else if (index3 == 6) countBrasov++;
        else if (index3 == 11) countArad++;
        else countOther++;
    }
    
    if (valid4) {
        if (index4 == 0) countBucharest++;
        else if (index4 == 3) countIasi++;
        else if (index4 == 6) countBrasov++;
        else if (index4 == 11) countArad++;
        else countOther++;
    }
    
    // Determine winner by vote count
    int selectedIndex = -1;
    
    Serial.println("EEPROM VOTING RESULTS:");
    Serial.print("Bucharest (0): ");
    Serial.println(countBucharest);
    Serial.print("Iasi (3): ");
    Serial.println(countIasi);
    Serial.print("Brasov (6): ");
    Serial.println(countBrasov);
    Serial.print("Arad (11): ");
    Serial.println(countArad);
    Serial.print("Other indices: ");
    Serial.println(countOther);
    
    // Find the highest count
    if (countIasi > 0 && countIasi >= countBucharest && countIasi >= countBrasov && countIasi >= countArad && countIasi >= countOther) {
        selectedIndex = 3; // Iasi
        Serial.println("Selected Iasi (3) by vote count");
    }
    else if (countBrasov > 0 && countBrasov >= countBucharest && countBrasov >= countIasi && countBrasov >= countArad && countBrasov >= countOther) {
        selectedIndex = 6; // Brasov
        Serial.println("Selected Brasov (6) by vote count");
    }
    else if (countArad > 0 && countArad >= countBucharest && countArad >= countIasi && countArad >= countBrasov && countArad >= countOther) {
        selectedIndex = 11; // Arad
        Serial.println("Selected Arad (11) by vote count");
    }
    else if (countOther > 0 && countOther >= countBucharest && countOther >= countIasi && countOther >= countBrasov && countOther >= countArad) {
        // Use any valid index from the "other" category
        if (valid1 && index1 != 0 && index1 != 3 && index1 != 6 && index1 != 11) {
            selectedIndex = index1;
        } else if (valid2 && index2 != 0 && index2 != 3 && index2 != 6 && index2 != 11) {
            selectedIndex = index2;
        } else if (valid3 && index3 != 0 && index3 != 3 && index3 != 6 && index3 != 11) {
            selectedIndex = index3;
        } else if (valid4 && index4 != 0 && index4 != 3 && index4 != 6 && index4 != 11) {
            selectedIndex = index4;
        }
        Serial.print("Selected other city by vote count: ");
        Serial.println(selectedIndex);
    }
    else {
        // Default to Bucharest if it has any votes, or if nothing else is valid
        selectedIndex = 0;
        Serial.println("Selected Bucharest (0) by default or vote count");
    }
    
    // Sanity check on the index
    if (selectedIndex < 0 || selectedIndex >= NUM_CITIES) {
        selectedIndex = 0; // Default to Bucharest
        Serial.println("Index out of range, defaulting to Bucharest (0)");
    }
    
    // Re-save the selected index to all locations to ensure consistency
    Serial.print("Re-saving selected index to ensure consistency: ");
    Serial.println(selectedIndex);
    saveCityToEEPROMSafe(selectedIndex);
    
    return selectedIndex;
}

// Function to initialize EEPROM with default values
void initializeEEPROMSafe() {
    Serial.println("Initializing EEPROM with default values and maximum reliability...");
    saveCityToEEPROMSafe(0); // Default to Bucharest
    Serial.println("EEPROM initialization complete.");
}

// Replacement for original saveCityToEEPROM function
void saveCityToEEPROM(int index) {
    saveCityToEEPROMSafe(index);
}

// Replacement for original loadCityFromEEPROM function
int loadCityFromEEPROM() {
    return loadCityFromEEPROMSafe();
}

// Function to force a specific city (use for troubleshooting)
void forceCity(const char* cityName) {
    Serial.print("FORCING CITY TO: ");
    Serial.println(cityName);
    
    int forcedIndex = -1;
    
    // Find the index of the specified city
    for (int i = 0; i < NUM_CITIES; i++) {
        if (strcmp(cityList[i], cityName) == 0) {
            forcedIndex = i;
            break;
        }
    }
    
    if (forcedIndex >= 0) {
        // Save this index forcefully
        Serial.print("Found index for ");
        Serial.print(cityName);
        Serial.print(": ");
        Serial.println(forcedIndex);
        
        // Force update variables
        cityIndex = forcedIndex;
        ::cityName = cityList[cityIndex];
        
        // Save with maximum reliability
        saveCityToEEPROMSafe(cityIndex);
        
        // Double check it was saved correctly
        EEPROM.begin(EEPROM_SIZE);
        int saved1 = EEPROM.read(CITY_ADDR1);
        int saved2 = EEPROM.read(CITY_ADDR2);
        int saved3 = EEPROM.read(CITY_ADDR3);
        int saved4 = EEPROM.read(CITY_ADDR4);
        EEPROM.end();
        
        Serial.print("Force-save verification: ");
        Serial.print(saved1);
        Serial.print(", ");
        Serial.print(saved2);
        Serial.print(", ");
        Serial.print(saved3);
        Serial.print(", ");
        Serial.println(saved4);
        
        // Show confirmation on the display
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("CITY FORCED TO:");
        oled.setTextSize(2);
        oled.setCursor(0, 12);
        oled.print(cityName);
        oled.setTextSize(1);
        oled.setCursor(0, 32);
        oled.print("Index: ");
        oled.print(cityIndex);
        oled.setCursor(0, 42);
        oled.print("Save verified: ");
        oled.print((saved1 == cityIndex && saved2 == cityIndex && 
                   saved3 == cityIndex && saved4 == cityIndex) ? "YES" : "NO");
        oled.display();
        delay(4000);
    } else {
        Serial.print("ERROR: City name not found: ");
        Serial.println(cityName);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n========= Weather Station Starting =========");
    Serial.println("Initializing...");
    
    // Initialize EEPROM and test it first
    testEEPROM();
    
    // Load city from EEPROM
    cityIndex = loadCityFromEEPROM();
    cityName = cityList[cityIndex];
    Serial.print("Using city from EEPROM: ");
    Serial.print(cityName);
    Serial.print(" (index ");
    Serial.print(cityIndex);
    Serial.println(")");
    
    // Initialize hardware
    initializeDHTSensor();
    initializeOLEDDisplay();
    
    // IMPORTANT: If you specifically want to force Iasi, uncomment this line:
    // Uncomment to override the city selection with Iasi
    forceCity("Iasi");
    
    // Show splash screen with city info
    Text_EAEA();
    delay(3000);
    
    // Show city info
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("Selected City:");
    oled.setCursor(0, 12);
    oled.setTextSize(2);
    oled.print(cityName);
    oled.setTextSize(1);
    oled.setCursor(0, 32);
    oled.print("EEPROM Index: ");
    oled.print(cityIndex);
    oled.setCursor(0, 44);
    oled.print("Press D3 to change city");
    oled.display();
    delay(3000);

    // Now attempt to connect to WiFi
    Serial.println("Attempting to connect to WiFi...");
    bool wifiConnected = connectToWiFi();
    
    // Double-check WiFi connection status
    if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi is actually connected despite connectToWiFi() returning false!");
        wifiConnected = true;
        
        // Show connected message
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("WiFi connected!");
        oled.setCursor(0, 10);
        oled.print("SSID: ");
        oled.print(WiFi.SSID());
        oled.setCursor(0, 20);
        oled.print("IP: ");
        oled.print(WiFi.localIP().toString());
        oled.setCursor(0, 35);
        oled.print("City: ");
        oled.print(cityName);
        oled.display();
        delay(3000); // Show for 3 seconds
    }
    
    // If not connected, don't get stuck in a loop
    // The loop() function will keep trying to reconnect
    if (!wifiConnected) {
        Serial.println("Failed initial WiFi connection, proceeding with sensor data only.");
        // Display a clear message that we'll operate without WiFi for now
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("Operating without WiFi");
        oled.setCursor(0, 10);
        oled.print("Using local sensor data");
        oled.setCursor(0, 20);
        oled.print("Will keep attempting");
        oled.setCursor(0, 30);
        oled.print("to reconnect...");
        oled.display();
        delay(3000);
    } else {
        // If connected, proceed to get weather data
        Serial.print("WiFi connected! City selected: ");
        Serial.println(cityName);
        
        // Get coordinates and fetch initial weather
        float lat, lon;
        if (getRomanianCityCoordinates(cityIndex, lat, lon)) {
            getWeather(lat, lon);
        }
    }
    
    Serial.println("Setup complete");
}

void initializeDHTSensor() {
    dht.begin();
    Serial.println("DHT sensor initialized.");
}

void initializeOLEDDisplay() {
    if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 failed"));
        for(;;);
    }
    Serial.println("OLED display initialized.");
    oled.setTextWrap(true);
    oled.clearDisplay();
    oled.setTextColor(WHITE);
}

void showSplashScreen() {
    Text_EAEA();
    delay(5000);
}

bool connectToWiFi() {
    // Show connecting message first
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("Connecting to WiFi...");
    oled.setCursor(0, 10);
    oled.print("If no WiFi configured:");
    oled.setCursor(0, 20);
    oled.print("1. Connect to WiFi");
    oled.setCursor(0, 30);
    oled.print("   'WeatherStationAP'");
    oled.setCursor(0, 40);
    oled.print("2. Password: password");
    oled.setCursor(0, 50);
    oled.print("3. Open 192.168.4.1");
    oled.display();
    
    // Initialize WiFiManager but don't start it yet
    WiFiManager wm;
    
    // Set timeout for configuration portal
    wm.setConfigPortalTimeout(120); // 2 minutes timeout
    
    // Add a simpler parameter that's easier to retrieve
    wm.addParameter(&custom_city_param);
    
    // Add city selection HTML with CSS for styling and hidden input
    String cityOptions = "<style>";
    cityOptions += ".city-grid { display: flex; flex-wrap: wrap; gap: 5px; margin-bottom: 10px; }";
    cityOptions += ".city-btn { flex: 1 0 45%; margin: 2px; padding: 8px 5px; background-color: #1fa3ec; color: #fff; border: none; border-radius: 4px; cursor: pointer; }";
    cityOptions += ".city-btn.active { background-color: #007bff; }";
    cityOptions += "#city { display: none; }"; // Hide the input field
    cityOptions += "label[for='city'] { display: none; }"; // Hide the label
    cityOptions += "</style>";
    
    cityOptions += "<br><p><b>Select City:</b></p>";
    cityOptions += "<div class='city-grid'>";
    
    for (int i = 0; i < NUM_CITIES; i++) {
        String activeClass = (i == cityIndex) ? " active" : "";
        cityOptions += "<button class='city-btn" + activeClass + "' onclick='document.getElementById(\"city\").value=\"" + 
                      String(i) + "\"; highlightBtn(this)'>" + String(cityList[i]) + "</button>";
    }
    
    cityOptions += "</div>";
    cityOptions += "<p>Currently selected: <b><span id='cityName'>" + String(cityList[cityIndex]) + "</span></b></p>";
    
    // Add JavaScript to highlight selected button
    cityOptions += "<script>function highlightBtn(btn) { ";
    cityOptions += "document.querySelectorAll('.city-btn').forEach(b => b.classList.remove('active')); ";
    cityOptions += "btn.classList.add('active'); ";
    cityOptions += "document.getElementById('cityName').innerHTML = btn.innerText;";
    cityOptions += "}</script>";
    
    WiFiManagerParameter custom_html(cityOptions.c_str());
    wm.addParameter(&custom_html);
    
    // Attempt to connect to WiFi with a clear message
    Serial.println("Starting WiFiManager connection process...");
    delay(1000); // Give time for display to be read
    
    // Try to connect with autoConnect
    bool connected = wm.autoConnect("WeatherStationAP", "password");
    
    // Double-check connection status
    delay(1000); // Wait a bit for connection to stabilize
    
    // Verify connection status with WiFi.status()
    if (connected || WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected successfully!");
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        // After successful connection, get the selected city
        String cityIdxStr = custom_city_param.getValue();
        int newCityIdx = cityIdxStr.toInt();
        
        Serial.print("Selected city index: ");
        Serial.println(newCityIdx);
        
        // Validate and save if changed
        if (newCityIdx >= 0 && newCityIdx < NUM_CITIES && newCityIdx != cityIndex) {
            cityIndex = newCityIdx;
            cityName = cityList[cityIndex];
            saveCityToEEPROM(cityIndex);
            Serial.print("City changed to: ");
            Serial.println(cityName);
        }
    
        // Show connected message
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("WiFi connected!");
        oled.setCursor(0, 10);
        oled.print("SSID: ");
        oled.print(WiFi.SSID());
        oled.setCursor(0, 20);
        oled.print("IP: ");
        oled.print(WiFi.localIP().toString());
        oled.setCursor(0, 35);
        oled.print("City: ");
        oled.print(cityName);
        oled.display();
        delay(3000); // Show for 3 seconds
        
        Serial.println("Connected to WiFi.");
        return true;
    } else {
        Serial.println("Failed to connect to WiFi.");
        // Show not connected message
        displayNotConnectedMessage();
        return false;
    }
}

void displayNotConnectedMessage() {
    Serial.println("Not connected to WiFi.");
    
    // Make sure display is clear and settings are reset
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setTextWrap(true);
    
    // Show WiFi not connected message with AP instructions
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("WiFi not connected!");
    
    // Highlight with a rectangle
    oled.drawRect(0, 0, 128, 10, WHITE);
    
    oled.setCursor(0, 12);
    oled.print("Connect to WiFi SSID:");
    oled.setCursor(0, 22);
    oled.print("'WeatherStationAP'");
    oled.setCursor(0, 32);
    oled.print("Password: password");
    oled.setCursor(0, 42);
    oled.print("Go to: 192.168.4.1");
    
    // Flashing "Reconnecting..." at the bottom
    static bool flashState = false;
    flashState = !flashState;
    
    if (flashState) {
        oled.setTextSize(1);
        oled.setCursor(0, 55);
        oled.print("Reconnecting...");
    }
    
    // Ensure display is updated
    oled.display();
}

void loop() {
    unsigned long currentMillis = millis();

    // Check WiFi connection status more frequently and reliably
    wl_status_t wifiStatus = WiFi.status();
    static bool wasConnected = false;
    bool isConnected = (wifiStatus == WL_CONNECTED);
    
    // Detect connection state changes
    if (isConnected && !wasConnected) {
        // Just connected - show connected message
        Serial.println("WiFi connection detected!");
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
        
        // Show connected message
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 10);
        oled.print("WiFi CONNECTED!");
        oled.setCursor(0, 25);
        oled.print("SSID: ");
        oled.print(WiFi.SSID());
        oled.setCursor(0, 40);
        oled.print("IP: ");
        oled.print(WiFi.localIP().toString());
        oled.display();
        delay(2000);
    } else if (!isConnected && wasConnected) {
        // Just disconnected
        Serial.println("WiFi connection lost!");
    }
    
    // Update connection state
    wasConnected = isConnected;
    
    // Handle disconnected state
    if (!isConnected) {
        // If WiFi is disconnected, show message and try to reconnect
        displayNotConnectedMessage();
        
        // Try to reconnect every 5 seconds
        static unsigned long lastReconnectAttempt = 0;
        if (currentMillis - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = currentMillis;
            Serial.println("Attempting to reconnect WiFi...");
            
            // Flash the display to indicate reconnection attempt
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 25);
            oled.print("TRYING TO RECONNECT...");
            oled.display();
            delay(300);
            
            // Attempt to reconnect
            WiFi.reconnect();
            
            // Check if reconnection was successful
            for (int i = 0; i < 10; i++) { // Check multiple times
                delay(200);
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println("Successfully reconnected to WiFi!");
                    Serial.print("Connected to: ");
                    Serial.println(WiFi.SSID());
                    
                    // Show connected message
                    oled.clearDisplay();
                    oled.setTextSize(1);
                    oled.setCursor(0, 10);
                    oled.print("WiFi RECONNECTED!");
                    oled.setCursor(0, 25);
                    oled.print("SSID: ");
                    oled.print(WiFi.SSID());
                    oled.setCursor(0, 40);
                    oled.print("IP: ");
                    oled.print(WiFi.localIP().toString());
                    oled.display();
                    delay(2000);
                    break;
                }
            }
        }
        
        delay(500);  // Shorter delay to update display more frequently
        return;  // Skip the rest of the loop while disconnected
    }

    // Only proceed with normal operation if connected to WiFi
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        showDHTData = !showDHTData;
        
        // Also cycle to next city if a button is pressed
        // For example, if a button on D3 is pressed
        if (digitalRead(D3) == LOW) {
            // Get previous city for feedback
            String previousCity = cityName;
            int previousIndex = cityIndex;
            
            // Update city index
            cityIndex = (cityIndex + 1) % NUM_CITIES;
            cityName = cityList[cityIndex];
            
            // Save to EEPROM
            Serial.print("Changing city from ");
            Serial.print(previousCity);
            Serial.print(" (index ");
            Serial.print(previousIndex);
            Serial.print(") to ");
            Serial.print(cityName);
            Serial.print(" (index ");
            Serial.print(cityIndex);
            Serial.println(")");
            
            saveCityToEEPROM(cityIndex);
            
            // Now verify the save was successful by reading it back
            EEPROM.begin(EEPROM_SIZE);
            int verifiedIndex1 = EEPROM.read(CITY_ADDR1);
            int verifiedIndex2 = EEPROM.read(CITY_ADDR2);
            int verifiedIndex3 = EEPROM.read(CITY_ADDR3);
            int verifiedIndex4 = EEPROM.read(CITY_ADDR4);
            EEPROM.end();
            
            Serial.print("EEPROM verification after save: Read indexes = ");
            Serial.print(verifiedIndex1);
            Serial.print(", ");
            Serial.print(verifiedIndex2);
            Serial.print(", ");
            Serial.print(verifiedIndex3);
            Serial.print(", ");
            Serial.println(verifiedIndex4);
            
            bool saveSuccess = (verifiedIndex1 == cityIndex && 
                               verifiedIndex2 == cityIndex && 
                               verifiedIndex3 == cityIndex &&
                               verifiedIndex4 == cityIndex);
            
            // Show feedback on display
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 0);
            oled.print("City changed!");
            oled.setCursor(0, 12);
            oled.print("From: ");
            oled.print(previousCity);
            oled.setCursor(0, 24);
            oled.print("To: ");
            oled.print(cityName);
            oled.setCursor(0, 40);
            oled.print("Index saved: ");
            oled.print(cityIndex);
            oled.setCursor(0, 54);
            oled.print("Save status: ");
            oled.print(saveSuccess ? "SUCCESS" : "FAILED");
            oled.display();
            
            // If save failed, try again
            if (!saveSuccess) {
                delay(1000);
                Serial.println("EEPROM save verification failed, retrying...");
                saveCityToEEPROM(cityIndex);
            }
            
            // Wait a moment to show the message and debounce
            delay(3000);
        }
    }

    if (showDHTData) {
        getSensorValue();
        updateDisplay();
    } else {
        // Display weather data
        float lat, lon;
        getRomanianCityCoordinates(cityIndex, lat, lon);
        getWeather(lat, lon);
    }

    delay(2000);  // Short delay to prevent rapid screen updates
}

void getSensorValue() {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("DHT sensor failed");
    } else {
        Serial.print("DHT Temperature: ");
        Serial.println(temperature);
        Serial.print("DHT Humidity: ");
        Serial.println(humidity);
    }
}

void updateDisplay() {
    oled.clearDisplay();

    oled.setTextSize(1);
    oled.setCursor(0,0);
    oled.print("Temperature: ");
    oled.setTextSize(2);
    oled.setCursor(0,10);
    oled.print(temperature);
    oled.print(" ");
    oled.setTextSize(1);

    oled.cp437(true);

    oled.write(248);
    oled.setTextSize(2);
    oled.print("C");

    oled.setTextSize(1);
    oled.setCursor(0, 30);
    oled.print("Humidity: ");
    oled.setTextSize(2);
    oled.setCursor(0, 40);
    oled.print(humidity);
    oled.print(" %");

    oled.display();
}

void Text_EAEA(){
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(0,0);
    oled.write("Early");
    oled.setCursor(0,20);
    oled.write("Alpha's");
    oled.setCursor(0,40);
    oled.write("Kids");
    oled.setCursor(60,50);
    oled.setTextSize(1);
    oled.write("2k25 #eaea");
    oled.display();  // Display before delay
    delay(2000);
}

void getWeather(float lat, float lon) {
    WiFiClient client;
    
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // Use HTTP instead of HTTPS
        String url = "http://api.open-meteo.com/v1/forecast?latitude=" + 
                    String(lat, 6) + "&longitude=" + String(lon, 6) + 
                    "&current=temperature_2m,windspeed_10m";
        
        Serial.print("Weather URL: ");
        Serial.println(url);
        
        http.begin(client, url);
        
        int httpCode = http.GET();
        Serial.print("Weather HTTP code: ");
        Serial.println(httpCode);

        if (httpCode == 200) {
            String payload = http.getString();
            
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error && doc.containsKey("current")) {
                temperature = doc["current"]["temperature_2m"].as<float>();
                windspeed = doc["current"]["windspeed_10m"].as<float>();
                
                Serial.print("API Temperature: ");
                Serial.println(temperature);
                Serial.print("API Windspeed: ");
                Serial.println(windspeed);
                
                updateWeatherDisplay(temperature, windspeed);
            } else {
                // If API response parsing fails, use local sensor
                getSensorValue();
                updateDisplay();
            }
        } else {
            // If API call fails, use local sensor
            getSensorValue();
            updateDisplay();
        }
        http.end();
    }
}

void updateWeatherDisplay(float temperature, float windspeed) {
    oled.clearDisplay();

    // City name at the top
    oled.setTextSize(1);
    oled.setCursor(0,0);
    oled.print("City: ");
    oled.print(cityName);
    
    // Temperature with same format as updateDisplay
    oled.setTextSize(1);
    oled.setCursor(0,10);
    oled.print("Temperature: ");
    oled.setTextSize(2);
    oled.setCursor(0,20);
    oled.print(temperature);
    oled.print(" ");
    oled.setTextSize(1);

    oled.cp437(true);

    oled.write(248);
    oled.setTextSize(2);
    oled.print("C");

    // Wind speed with similar format
    oled.setTextSize(1);
    oled.setCursor(0, 37);
    oled.print("Wind: ");
    oled.setTextSize(2);
    oled.setCursor(0, 47);
    oled.print(windspeed);
    oled.print(" km/h");

    oled.display();
}

// Function to get city coordinates
bool getRomanianCityCoordinates(int cityIdx, float &lat, float &lon) {
    // Default to Bucharest
    lat = 44.4361414;
    lon = 26.1027202;
    
    // Validate index
    if (cityIdx < 0 || cityIdx >= NUM_CITIES) {
        return false;
    }
    
    switch (cityIdx) {
        case 0: // Bucharest
            lat = 44.4361414;
            lon = 26.1027202;
            break;
        case 1: // Cluj-Napoca
            lat = 46.7712;
            lon = 23.6236;
            break;
        case 2: // Timisoara
            lat = 45.7489;
            lon = 21.2087;
            break;
        case 3: // Iasi
            lat = 47.1585;
            lon = 27.6014;
            break;
        case 4: // Constanta
            lat = 44.1598;
            lon = 28.6348;
            break;
        case 5: // Craiova
            lat = 44.3302;
            lon = 23.7949;
            break;
        case 6: // Brasov
            lat = 45.6427;
            lon = 25.5887;
            break;
        case 7: // Galati
            lat = 45.4353;
            lon = 28.0480;
            break;
        case 8: // Ploiesti
            lat = 44.9436;
            lon = 26.0739;
            break;
        case 9: // Oradea
            lat = 47.0465;
            lon = 21.9189;
            break;
        case 10: // Sibiu
            lat = 45.7983;
            lon = 24.1256;
            break;
        case 11: // Arad
            lat = 46.1865;
            lon = 21.3123;
            break;
        case 12: // Pitesti
            lat = 44.8563;
            lon = 24.8691;
            break;
        case 13: // Baia Mare
            lat = 47.6567;
            lon = 23.5698;
            break;
        case 14: // Buzau
            lat = 45.1500;
            lon = 26.8166;
            break;
        case 15: // Satu Mare
            lat = 47.7921;
            lon = 22.8876;
            break;
        case 16: // Botosani
            lat = 47.7487;
            lon = 26.6670;
            break;
        case 17: // Ramnicu Valcea
            lat = 45.1047;
            lon = 24.3754;
            break;
        case 18: // Suceava
            lat = 47.6356;
            lon = 26.2502;
            break;
        case 19: // Piatra Neamt
            lat = 46.9259;
            lon = 26.3718;
            break;
        case 20: // Drobeta-Turnu Severin
            lat = 44.6314;
            lon = 22.6594;
            break;
        case 21: // Targu Mures
            lat = 46.5383;
            lon = 24.5577;
            break;
        case 22: // Targoviste
            lat = 44.9252;
            lon = 25.4598;
            break;
        case 23: // Focsani
            lat = 45.6965;
            lon = 27.1862;
            break;
        case 24: // Tulcea
            lat = 45.1871;
            lon = 28.8005;
            break;
        case 25: // Alba Iulia
            lat = 46.0697;
            lon = 23.5804;
            break;
        case 26: // Giurgiu
            lat = 43.9038;
            lon = 25.9699;
            break;
        case 27: // Hunedoara
            lat = 45.7545;
            lon = 22.9139;
            break;
        case 28: // Bistrita
            lat = 47.1304;
            lon = 24.4909;
            break;
        case 29: // Resita
            lat = 45.2971;
            lon = 21.8898;
            break;
        default:
            return false;
    }
    
    return true;
}

// Update the testEEPROM function
void testEEPROM() {
    Serial.println("\n========= EEPROM TESTING =========");
    Serial.println("Testing EEPROM functionality...");
    
    // Check if EEPROM is available
    EEPROM.begin(EEPROM_SIZE);
    
    // Read the magic byte from all locations to see if EEPROM has been initialized
    byte magic1 = EEPROM.read(MAGIC_ADDR1);
    byte magic2 = EEPROM.read(MAGIC_ADDR2);
    byte magic3 = EEPROM.read(MAGIC_ADDR3);
    byte magic4 = EEPROM.read(MAGIC_ADDR4);
    
    // Read indexes
    byte index1 = EEPROM.read(CITY_ADDR1);
    byte index2 = EEPROM.read(CITY_ADDR2);
    byte index3 = EEPROM.read(CITY_ADDR3);
    byte index4 = EEPROM.read(CITY_ADDR4);
    
    EEPROM.end();
    
    Serial.println("EEPROM CURRENT STATUS:");
    Serial.print("Magic bytes: ");
    Serial.print(magic1, HEX);
    Serial.print(", ");
    Serial.print(magic2, HEX);
    Serial.print(", ");
    Serial.print(magic3, HEX);
    Serial.print(", ");
    Serial.println(magic4, HEX);
    
    Serial.print("City indexes: ");
    Serial.print(index1);
    Serial.print(" (");
    Serial.print(index1 < NUM_CITIES ? cityList[index1] : "INVALID");
    Serial.print("), ");
    Serial.print(index2);
    Serial.print(" (");
    Serial.print(index2 < NUM_CITIES ? cityList[index2] : "INVALID");
    Serial.print("), ");
    Serial.print(index3);
    Serial.print(" (");
    Serial.print(index3 < NUM_CITIES ? cityList[index3] : "INVALID");
    Serial.print("), ");
    Serial.print(index4);
    Serial.print(" (");
    Serial.print(index4 < NUM_CITIES ? cityList[index4] : "INVALID");
    Serial.println(")");
    
    // If EEPROM hasn't been initialized properly, do it now
    bool needsInit = false;
    
    if (magic1 != EEPROM_MAGIC || magic2 != EEPROM_MAGIC || 
        magic3 != EEPROM_MAGIC || magic4 != EEPROM_MAGIC) {
        Serial.println("EEPROM not properly initialized (magic bytes don't match)");
        needsInit = true;
    }
    
    if (index1 >= NUM_CITIES || index2 >= NUM_CITIES || 
        index3 >= NUM_CITIES || index4 >= NUM_CITIES) {
        Serial.println("EEPROM contains invalid city indexes");
        needsInit = true;
    }
    
    // Check if indexes are consistent
    if (index1 != index2 || index1 != index3 || index1 != index4) {
        Serial.println("EEPROM data is inconsistent across locations");
        needsInit = true;
    }
    
    if (needsInit) {
        Serial.println("EEPROM needs initialization. Initializing now...");
        initializeEEPROMSafe();
    } else {
        Serial.println("EEPROM appears to be properly initialized and consistent.");
    }
    
    // Force a test write/read cycle to verify EEPROM is working
    Serial.println("Running EEPROM write/read test at test location (20)...");
    
    EEPROM.begin(EEPROM_SIZE);
    byte testValue = 0xA5; // 10100101 binary - a pattern easy to verify
    EEPROM.write(20, testValue);
    bool commitSuccess = EEPROM.commit();
    delay(100); // Wait for write to complete
    byte readValue = EEPROM.read(20);
    EEPROM.end();
    
    Serial.print("EEPROM test: Wrote 0x");
    Serial.print(testValue, HEX);
    Serial.print(", Read 0x");
    Serial.print(readValue, HEX);
    Serial.print(" (Success: ");
    Serial.print(testValue == readValue ? "YES" : "NO");
    Serial.println(")");
    
    if (testValue != readValue) {
        Serial.println("WARNING: EEPROM TEST FAILED! Hardware may be faulty!");
        // Use a visual indication of failure
        oled.clearDisplay();
        oled.setTextSize(2);
        oled.setCursor(0, 0);
        oled.print("EEPROM");
        oled.setCursor(0, 16);
        oled.print("ERROR!");
        oled.setTextSize(1);
        oled.setCursor(0, 40);
        oled.print("Memory may be faulty");
        oled.setCursor(0, 50);
        oled.print("Settings won't persist");
        oled.display();
        delay(5000);
    }
    
    Serial.println("========= EEPROM TEST COMPLETE =========\n");
}
