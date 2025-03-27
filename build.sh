#!/bin/bash
set -o errexit  # Exit on error
set -o pipefail # Exit if any command in a pipe fails
set -o nounset  # Exit on undefined variables

VERSION="1.0.0"

# Script name for usage info
SCRIPT_NAME=$(basename "$0")

# Default values
DEFAULT_PORT=""  # Empty to force user to specify or to auto-detect
DEFAULT_SKETCH="weather_station.ino"
DEFAULT_BOARD="esp8266:esp8266:nodemcu"  # Changed from generic to nodemcu which includes D3, D4, D5 pin definitions
DEFAULT_CPU="80"  # MHz
DEFAULT_UPLOAD_SPEED="115200"
DEFAULT_DEBUG_LEVEL="None____"  # Debug level for ESP8266 - correct value with underscores
DEFAULT_FLASH_SIZE="4M1M"  # 4MB Flash with 1MB SPIFFS
DEFAULT_BUILD_DIR="build"  # Build directory for output files
DEFAULT_CONFIG_FILE=".arduino-build.conf"  # Configuration file

# ANSI color codes for pretty output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to log messages with timestamp
log() {
    local level="$1"
    local message="$2"
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    local color=""
    
    case "$level" in
        "INFO")  color="$BLUE" ;;
        "SUCCESS") color="$GREEN" ;;
        "WARNING") color="$YELLOW" ;;
        "ERROR")   color="$RED" ;;
        *)        color="$NC" ;;
    esac
    
    echo -e "${color}[$timestamp] $level: $message${NC}"
}

# Function to display usage information
show_usage() {
    echo -e "${CYAN}ESP8266 Weather Station Build Script v$VERSION${NC}"
    echo -e "${BLUE}Description:${NC} Automates the building and uploading of ESP8266 Weather Station sketch"
    echo
    echo -e "${BLUE}Usage:${NC}"
    echo -e "  $SCRIPT_NAME [options]"
    echo
    echo -e "${BLUE}Options:${NC}"
    echo -e "  -p, --port PORT          Serial port (e.g., /dev/ttyUSB0, COM3)"
    echo -e "  -s, --sketch SKETCH      Path to sketch file (.ino)"
    echo -e "  -b, --board BOARD        Board name (default: $DEFAULT_BOARD)"
    echo -e "  -c, --cpu FREQ           CPU frequency in MHz (80, 160)"
    echo -e "  -u, --upload-speed BAUD  Upload speed (default: $DEFAULT_UPLOAD_SPEED)"
    echo -e "  -d, --debug LEVEL        Debug level (None, Basic, All)"
    echo -e "  -f, --flash-size SIZE    Flash size/layout (e.g., 4M1M, 4M2M)"
    echo -e "  -o, --output-dir DIR     Output directory for builds (default: $DEFAULT_BUILD_DIR)"
    echo -e "  -i, --install-libs       Install all required libraries"
    echo -e "  -n, --no-upload          Compile only, don't upload"
    echo -e "  -m, --monitor            Start serial monitor after upload"
    echo -e "  -r, --baud-rate RATE     Serial monitor baud rate (default: 115200)"
    echo -e "  -a, --auto-detect        Auto-detect port (uses the first available)"
    echo -e "  -v, --verbose            Verbose output"
    echo -e "  -l, --list-ports         List available serial ports"
    echo -e "  -z, --spiffs             Upload SPIFFS data folder"
    echo -e "  -e, --erase              Erase flash before uploading"
    echo -e "  -x, --export             Export compiled binary to .bin file"
    echo -e "  -t, --time-uploads       Show upload time statistics"
    echo -e "  -g, --config FILE        Use configuration file (default: $DEFAULT_CONFIG_FILE)"
    echo -e "  -w, --write-config       Write current settings to config file"
    echo -e "  -h, --help               Show this help message"
    echo -e "  --version                Show version information"
    echo
    echo -e "${BLUE}Examples:${NC}"
    echo -e "  $SCRIPT_NAME --port /dev/cu.usbserial-1410 --sketch weather_station.ino"
    echo -e "  $SCRIPT_NAME -p /dev/cu.usbserial-1410 -s weather_station.ino -c 160 -d Basic"
    echo -e "  $SCRIPT_NAME -p /dev/cu.usbserial-1410 -i -v"
    echo -e "  $SCRIPT_NAME -a -m -r 9600 -z"
    echo -e "  $SCRIPT_NAME -g my-project.conf"
}

# Function to check if command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Function to check for Arduino CLI and install if missing
check_arduino_cli() {
    if ! command_exists arduino-cli; then
        log "WARNING" "Arduino CLI not found. Attempting to install..."
        
        # Detect OS
        case "$(uname -s)" in
            Linux*)     OS="Linux";;
            Darwin*)    OS="macOS";;
            CYGWIN*|MINGW*|MSYS*) OS="Windows";;
            *)          OS="Unknown";;
        esac
        
        # Install based on OS
        if [ "$OS" = "Linux" ] || [ "$OS" = "macOS" ]; then
            ARDUINO_CLI_DIR="$PWD/bin"
            mkdir -p "$ARDUINO_CLI_DIR"
            curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$ARDUINO_CLI_DIR" sh
            export PATH="$ARDUINO_CLI_DIR:$PATH"
            log "INFO" "Arduino CLI installed to $ARDUINO_CLI_DIR"
        elif [ "$OS" = "Windows" ]; then
            log "ERROR" "Please install Arduino CLI manually: https://arduino.github.io/arduino-cli/installation/"
            exit 1
        else
            log "ERROR" "Unsupported OS. Please install Arduino CLI manually."
            exit 1
        fi
        
        # Verify the installation
        if command_exists arduino-cli || [ -f "$ARDUINO_CLI_DIR/arduino-cli" ]; then
            log "SUCCESS" "Arduino CLI installed successfully."
            # Make arduino-cli command available in this script
            if [ -f "$ARDUINO_CLI_DIR/arduino-cli" ]; then
                alias arduino-cli="$ARDUINO_CLI_DIR/arduino-cli"
                # Set ARDUINO_CLI variable to use the full path
                ARDUINO_CLI="$ARDUINO_CLI_DIR/arduino-cli"
            fi
        else
            log "ERROR" "Failed to install Arduino CLI. Please install manually."
            exit 1
        fi
    else
        # Arduino CLI is already in the PATH
        ARDUINO_CLI="arduino-cli"
    fi
}

# Function to auto-detect port
auto_detect_port() {
    log "INFO" "Auto-detecting serial port..."
    
    # Get the first connected board port
    local detected_port=$($ARDUINO_CLI board list 2>/dev/null | grep -v "Unknown" | awk 'NR>1 {print $1; exit}')
    
    if [ -n "$detected_port" ]; then
        log "SUCCESS" "Detected port: $detected_port"
        PORT="$detected_port"
    else
        log "ERROR" "No valid serial port detected. Please specify port manually with -p option."
        list_ports
        exit 1
    fi
}

# Function to list available serial ports
list_ports() {
    log "INFO" "Available serial ports:"
    $ARDUINO_CLI board list
}

# Function to install libraries
install_libraries() {
    log "INFO" "Installing required libraries..."
    
    # List of libraries from the log file
    declare -a LIBRARIES=(
        "DHT sensor library@1.4.6"
        "Adafruit Unified Sensor@1.1.15" 
        "Adafruit GFX Library@1.12.0" 
        "Adafruit BusIO@1.17.0"
        "Adafruit SSD1306@2.5.13"
        "WiFiManager@2.0.17"
        "ArduinoJson@7.3.0"
    )
    
    for lib in "${LIBRARIES[@]}"; do
        log "INFO" "Installing $lib..."
        $ARDUINO_CLI lib install "$lib" || log "WARNING" "Failed to install $lib"
    done
    
    # Make sure ESP8266 board is installed
    log "INFO" "Installing ESP8266 board package..."
    $ARDUINO_CLI core install esp8266:esp8266 || {
        log "ERROR" "Failed to install ESP8266 board package"
        exit 1
    }
    
    log "SUCCESS" "All libraries installed successfully"
}

# Function to validate port
validate_port() {
    if [ -z "$PORT" ]; then
        log "WARNING" "No port specified."
        
        # Ask if user wants to auto-detect
        read -p "Auto-detect port? (y/n): " choice
        case "$choice" in
            y|Y) auto_detect_port ;;
            *)
                list_ports
                read -p "Enter port name: " PORT
                if [ -z "$PORT" ]; then
                    log "ERROR" "No port specified"
                    exit 1
                fi
                ;;
        esac
    fi
}

# Function to read from config file
read_config_file() {
    local config_file="$1"
    
    if [ -f "$config_file" ]; then
        log "INFO" "Reading configuration from $config_file"
        source "$config_file"
    else
        log "WARNING" "Configuration file $config_file not found. Using defaults."
    fi
}

# Function to write to config file
write_config_file() {
    local config_file="$1"
    
    log "INFO" "Writing configuration to $config_file"
    cat > "$config_file" << EOF
# ESP8266 Weather Station Build Configuration
# Generated on $(date)

# Board settings
PORT="$PORT"
SKETCH_PATH="$SKETCH_PATH"
BOARD="$BOARD"
CPU="$CPU"
UPLOAD_SPEED="$UPLOAD_SPEED"
DEBUG_LEVEL="$DEBUG_LEVEL"
FLASH_SIZE="$FLASH_SIZE"

# Build settings
BUILD_DIR="$BUILD_DIR"
VERBOSE=$VERBOSE

# Monitor settings
MONITOR_BAUD_RATE="$MONITOR_BAUD_RATE"

# Options
NO_UPLOAD=$NO_UPLOAD
MONITOR=$MONITOR
UPLOAD_SPIFFS=$UPLOAD_SPIFFS
ERASE_FLASH=$ERASE_FLASH
EXPORT_BIN=$EXPORT_BIN
TIME_UPLOADS=$TIME_UPLOADS
EOF
    
    log "SUCCESS" "Configuration written to $config_file"
}

# Function to upload SPIFFS data
upload_spiffs() {
    local data_dir="data"
    
    # Check if data directory exists
    if [ ! -d "$data_dir" ]; then
        log "ERROR" "SPIFFS data directory not found: $data_dir"
        return 1
    fi
    
    log "INFO" "Building SPIFFS image..."
    $ARDUINO_CLI filesystem:upload $VERBOSITY -p "$PORT" --fqbn "$FQBN" "$SKETCH_PATH" || {
        log "ERROR" "Failed to upload SPIFFS data"
        return 1
    }
    
    log "SUCCESS" "SPIFFS data uploaded successfully"
    return 0
}

# Function to export binary
export_binary() {
    local sketch_name=$(basename "$SKETCH_PATH" .ino)
    local bin_dir="$BUILD_DIR/binaries"
    local timestamp=$(date "+%Y%m%d_%H%M%S")
    local bin_name="${sketch_name}_${timestamp}.bin"
    
    mkdir -p "$bin_dir"
    
    # Find the compiled binary
    local compiled_bin=$(find "$BUILD_DIR" -name "*.bin" | head -1)
    
    if [ -z "$compiled_bin" ]; then
        log "ERROR" "Compiled binary not found"
        return 1
    fi
    
    # Copy the binary with a timestamp
    cp "$compiled_bin" "$bin_dir/$bin_name"
    
    log "SUCCESS" "Binary exported to $bin_dir/$bin_name"
    
    # Create a symlink to the latest bin
    ln -sf "$bin_name" "$bin_dir/${sketch_name}_latest.bin"
    log "INFO" "Latest binary symlink created: $bin_dir/${sketch_name}_latest.bin"
    
    return 0
}

# Function to start serial monitor
start_monitor() {
    log "INFO" "Starting serial monitor at $MONITOR_BAUD_RATE baud..."
    $ARDUINO_CLI monitor -p "$PORT" -c baudrate=$MONITOR_BAUD_RATE
}

# Function to erase flash
erase_flash() {
    log "INFO" "Erasing flash memory..."
    
    if command_exists esptool.py; then
        esptool.py --port "$PORT" erase_flash || {
            log "ERROR" "Failed to erase flash"
            return 1
        }
    else
        log "WARNING" "esptool.py not found. Installing..."
        pip install esptool || {
            log "ERROR" "Failed to install esptool"
            return 1
        }
        
        esptool.py --port "$PORT" erase_flash || {
            log "ERROR" "Failed to erase flash"
            return 1
        }
    fi
    
    log "SUCCESS" "Flash memory erased"
    # Delay to allow the board to reset
    sleep 2
    return 0
}

# Check for Arduino CLI
check_arduino_cli

# Parse command line arguments
INSTALL_LIBS=false
NO_UPLOAD=false
VERBOSE=false
MONITOR=false
UPLOAD_SPIFFS=false
ERASE_FLASH=false
EXPORT_BIN=false
TIME_UPLOADS=false
AUTO_DETECT=false
WRITE_CONFIG=false
CONFIG_FILE="$DEFAULT_CONFIG_FILE"

PORT="$DEFAULT_PORT"
SKETCH_PATH="$DEFAULT_SKETCH"
BOARD="$DEFAULT_BOARD"
CPU="$DEFAULT_CPU"
UPLOAD_SPEED="$DEFAULT_UPLOAD_SPEED"
DEBUG_LEVEL="$DEFAULT_DEBUG_LEVEL"
FLASH_SIZE="$DEFAULT_FLASH_SIZE"
BUILD_DIR="$DEFAULT_BUILD_DIR"
MONITOR_BAUD_RATE="115200"

# First, check for config file parameter
for (( i=1; i<=$#; i++ )); do
    if [[ "${!i}" == "-g" || "${!i}" == "--config" ]]; then
        if (( i+1 <= $# )); then
            CONFIG_FILE="${!((i+1))}"
            if [ -f "$CONFIG_FILE" ]; then
                read_config_file "$CONFIG_FILE"
            fi
        fi
    fi
done

# Now parse other command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -s|--sketch)
            SKETCH_PATH="$2"
            shift 2
            ;;
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        -c|--cpu)
            CPU="$2"
            shift 2
            ;;
        -u|--upload-speed)
            UPLOAD_SPEED="$2"
            shift 2
            ;;
        -d|--debug)
            DEBUG_LEVEL="$2"
            shift 2
            ;;
        -f|--flash-size)
            FLASH_SIZE="$2"
            shift 2
            ;;
        -o|--output-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -r|--baud-rate)
            MONITOR_BAUD_RATE="$2"
            shift 2
            ;;
        -i|--install-libs)
            INSTALL_LIBS=true
            shift
            ;;
        -n|--no-upload)
            NO_UPLOAD=true
            shift
            ;;
        -m|--monitor)
            MONITOR=true
            shift
            ;;
        -a|--auto-detect)
            AUTO_DETECT=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -l|--list-ports)
            list_ports
            exit 0
            ;;
        -z|--spiffs)
            UPLOAD_SPIFFS=true
            shift
            ;;
        -e|--erase)
            ERASE_FLASH=true
            shift
            ;;
        -x|--export)
            EXPORT_BIN=true
            shift
            ;;
        -t|--time-uploads)
            TIME_UPLOADS=true
            shift
            ;;
        -g|--config)
            # Already handled above
            shift 2
            ;;
        -w|--write-config)
            WRITE_CONFIG=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        --version)
            echo "ESP8266 Weather Station Build Script v$VERSION"
            exit 0
            ;;
        *)
            log "ERROR" "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Auto-detect port if requested
if [ "$AUTO_DETECT" = true ]; then
    auto_detect_port
fi

# Build the fqbn (fully qualified board name) with parameters
# Base FQBN is board:framework:model
BOARD_OPTIONS=""

# Add board-specific options with commas between them
[ -n "$CPU" ] && BOARD_OPTIONS="${BOARD_OPTIONS}xtal=${CPU},"
[ -n "$DEBUG_LEVEL" ] && BOARD_OPTIONS="${BOARD_OPTIONS}lvl=${DEBUG_LEVEL},"
[ -n "$FLASH_SIZE" ] && BOARD_OPTIONS="${BOARD_OPTIONS}eesz=${FLASH_SIZE},"
[ -n "$UPLOAD_SPEED" ] && BOARD_OPTIONS="${BOARD_OPTIONS}baud=${UPLOAD_SPEED},"

# Remove trailing comma if exists
BOARD_OPTIONS=$(echo "$BOARD_OPTIONS" | sed 's/,$//')

# Construct the full FQBN
if [ -n "$BOARD_OPTIONS" ]; then
    FQBN="${BOARD}:${BOARD_OPTIONS}"
else
    FQBN="${BOARD}"
fi

# Show configuration
log "INFO" "Weather Station Build Configuration:"
echo -e "  Port:          ${GREEN}$PORT${NC}"
echo -e "  Sketch:        ${GREEN}$SKETCH_PATH${NC}"
echo -e "  Board:         ${GREEN}$BOARD${NC}"
echo -e "  CPU:           ${GREEN}$CPU MHz${NC}"
echo -e "  Upload Speed:  ${GREEN}$UPLOAD_SPEED${NC}"
echo -e "  Debug Level:   ${GREEN}$DEBUG_LEVEL${NC}"
echo -e "  Flash Size:    ${GREEN}$FLASH_SIZE${NC}"
echo -e "  Build Dir:     ${GREEN}$BUILD_DIR${NC}"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Check if sketch file exists
if [ ! -f "$SKETCH_PATH" ]; then
    log "ERROR" "Sketch file not found: $SKETCH_PATH"
    exit 1
fi

# Write config if requested
if [ "$WRITE_CONFIG" = true ]; then
    write_config_file "$CONFIG_FILE"
fi

# Install libraries if requested
if [ "$INSTALL_LIBS" = true ]; then
    install_libraries
fi

# Set verbosity flag
VERBOSITY=""
if [ "$VERBOSE" = true ]; then
    VERBOSITY="--verbose"
fi

# Erase flash if requested
if [ "$ERASE_FLASH" = true ]; then
    validate_port
    erase_flash
fi

# Calculate MD5 of the sketch before building
SKETCH_MD5=$(md5 -q "$SKETCH_PATH" 2>/dev/null || md5sum "$SKETCH_PATH" | awk '{print $1}')
log "INFO" "Sketch MD5: $SKETCH_MD5"

# Build the project
log "INFO" "Building sketch..."
BUILD_START=$(date +%s)

if $ARDUINO_CLI compile $VERBOSITY --fqbn "$FQBN" --build-path "$BUILD_DIR" "$SKETCH_PATH"; then
    BUILD_END=$(date +%s)
    BUILD_TIME=$((BUILD_END - BUILD_START))
    log "SUCCESS" "Build completed in $BUILD_TIME seconds"
    
    # Calculate sketch size
    if [ -f "$BUILD_DIR/$(basename "$SKETCH_PATH" .ino).ino.bin" ]; then
        SKETCH_SIZE=$(ls -lh "$BUILD_DIR/$(basename "$SKETCH_PATH" .ino).ino.bin" | awk '{print $5}')
        log "INFO" "Sketch size: $SKETCH_SIZE"
    fi
else
    log "ERROR" "Build failed!"
    exit 1
fi

# Export binary if requested
if [ "$EXPORT_BIN" = true ]; then
    export_binary
fi

# Upload the project if requested
if [ "$NO_UPLOAD" = false ]; then
    validate_port
    log "INFO" "Uploading sketch to $PORT..."
    
    UPLOAD_START=$(date +%s)
    
    if $ARDUINO_CLI upload $VERBOSITY -p "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR" "$SKETCH_PATH"; then
        UPLOAD_END=$(date +%s)
        UPLOAD_TIME=$((UPLOAD_END - UPLOAD_START))
        
        if [ "$TIME_UPLOADS" = true ]; then
            log "SUCCESS" "Upload completed in $UPLOAD_TIME seconds"
        else
            log "SUCCESS" "Upload completed"
        fi
    else
        log "ERROR" "Upload failed!"
        exit 1
    fi
    
    # Upload SPIFFS if requested
    if [ "$UPLOAD_SPIFFS" = true ]; then
        upload_spiffs
    fi
fi

# Start serial monitor if requested
if [ "$MONITOR" = true ] && [ "$NO_UPLOAD" = false ]; then
    start_monitor
fi

log "SUCCESS" "All operations completed successfully!" 