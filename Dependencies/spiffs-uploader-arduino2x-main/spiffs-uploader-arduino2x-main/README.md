# ESP32 SPIFFS Upload Tool - Arduino IDE 2.x Integration

A modern, flexible command-line tool for uploading files to ESP32 SPIFFS (SPI Flash File System) partitions. This tool integrates seamlessly with Arduino IDE 2.x and provides four convenient upload methods: **Serial Upload**, **OTA Upload**, **Image Creation Only**, and **VS Code Tasks**.

## ✨ Features

- 🚀 **Four Upload Methods**: Serial, OTA (Over-The-Air), Image-only, or VS Code Tasks
- ⚙️ **JSON Configuration**: Easy project configuration with `esp32_spiffs_config.json`
- 🔧 **Arduino IDE 2.x Integration**: Works with modern Arduino IDE and VS Code
- 📁 **Flexible Data Directory**: Upload any folder structure to SPIFFS
- 🛠️ **Auto Tool Detection**: Automatically finds Arduino tools and ESP32 core
- 📊 **Multiple Board Support**: esp32dev, esp32s2, esp32s3, esp32c3, and more
- 🖥️ **Cross-Platform**: Windows, macOS, and Linux support
- 📝 **Verbose Logging**: Detailed output for debugging

## 🔧 Prerequisites

### Required Software
- **Python 3.6+** - Download from [python.org](https://python.org)
- **Arduino IDE 2.x** - Download from [arduino.cc](https://arduino.cc)
- **ESP32 Arduino Core** - Install via Arduino IDE Boards Manager

### Hardware
- ESP32 development board (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.)
- USB cable for serial communication
- (Optional) WiFi network for OTA uploads

## 🚀 Four Upload Methods

This tool provides four flexible ways to work with your ESP32 SPIFFS data:

### 1. 📡 Serial Upload (Default)
Uploads files directly to your ESP32 via USB/Serial connection.

**Use when:**
- First-time setup
- Development and testing
- ESP32 is connected via USB
- Most reliable method

**Command:**
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json
```

**Batch script:**
```bash
upload_spiffs.bat
```

### 2. 🌐 OTA Upload (Over-The-Air)
Uploads files wirelessly to your ESP32 over WiFi network.

**Use when:**
- ESP32 is deployed remotely
- Wireless updates needed
- ESP32 has OTA enabled firmware
- Faster than serial for large files

**Command:**
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json --ota
```

**Batch script:**
```bash
upload_spiffs_ota.bat
```

**Requirements:**
- ESP32 must be connected to WiFi
- ESP32 firmware must support OTA updates
- Configure IP address in `esp32_spiffs_config.json`

### 3. 💾 Image Creation Only
Creates SPIFFS image file without uploading (useful for custom deployment).

**Use when:**
- Creating firmware images for production
- Custom flashing procedures
- Backup/archive purposes
- Integration with other tools

**Command:**
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json --no-upload
```

**VS Code Task:**
Use the "ESP32 SPIFFS Create Image Only (Config)" task from the command palette.

### 4. 🎯 VS Code Tasks (Recommended)
Use built-in VS Code tasks for the most integrated development experience.

**Use when:**
- Working in VS Code environment
- Want integrated terminal output
- Need quick access to all upload methods
- Prefer GUI over command line

**How to use:**
1. Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
2. Type "Tasks: Run Task"
3. Select your desired ESP32 SPIFFS task:
   - **ESP32 SPIFFS Upload (Config)** - Serial upload
   - **ESP32 SPIFFS Upload (Config + OTA)** - OTA upload  
   - **ESP32 SPIFFS Create Image Only (Config)** - Image creation only
   - **ESP32 Config - Edit** - Opens config file for editing

**Benefits:**
- ✅ Integrated with VS Code terminal
- ✅ No need to remember command syntax
- ✅ Quick access via Command Palette
- ✅ Shared terminal panel for all tasks
- ✅ Professional development workflow

## ⚙️ Configuration File (`esp32_spiffs_config.json`)

The heart of this tool is the JSON configuration file that defines your project settings. This eliminates the need for complex command-line arguments and makes the tool easy to integrate into your workflow.

### 📝 Configuration Structure

```json
{
  "project": {
    "name": "ESP32_SPIFFS_Project",
    "description": "ESP32 project with SPIFFS file system",
    "version": "1.0.0"
  },
  "upload": {
    "port": "COM3",
    "baud_rate": 921600,
    "data_directory": "./data",
    "partition_name": "spiffs",
    "board": "esp32dev"
  },
  "board_settings": {
    "flash_mode": "dio",
    "flash_frequency": "80m",
    "flash_size": "4MB"
  },
  "ota": {
    "enabled": true,
    "ip_address": "192.168.1.100",
    "port": 3232,
    "password": ""
  },
  "development": {
    "verbose": true,
    "auto_detect_tools": true,
    "backup_before_upload": false
  },
  "advanced": {
    "partition_file": "",
    "custom_mkspiffs_args": [],
    "custom_esptool_args": [],
    "timeout_seconds": 60
  }
}
```

### 🔧 Configuration Sections Explained

#### **Project Section**
- `name`: Your project identifier
- `description`: Project description
- `version`: Project version for tracking

#### **Upload Section** ⚡
- `port`: Serial port (e.g., "COM3", "/dev/ttyUSB0") or IP address for OTA
- `baud_rate`: Upload speed (921600 recommended for fastest uploads)
- `data_directory`: Path to folder containing files to upload (default: "./data")
- `partition_name`: SPIFFS partition name (usually "spiffs")
- `board`: ESP32 board type (esp32dev, esp32s2, esp32s3, esp32c3, etc.)

#### **Board Settings Section** 🔌
- `flash_mode`: Flash interface mode (dio, dout, qio, qout)
- `flash_frequency`: Flash clock frequency (80m, 40m, 26m, 20m)
- `flash_size`: Total flash size (4MB, 8MB, 16MB, etc.)

#### **OTA Section** 📡
- `enabled`: Enable/disable OTA functionality
- `ip_address`: ESP32 IP address for wireless uploads
- `port`: OTA port (default: 3232)
- `password`: OTA password (if required by your firmware)

#### **Development Section** 🛠️
- `verbose`: Enable detailed logging output
- `auto_detect_tools`: Automatically find Arduino tools
- `backup_before_upload`: Create backup before uploading (future feature)

#### **Advanced Section** ⚙️
- `partition_file`: Custom partition table CSV file path
- `custom_mkspiffs_args`: Additional arguments for mkspiffs tool
- `custom_esptool_args`: Additional arguments for esptool
- `timeout_seconds`: Upload timeout duration

### 🎯 Quick Configuration Tips

1. **Find Your Serial Port:**
   - **Windows**: Check Device Manager or Arduino IDE Serial Monitor
   - **macOS/Linux**: Use `ls /dev/tty*` or Arduino IDE Serial Monitor

2. **OTA IP Address:**
   - Check your router's admin panel
   - Use Arduino Serial Monitor to print `WiFi.localIP()`
   - Use network scanner tools

3. **Board Types:**
   - `esp32dev`: Standard ESP32 (most common)
   - `esp32s2`: ESP32-S2 boards
   - `esp32s3`: ESP32-S3 boards  
   - `esp32c3`: ESP32-C3 boards

## 🚀 Quick Start Guide

### 1. Setup Your Project
1. Copy the tool files to your Arduino project directory
2. Create a `data` folder in your project directory
3. Add your web files (HTML, CSS, JS, images) to the `data` folder

### 2. Configure the Tool
1. Edit `esp32_spiffs_config.json` with your settings:
   - Set correct serial port (`"port": "COM3"`)
   - Set your board type (`"board": "esp32dev"`)
   - Configure OTA IP if using wireless upload

### 3. Choose Your Upload Method

**Option A: VS Code Tasks (Recommended for VS Code users)**
1. Press `Ctrl+Shift+P` → "Tasks: Run Task"
2. Select "ESP32 SPIFFS Upload (Config)" for serial upload
3. Or select "ESP32 SPIFFS Upload (Config + OTA)" for OTA upload

**Option B: Serial Upload (Command line/batch)**
```bash
upload_spiffs.bat
```

**Option C: OTA Upload (Command line/batch)**
```bash
upload_spiffs_ota.bat
```

**Option D: Direct Command Line**
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json
```

## 🛠️ VS Code Integration (Recommended Method)

This tool includes comprehensive VS Code tasks for the most seamless development experience. The `.vscode/tasks.json` file provides four pre-configured tasks:

### 📋 Available VS Code Tasks

| Task Name | Description | Upload Method |
|-----------|-------------|---------------|
| **ESP32 SPIFFS Upload (Config)** | Serial upload using config file | Serial/USB |
| **ESP32 SPIFFS Upload (Config + OTA)** | OTA upload using config file | WiFi/OTA |
| **ESP32 SPIFFS Create Image Only (Config)** | Create image without uploading | Image Only |
| **ESP32 Config - Edit** | Opens configuration file for editing | Configuration |

### 🎯 How to Run VS Code Tasks

#### Method 1: Command Palette (Recommended)
1. Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
2. Type "Tasks: Run Task"
3. Select your desired ESP32 SPIFFS task from the list

#### Method 2: Terminal Menu
1. Go to **Terminal** → **Run Task...**
2. Select your desired ESP32 SPIFFS task

#### Method 3: Keyboard Shortcut
1. Press `Ctrl+Shift+P` and type "Tasks: Configure Default Build Task"
2. Select an ESP32 task as default
3. Use `Ctrl+Shift+B` to run the default build task

### ⚙️ Task Configuration Details

All tasks are configured with:
- **Shared terminal panel** - All output appears in the same terminal
- **Auto-reveal terminal** - Terminal automatically shows when task runs
- **Echo commands** - Shows the exact command being executed
- **Build group** - Organized under build tasks category

**Example task structure:**
```json
{
    "label": "ESP32 SPIFFS Upload (Config)",
    "type": "shell",
    "command": "python",
    "args": [
        "${workspaceFolder}/esp32_spiffs_upload.py",
        "--config", "${workspaceFolder}/esp32_spiffs_config.json"
    ],
    "group": "build",
    "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
    }
}
```

### 🚀 VS Code Workflow Tips

1. **Quick Config Editing**: Use "ESP32 Config - Edit" task to quickly open and modify your configuration
2. **Development Cycle**: Use "Create Image Only" during development to test without uploading
3. **Production Deployment**: Use serial upload for initial deployment, then switch to OTA for updates
4. **Terminal Integration**: All task output appears in VS Code's integrated terminal with syntax highlighting

## 📁 Data Directory Structure

Your `data` folder can contain any file structure you need:

```
data/
├── index.html          # Main web page
├── style.css           # Stylesheets
├── script.js           # JavaScript files
├── images/
│   ├── logo.png        # Images
│   └── background.jpg
├── config/
│   └── settings.json   # Configuration files
└── fonts/
    └── custom.woff     # Font files
```

**Supported File Types:**
- Web files: HTML, CSS, JS, JSON
- Images: PNG, JPG, GIF, SVG, ICO
- Documents: TXT, XML, CSV
- Fonts: WOFF, TTF, OTF
- Any binary or text file

## 🔍 Troubleshooting

### Common Issues and Solutions

**❌ "Configuration file not found"**
- Ensure `esp32_spiffs_config.json` exists in your project directory
- Check file name spelling and extension

**❌ "Port not found" or "Access denied"**
- Close Arduino IDE Serial Monitor
- Check if another program is using the port
- Verify correct port name in config file
- On Linux/macOS: Check port permissions

**❌ "ESP32 tools not found"**
- Install ESP32 Arduino Core via Arduino IDE Boards Manager
- Restart VS Code/command prompt after installation
- Check Arduino IDE installation path

**❌ "OTA upload failed"**
- Verify ESP32 is connected to WiFi
- Check IP address in configuration
- Ensure ESP32 firmware supports OTA
- Check firewall settings

**❌ "mkspiffs not found"**
- ESP32 Arduino Core not properly installed
- Try reinstalling ESP32 core in Arduino IDE
- Restart Arduino IDE after installation

### 🔧 Debug Mode

Enable verbose logging for detailed troubleshooting:

```json
{
  "development": {
    "verbose": true
  }
}
```

Or use command line:
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json --verbose
```

## 📚 Command Line Reference

### Basic Usage
```bash
python esp32_spiffs_upload.py [options]
```

### Common Options
- `--config FILE` - Use JSON configuration file
- `--data-dir PATH` - Data directory path (default: ./data)
- `--port PORT` - Serial port or IP address
- `--board BOARD` - Board type (default: esp32dev)
- `--ota` - Use OTA upload instead of serial
- `--no-upload` - Create image only, don't upload
- `--verbose` - Enable verbose output

### Examples

**Serial upload with config:**
```bash
python esp32_spiffs_upload.py --config esp32_spiffs_config.json
```

**OTA upload to specific IP:**
```bash
python esp32_spiffs_upload.py --port 192.168.1.100 --ota
```

**Create image only:**
```bash
python esp32_spiffs_upload.py --data-dir ./web_files --no-upload
```

**Custom board and port:**
```bash
python esp32_spiffs_upload.py --board esp32s3 --port COM5 --data-dir ./data
```

## 🎯 Best Practices

### 📁 File Organization
- Keep your `data` folder organized with logical subdirectories
- Use descriptive filenames for easier debugging
- Compress large files (images, etc.) to save SPIFFS space
- Remove unnecessary files before upload

### 🔧 Configuration Management
- Use version control for your configuration files
- Create different config files for development/production
- Document any custom settings for team members
- Test configuration changes in development first

### 🚀 Performance Tips
- Use highest baud rate your system supports (921600)
- OTA uploads are often faster for large file sets
- Create image-only builds for faster iteration during development
- Monitor SPIFFS usage to avoid filling the partition

### 🔒 Security Considerations
- Don't include sensitive data in SPIFFS files
- Use OTA passwords in production environments
- Secure your WiFi network for OTA uploads
- Consider encrypted storage for sensitive configuration

## 📄 License

This project is open source. Feel free to modify and distribute according to your needs.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

---

**Happy coding with ESP32 SPIFFS! 🚀**

