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

// Add this line to define the pull-up resistor pin
#define DHTPULLUP_PIN D5  // Using D5 to avoid conflict with button on D3

#define OLED_RESET -1 

// Simple EEPROM configuration
#define EEPROM_SIZE 32  // Size of EEPROM to use
#define EEPROM_MAGIC 0xAB  // Magic byte to verify EEPROM is initialized

// Single EEPROM location for city storage
#define CITY_ADDR 0
#define VERIFY_ADDR 1
#define MAGIC_ADDR 2

// List of supported cities (must match dropdown options)
const char* cityList[] = {
    "Bucharest", "Cluj-Napoca", "Timisoara", "Iasi", "Constanta",
    "Craiova", "Brasov", "Galati", "Ploiesti", "Oradea",
    "Sibiu", "Arad", "Pitesti", "Baia Mare", "Buzau",
    "Satu Mare", "Botosani", "Ramnicu Valcea", "Suceava", "Piatra Neamt",
    "Drobeta-Turnu Severin", "Targu Mures", "Targoviste", "Focsani", "Tulcea",
    "Alba Iulia", "Giurgiu", "Hunedoara", "Bistrita", "Resita",
    "Slobozia", "Alexandria", "Calarasi", "Vaslui", "Zalau", 
    "Miercurea Ciuc", "Sfantu Gheorghe", "Braila", "Deva"
};
const int NUM_CITIES = 39; // Updated count with added cities

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

void saveCityToEEPROMSafe(int index) {
    // Sanity check for valid index
    if (index < 0 || index >= NUM_CITIES) {
        Serial.println("ERROR: Attempt to save invalid city index, defaulting to 0");
        index = 0;
    }
    
    Serial.print("SAVING CITY INDEX TO EEPROM: ");
    Serial.print(index);
    Serial.print(" (");
    Serial.print(cityList[index]);
    Serial.println(")");
    
    // Begin EEPROM session
    EEPROM.begin(EEPROM_SIZE);
    
    // CLEAR city data first
    Serial.println("Clearing previous city data from EEPROM...");
    EEPROM.write(CITY_ADDR, 255);
    EEPROM.write(VERIFY_ADDR, 0);
    EEPROM.write(MAGIC_ADDR, 0);
    
    // Commit the clear operation
    EEPROM.commit();
    delay(50);  // Wait for clear to complete
    
    Serial.println("Writing new city data...");
    
    // Now write the new city data
    EEPROM.write(CITY_ADDR, index);
    EEPROM.write(VERIFY_ADDR, 255 - index);
    EEPROM.write(MAGIC_ADDR, EEPROM_MAGIC);
    
    // Commit changes
    bool success = EEPROM.commit();
    
    // Wait to ensure write completes
    delay(100);
    
    // Re-read value to verify
    int readBack = EEPROM.read(CITY_ADDR);
    
    // End EEPROM session
    EEPROM.end();
    
    // Verify the save was successful
    bool verified = (readBack == index);
    
    Serial.print("EEPROM SAVE VERIFICATION: Read=");
    Serial.print(readBack);
    Serial.print(" (Success: ");
    Serial.print(verified ? "YES" : "NO");
    Serial.println(")");
    
    Serial.print("Overall EEPROM commit successful: ");
    Serial.println(success ? "YES" : "NO");
    
    // If verification failed, try again
    if (!verified) {
        Serial.println("EEPROM VERIFICATION FAILED! Retrying after delay...");
        delay(500); // Wait longer before retry
        
        // Recursive call to try again
        saveCityToEEPROMSafe(index);
    } else {
        Serial.println("EEPROM WRITE VERIFIED!");
    }
}

int loadCityFromEEPROMSafe() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Read from storage location
    int index = EEPROM.read(CITY_ADDR);
    int verify = EEPROM.read(VERIFY_ADDR);
    int magic = EEPROM.read(MAGIC_ADDR);
    
    EEPROM.end();
    
    // Check if data is valid
    bool isValid = (index < NUM_CITIES && magic == EEPROM_MAGIC && verify == (255 - index));
    
    Serial.print("EEPROM LOAD: City index = ");
    Serial.print(index);
    Serial.print(" (");
    Serial.print(index < NUM_CITIES ? cityList[index] : "INVALID");
    Serial.print("), Valid: ");
    Serial.println(isValid ? "YES" : "NO");
    
    if (isValid) {
        Serial.print("Using city from EEPROM: ");
        Serial.print(cityList[index]);
        Serial.print(" (");
        Serial.print(index);
        Serial.println(")");
    } else {
        // Default to Bucharest if no valid data
        index = 0; // Bucharest
        Serial.println("No valid city found in EEPROM, defaulting to Bucharest (0)");
        
        // Save default city
        saveCityToEEPROMSafe(index);
    }
    
    return index;
}

// Function to initialize EEPROM with default values
void initializeEEPROMSafe() {
    Serial.println("Initializing EEPROM with default city (Bucharest)...");
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
        
        // Save with reliability
        saveCityToEEPROMSafe(cityIndex);
        
        // Double check it was saved correctly
        EEPROM.begin(EEPROM_SIZE);
        int saved = EEPROM.read(CITY_ADDR);
        EEPROM.end();
        
        Serial.print("Force-save verification: Read index = ");
        Serial.print(saved);
        Serial.print(" (Expected: ");
        Serial.print(cityIndex);
        Serial.println(")");
        
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
        oled.print(saved == cityIndex ? "YES" : "NO");
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
    
    // IMPORTANT: If you specifically want to force a city, uncomment this line:
    // Uncomment and modify to override the city selection
    // forceCity("Iasi"); // Example: force to Iasi
    
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
            Serial.print("Getting initial weather for: ");
            Serial.print(cityName);
            Serial.print(" at coordinates: ");
            Serial.print(lat, 6);
            Serial.print(", ");
            Serial.println(lon, 6);
            getWeather(lat, lon);
        } else {
            Serial.println("ERROR: Could not get coordinates for city");
        }
    }
    
    Serial.println("Setup complete");
}

void initializeDHTSensor() {
    // Set up the pull-up resistor pin
    pinMode(DHTPULLUP_PIN, OUTPUT);
    digitalWrite(DHTPULLUP_PIN, HIGH);  // Enable pull-up
    
    // Give the sensor time to stabilize before beginning
    delay(1000);
    dht.begin();
    
    // Allow sensor to initialize properly
    delay(2000);
    
    // First reading is often incorrect, so discard it
    dht.readTemperature();
    dht.readHumidity();
    delay(50);
    
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
    
    // Store previous city selection before configuration
    int previousCityIndex = cityIndex;
    String previousCityName = cityName;
    
    // Set initial city value in parameter - will be hidden but still used
    char cityIdxBuf[4];
    sprintf(cityIdxBuf, "%d", cityIndex);
    
    // Create the parameter for the city index - we'll hide this visually
    WiFiManagerParameter custom_city_param("city", "City Index", cityIdxBuf, 4);
    wm.addParameter(&custom_city_param);
    
    // Add CSS to hide the city index field
    String css = "<style>"
                "label[for='city'], input#city { display: none !important; }" // Hide the city index field
                "select { width: 100%; padding: 8px; margin: 10px 0; border-radius: 4px; border: 1px solid #ccc; }"
                "h3 { color: #1FA3EC; }"
                "</style>";
    WiFiManagerParameter custom_css(css.c_str());
    wm.addParameter(&custom_css);
    
    // Add a title
    String title = "<h3>Weather Station Configuration</h3>";
    WiFiManagerParameter custom_title(title.c_str());
    wm.addParameter(&custom_title);
    
    // Create a simple dropdown for city selection
    String citySelect = "<p><b>Select Your City:</b></p>"
                        "<select id='cityDropdown' onchange='document.getElementById(\"city\").value=this.value'>";
    
    // Add all cities as options
    for (int i = 0; i < NUM_CITIES; i++) {
        String selected = (i == cityIndex) ? " selected" : "";
        citySelect += "<option value='" + String(i) + "'" + selected + ">" + String(cityList[i]) + "</option>";
    }
    
    citySelect += "</select>";
    
    // Add the custom HTML
    WiFiManagerParameter custom_html(citySelect.c_str());
    wm.addParameter(&custom_html);
    
    // Add instruction text
    String instructions = "<p>Weather data will be shown for your selected city.</p>";
    WiFiManagerParameter custom_instructions(instructions.c_str());
    wm.addParameter(&custom_instructions);
    
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
        
        Serial.print("Selected city index from WiFi manager: ");
        Serial.println(newCityIdx);
        
        // Validate and save if changed
        if (newCityIdx >= 0 && newCityIdx < NUM_CITIES && newCityIdx != cityIndex) {
            // Clear out old city data before setting new city
            cityIndex = newCityIdx;
            cityName = cityList[cityIndex];
            
            // Save the new city index with the enhanced saving method
            saveCityToEEPROM(cityIndex);
            
            Serial.print("City changed from ");
            Serial.print(previousCityName);
            Serial.print(" (");
            Serial.print(previousCityIndex);
            Serial.print(") to ");
            Serial.print(cityName);
            Serial.print(" (");
            Serial.print(cityIndex);
            Serial.println(")");
            
            // Show city change message
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 0);
            oled.print("City changed!");
            oled.setCursor(0, 12);
            oled.print("From: ");
            oled.print(previousCityName);
            oled.setCursor(0, 24);
            oled.print("To: ");
            oled.print(cityName);
            oled.display();
            delay(2000);
        } else {
            Serial.println("City selection unchanged");
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

// This function handles the button press to change city
void handleCityChange() {
    // Get previous city for feedback
    String previousCity = cityName;
    int previousIndex = cityIndex;
    
    // Update city index
    cityIndex = (cityIndex + 1) % NUM_CITIES;
    cityName = cityList[cityIndex];
    
    // Save to EEPROM - clear old data first
    Serial.print("Changing city from ");
    Serial.print(previousCity);
    Serial.print(" (index ");
    Serial.print(previousIndex);
    Serial.print(") to ");
    Serial.print(cityName);
    Serial.print(" (index ");
    Serial.print(cityIndex);
    Serial.println(")");
    
    // Use the improved save function that clears first
    saveCityToEEPROM(cityIndex);
    
    // Now verify the save was successful by reading it back
    EEPROM.begin(EEPROM_SIZE);
    int verifiedIndex = EEPROM.read(CITY_ADDR);
    EEPROM.end();
    
    Serial.print("EEPROM verification after save: Read index = ");
    Serial.print(verifiedIndex);
    Serial.print(" (Expected: ");
    Serial.print(cityIndex);
    Serial.println(")");
    
    bool saveSuccess = (verifiedIndex == cityIndex);
    
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
    
    // ADDED: Immediately get weather for the new city after change
    if (WiFi.status() == WL_CONNECTED) {
        float lat, lon;
        if (getRomanianCityCoordinates(cityIndex, lat, lon)) {
            // Log city info for debugging
            Serial.print("Getting weather for new city ");
            Serial.print(cityName);
            Serial.print(" (index ");
            Serial.print(cityIndex);
            Serial.print(") at coordinates: ");
            Serial.print(lat, 6);
            Serial.print(", ");
            Serial.println(lon, 6);
            
            // Force an immediate update for the new city
            showDHTData = false; // Make sure we show weather data
            getWeather(lat, lon);
            delay(4000); // Show the new city weather a bit longer
        }
    }
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
        
        // Immediately get weather for the current city after connection
        float lat, lon;
        if (getRomanianCityCoordinates(cityIndex, lat, lon)) {
            // Log city info for debugging
            Serial.print("Getting weather for ");
            Serial.print(cityName);
            Serial.print(" (index ");
            Serial.print(cityIndex);
            Serial.print(") at coordinates: ");
            Serial.print(lat, 6);
            Serial.print(", ");
            Serial.println(lon, 6);
            
            getWeather(lat, lon);
        } else {
            Serial.println("Failed to get coordinates for selected city");
        }
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
            handleCityChange();
        }
    }

    if (showDHTData) {
        getSensorValue();
        updateDisplay();
    } else {
        // Display weather data - ALWAYS use the current cityIndex
        float lat, lon;
        if (getRomanianCityCoordinates(cityIndex, lat, lon)) {
            // Log city and coordinates for every API call
            Serial.print("API call for city: ");
            Serial.print(cityName);
            Serial.print(" (index ");
            Serial.print(cityIndex);
            Serial.print(") at coordinates: ");
            Serial.print(lat, 6);
            Serial.print(", ");
            Serial.println(lon, 6);
            
            getWeather(lat, lon);
        } else {
            Serial.println("ERROR: Failed to get coordinates for city index: " + String(cityIndex));
            // Fallback to sensor data if coordinates retrieval fails
            getSensorValue();
            updateDisplay();
        }
    }

    delay(2000);  // Short delay to prevent rapid screen updates
}

void getSensorValue() {
    // Take multiple readings and average them for more stable results
    float tempSum = 0;
    float humSum = 0;
    int validReadings = 0;
    
    // Try up to 3 readings to get valid data
    for (int i = 0; i < 3; i++) {
        float tempReading = dht.readTemperature();
        float humReading = dht.readHumidity();
        
        if (!isnan(tempReading) && !isnan(humReading)) {
            // Only include readings that are within reasonable range
            // DHT11 temperature range is typically 0-50°C
            if (tempReading >= 0 && tempReading <= 50) {
                tempSum += tempReading;
                humSum += humReading;
                validReadings++;
            }
        }
        
        // Wait between readings
        delay(50);
    }
    
    // Only update values if we got valid readings
    if (validReadings > 0) {
        temperature = tempSum / validReadings;
        humidity = humSum / validReadings;
        
        Serial.print("DHT Temperature (avg of ");
        Serial.print(validReadings);
        Serial.print(" readings): ");
        Serial.println(temperature);
        Serial.print("DHT Humidity: ");
        Serial.println(humidity);
    } else {
        Serial.println("DHT sensor failed to get valid readings");
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
        // Verify that we're using the current city
        Serial.println("\n--- WEATHER API CALL ---");
        Serial.print("Current city from memory: ");
        Serial.print(cityName);
        Serial.print(" (index ");
        Serial.print(cityIndex);
        Serial.println(")");
        
        Serial.print("Using coordinates: Lat=");
        Serial.print(lat, 6);
        Serial.print(", Lon=");
        Serial.println(lon, 6);
        
        // Verify coordinates match the city index
        float cityLat, cityLon;
        if (getRomanianCityCoordinates(cityIndex, cityLat, cityLon)) {
            if (abs(cityLat - lat) > 0.001 || abs(cityLon - lon) > 0.001) {
                // Coordinates don't match current city index - use correct ones
                Serial.println("WARNING: Coordinates don't match current city index!");
                Serial.print("Updating to use correct coordinates for ");
                Serial.println(cityName);
                lat = cityLat;
                lon = cityLon;
            } else {
                Serial.println("Coordinates match current city index");
            }
        }
        
        HTTPClient http;
        
        // Use HTTP instead of HTTPS
        String url = "http://api.open-meteo.com/v1/forecast?latitude=" + 
                    String(lat, 6) + "&longitude=" + String(lon, 6) + 
                    "&current=temperature_2m,windspeed_10m";
        
        Serial.print("Weather URL: ");
        Serial.println(url);
        Serial.print("Getting weather for: ");
        Serial.print(cityName);
        Serial.print(" (index ");
        Serial.print(cityIndex);
        Serial.println(")");
        
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
                Serial.println("API response parsing failed, using local sensor");
                getSensorValue();
                updateDisplay();
            }
        } else {
            // If API call fails, use local sensor
            Serial.println("API call failed, using local sensor");
            getSensorValue();
            updateDisplay();
        }
        http.end();
        Serial.println("--- END WEATHER API CALL ---\n");
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
        case 30: // Slobozia (Ialomița)
            lat = 44.5659;
            lon = 27.3629;
            break;
        case 31: // Alexandria (Teleorman)
            lat = 43.9701;
            lon = 25.3307;
            break;
        case 32: // Călărași (Călărași)
            lat = 44.2024;
            lon = 27.3329;
            break;
        case 33: // Vaslui
            lat = 46.6406;
            lon = 27.7276;
            break;
        case 34: // Zalau (Sălaj)
            lat = 47.1955;
            lon = 23.0572;
            break;
        case 35: // Miercurea Ciuc (Harghita)
            lat = 46.3570;
            lon = 25.8046;
            break;
        case 36: // Sfantu Gheorghe (Covasna)
            lat = 45.8679;
            lon = 25.7873;
            break;
        case 37: // Braila
            lat = 45.2692;
            lon = 27.9574;
            break;
        case 38: // Deva (the actual county capital of Hunedoara)
            lat = 45.8785;
            lon = 22.9199;
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
    
    // Read the storage location
    byte cityIndex = EEPROM.read(CITY_ADDR);
    byte verify = EEPROM.read(VERIFY_ADDR);
    byte magic = EEPROM.read(MAGIC_ADDR);
    
    EEPROM.end();
    
    Serial.println("EEPROM CURRENT STATUS:");
    Serial.print("Magic byte: ");
    Serial.println(magic, HEX);
    
    Serial.print("City index: ");
    Serial.print(cityIndex);
    Serial.print(" (");
    Serial.print(cityIndex < NUM_CITIES ? cityList[cityIndex] : "INVALID");
    Serial.println(")");
    
    Serial.print("Verify byte: ");
    Serial.print(verify);
    Serial.print(" (Should be: ");
    Serial.print(255 - cityIndex);
    Serial.println(")");
    
    // If EEPROM hasn't been initialized properly, do it now
    bool needsInit = false;
    
    if (magic != EEPROM_MAGIC) {
        Serial.println("EEPROM not properly initialized (magic byte doesn't match)");
        needsInit = true;
    }
    
    if (cityIndex >= NUM_CITIES) {
        Serial.println("EEPROM contains invalid city index");
        needsInit = true;
    }
    
    if (verify != (255 - cityIndex)) {
        Serial.println("EEPROM verification byte doesn't match");
        needsInit = true;
    }
    
    if (needsInit) {
        Serial.println("EEPROM needs initialization. Initializing now...");
        initializeEEPROMSafe();
    } else {
        Serial.println("EEPROM appears to be properly initialized.");
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