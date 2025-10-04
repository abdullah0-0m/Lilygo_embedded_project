#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include "ui.h"

// ------------------- SENSOR (separate I2C bus) -------------------
TwoWire I2C_BME = TwoWire(1);      // Create secondary I2C bus
Adafruit_BME680 bme(&I2C_BME);     // Attach BME680 to that bus

// ------------------- DISPLAY -------------------
LilyGo_Class amoled;

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  // --- Initialize Display ---
  bool rslt = amoled.begin();
  if (!rslt) {
    while (1) {
      Serial.println("Display model not detected! Increase Core Debug Level to 'Error' in Arduino IDE.");
      delay(1000);
    }
  }

  beginLvglHelper(amoled);
  ui_init();
  Serial.println("UI initialized!");

  // --- Initialize Secondary I2C Bus for BME680 ---
  Serial.println("\nDevices Scan start (BME680 Bus)...");
  I2C_BME.begin(10, 11, 100000);   // SDA=10, SCL=11, 100kHz clock
  for (uint8_t address = 1; address < 127; address++) {
    I2C_BME.beginTransmission(address);
    if (I2C_BME.endTransmission() == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
    }
  }
  Serial.println("Scan Done\n");

  // --- Initialize BME680 Sensor ---
  if (!bme.begin(0x76)) {
    Serial.println("BME680 not found at 0x76, trying 0x77...");
    if (!bme.begin(0x77)) {
      Serial.println("Could not find a valid BME680 sensor!");
      while (1) delay(100);
    }
  }

  Serial.println("BME680 detected!");
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms

  Serial.println("BME680 init successful!");
}

// ------------------- LOOP -------------------
void loop() {
  // --- Perform Sensor Reading ---
  if (!bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    lv_label_set_text(ui_TempLabel, "Sensor Error");
    delay(2000);
    return;
  }

  float temperature = bme.temperature;
  float pressure    = bme.pressure / 100.0;       // hPa
  float humidity    = bme.humidity;
  float gas         = bme.gas_resistance / 1000.0; // kΩ

  // --- Serial Debugging ---
  Serial.println("Performing reading...");
  Serial.printf("Temperature = %.2f °C\n", temperature);
  Serial.printf("Pressure    = %.2f hPa\n", pressure);
  Serial.printf("Humidity    = %.2f %%\n", humidity);
  Serial.printf("Gas         = %.2f kΩ\n", gas);
  Serial.println("---------------------------");

  // --- Update UI Labels ---
  lv_label_set_text_fmt(ui_TempLabel, "%.2f °C", temperature);
  lv_label_set_text_fmt(ui_PressureLabel, "%.2f hPa", pressure);
  lv_label_set_text_fmt(ui_HumidityLabel, "%.2f %%", humidity);
  lv_label_set_text_fmt(ui_GasLabel, "%.2f kΩ", gas);

  // --- Refresh Display ---
  lv_task_handler();
  delay(2000);   // Refresh interval (2 seconds)
}
