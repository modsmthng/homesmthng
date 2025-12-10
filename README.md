# homesmthng
Turns the LilyGO 2.1 T-RGB display into a smart switch dashboard for HomeKit with HomeSpan.

# Prerequisites (Hardware & Software)
- Display	: LilyGo T-RGB (480x480 ESP32-S3)
- IDE/Framework:	Arduino IDE or VS Code (PlatformIO)
- Board Support: esp32 Board Manager (minimum version 2.0.x)

# Libraries (Crucial for easy installation)
- HomeSpan
- LilyGo T-RGB
- SensorLib, install version greater than v0.2.3
- lvgl, install version v8.3.11
- GFX Library for Arduino, install version v1.4.2
- TFT_eSPI, install version v2.5.22
- (LV_Helper)

# For Arduino IDE: 
- Arduino IDE Setting	Value
- Board	ESP32S3 Dev Module
- Port	Your port
- USB CDC On Boot	Enable
- CPU Frequency	240MHZ(WiFi)
- Core Debug Level	None
- USB DFU On Boot	Disable
- Erase All Flash Before Sketch Upload	Disable
- Events Run On	Core1
- Flash Mode	QIO 80MHZ
- Flash Size	16MB(128Mb)
- Arduino Runs On	Core1
- USB Firmware MSC On Boot	Disable
- Partition Scheme	16M Flash(3M APP/9.9MB FATFS)
- PSRAM	OPI PSRAM
- Upload Mode	UART0/Hardware CDC
- Upload Speed	921600
- USB Mode	CDC and JTAG

# Post-Flash Setup
1. Wi-Fi Setup: Wait for the Wi-Fi setup mode to start (this happens automatically if no credentials are found). Connect your phone or computer to the temporary access point named HOMEsmthng.

2. HomeKit Pairing:

	- Open the Apple Home App.

	- Add a new Accessory and scan or manually enter the HomeKit Pairing Code.

	- The Pairing Code is displayed on Tile 4 (swipe down twice from the main screen).


#DISCLAIMER
The code has been optimized and commented using AI.


## License
This project (code and documentation) is released under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)** License.

This means:
* You are free to share and adapt the code.
* You must give appropriate credit to the original author (Attribution).
* Use for commercial purposes is **not** permitted (NonCommercial).
* All derivatives (adaptations/developments) must be published under the **same license** as the original (ShareAlike).

To view a copy of this license, visit:
https://creativecommons.org/licenses/by-nc-sa/4.0/

---
