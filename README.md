LilyGo T-Display S3 AMOLED + BME680 Environmental Monitor

This project displays real-time air quality and environmental data on a LilyGo T-Display S3 AMOLED screen using a BME680 environmental sensor. The user interface is written using LVGL and designed in SquareLine Studio.

## Features

- Real-time environmental data including IAQ (Air Quality Index), temperature, humidity, and pressure
- Touchscreen UI with gesture-based navigation between screens
- AQI history graph with basic filtering and smoothing for stable representation
- IAQ-based status indicator with visual pointer on a color-coded bar
- Data filtering to avoid spikes and invalid values
- NTP-based real-time clock display (12-hour format with AM/PM and day/date)
- Calibration state management for BSEC to speed up warm starts
- Optimized memory usage to fit on ESP32-S3 flash

## Screens

- Screen 1: Live temperature, humidity, pressure, date, and IAQ value
- Screen 2: AQI bar with a moving triangle to indicate current level, history graph over last several hours
- Additional screens reserved for future expansion (weather, settings, etc.)

## Hardware Requirements

- LilyGo T-Display S3 AMOLED Plus (ESP32-S3, 536x240)
- Bosch BME680 sensor (I2C at 0x76 or 0x77)
- CST816 touch controller (built into T-Display)
- USB-C cable for flashing and powering device

## Software Stack

- Arduino framework
- LVGL 8.3.11 for UI
- Bosch BSEC2 library for environmental calculations
- LilyGo AMOLED library for display/touch handling
- Preferences for storing BSEC calibration state
- WiFi and time.h for NTP time sync

## Main Code Components

### 1. Sensor Filtering

- Ignore first 2 minutes or until BSEC accuracy >= 2
- Reject values below 0 or above 500
- Ignore sudden jumps > 60 between readings
- Apply weighted moving average to stabilize data

Example smoothing logic:
```cpp
  smoothIAQ = (smoothIAQ * 0.7f) + (iaq * 0.3f);
````

### 2. AQI History Graph

* Displays 96 recent AQI samples (about 8 hours at 5-minute intervals)
* Uses LVGL chart widget in shift update mode to push data in from the right
* Filtered values only (no invalid data)

### 3. Visual AQI Pointer

Triangle image on the AQI bar moves based on AQI:

```cpp
int16_t x = map(iaq, 0, 300, -40, 230);
lv_obj_set_x(ui_AQIArrow, x);
lv_obj_set_y(ui_AQIArrow, -22);
```

### 4. Time and Date

* WiFi connects on boot and updates system RTC via NTP
* Time updates every second
* 12-hour format with AM/PM, plus day (e.g. MON) and date (e.g. 14 Jan)

## Build Instructions

1. Clone repo and open firmware folder in Arduino IDE or PlatformIO
2. Install required libraries:

   * LVGL 8.3.11
   * LilyGo AMOLED
   * bsec2
3. Add your WiFi credentials in the code
4. Flash to ESP32-S3 board with correct settings ("LilyGo T-Display S3 AMOLED")
5. Reset the board and watch it calibrate. After around 2–10 minutes, IAQ values will stabilize

## Resetting Calibration

The BSEC baseline is stored using the Preferences library. To force recalibration:

* Call `prefs.clear()` in setup once
* Or manually erase flash using ESP32 tools

## Tips for Memory and Performance

* Avoid large PNGs or images where not needed
* Use flat colors for backgrounds instead of full-size images
* Disable unused LVGL features in `lv_conf.h`
* Release build should disable LVGL logging for better performance
