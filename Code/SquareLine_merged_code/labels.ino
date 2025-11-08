/***********************
   Qwiic BME680 + LilyGo AMOLED
   BSEC2 + AQI History + WiFi Time (12 Hours)
************************/

#include <Wire.h>
#include <bsec2.h>
#include <Preferences.h>

#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>

#include "ui.h"           // Main UI header
#include <WiFi.h>
#include "time.h"

// -------- AQI Arrow Placement Constants (Screen2) --------
const int ARROW_Y_POS = -22;     // Fixed Y-coordinate for arrow placement
const int ARROW_MIN_X = -40;     // X-position when IAQ = 0
const int ARROW_MAX_X = 230;     // X-position when IAQ = 300

// -------- GLOBALS --------
LilyGo_Class amoled;
Bsec2 envSensor;
Preferences prefs;

uint8_t detectedAddress = 0x00;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint32_t bsecStateSize = 0;
bool baselineRestored = false;
int8_t lastAccuracy = 0;

// Chart (Screen2)
static const int HISTORY_POINTS = 72;  // 12 hours @ 10-min interval
lv_chart_series_t* serAQI = nullptr;
int lastChartIndex = -1;

// WiFi + Time
const char* ssid = "SSID";
const char* password = "password";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600 + 1800; // UTC+5:30
const int daylightOffset_sec = 0;

// -------- WIFI + TIME --------
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" connected ✅");
}

void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void updateTimeLabel() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
      // Time formatting
      char timeStr[10];  // "HH:MM AM"
      int hour12 = timeinfo.tm_hour % 12;
      if (hour12 == 0) hour12 = 12;
      const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
      sprintf(timeStr, "%02d:%02d %s", hour12, timeinfo.tm_min, ampm);
      lv_label_set_text(ui_TimeLabel, timeStr);  // Screen1
      lv_label_set_text(ui_Time, timeStr);       // Screen2

      // Day and Date formatting (Screen1)
      const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
      lv_label_set_text(ui_DayLabel, days[timeinfo.tm_wday]);

      char dateStr[10]; // "14 Jan"
      const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      sprintf(dateStr, "%d %s", timeinfo.tm_mday, months[timeinfo.tm_mon]);
      lv_label_set_text(ui_DateLabel, dateStr);
    }
  }
}

// -------- CHART HANDLING (72 points, time-based index) --------
void initAQIChart() {
  lv_chart_set_point_count(ui_AQIHistory, HISTORY_POINTS);

  serAQI = lv_chart_add_series(ui_AQIHistory, lv_color_hex(0x18C240), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < HISTORY_POINTS; i++) {
    lv_chart_set_value_by_id(ui_AQIHistory, serAQI, i, LV_CHART_POINT_NONE);
  }

  // Add X-axis tick labels every 2 hours
  lv_chart_set_axis_tick(ui_AQIHistory, LV_CHART_AXIS_PRIMARY_X,
                         5, 2, 6, 1, true, 50);
  //lv_chart_set_axis_tick_texts(ui_AQIHistory,
   //                            "12AM\n2AM\n4AM\n6AM\n8AM\n10AM\n12PM\n2PM\n4PM\n6PM\n8PM\n10PM",
     //                          11, LV_CHART_AXIS_PRIMARY_X);
}

void addAQIToChart(float iaq) {
  // Compute current index
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  int minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int index = (minutes / 10) % HISTORY_POINTS;  // 10 min step

  if (index != lastChartIndex) {
    lv_chart_set_value_by_id(ui_AQIHistory, serAQI, index, (int)iaq);
    lastChartIndex = index;
  }
}

void updateAQIArrow(float iaq) {
  // Clamp IAQ between 0 and 300
  if (iaq < 0) iaq = 0;
  if (iaq > 300) iaq = 300;

  // Map IAQ to usable X position (scaled to bar)
  int xPos = map((int)iaq, 0, 300, ARROW_MIN_X, ARROW_MAX_X);

  // Update arrow position
  lv_obj_set_x(ui_AQIArrow, xPos);
  lv_obj_set_y(ui_AQIArrow, ARROW_Y_POS);
}


// -------- BSEC CALLBACK --------
void newDataCallback(bme68x_data data, bsecOutputs outputs, Bsec2 bsec) {
  float temp = NAN, hum = NAN, pres = NAN, iaq = NAN;
  int accuracy = 0;

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    switch (outputs.output[i].sensor_id) {
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      temp = outputs.output[i].signal; break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      hum = outputs.output[i].signal; break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      pres = outputs.output[i].signal / 100.0; break;
    case BSEC_OUTPUT_IAQ:
      iaq = outputs.output[i].signal;
      accuracy = outputs.output[i].accuracy;
      break;
    }
  }

  lastAccuracy = accuracy;
  Serial.printf("T=%.1f°C  H=%.1f%%  P=%.0f hPa  IAQ=%.0f (acc %d)\n",
                temp, hum, pres, iaq, accuracy);

  // --- SCREEN 1 LABELS ---
  if (accuracy < 2)
    lv_label_set_text(ui_AQIValue, "Calibrating...");
  else
    lv_label_set_text_fmt(ui_AQIValue, "IAQ: %.0f", iaq);

  lv_label_set_text_fmt(ui_TEMPValue, "%.1f °C", temp);
  lv_label_set_text_fmt(ui_HumiValue, "%.1f %%", hum);
  lv_label_set_text_fmt(ui_PressValue, "%.0f hPa", pres);

  // --- SCREEN 2 UI (Large AQI + Status) ---
  if (accuracy >= 2) {
    lv_label_set_text_fmt(ui_AQI, "%.0f", iaq);

    updateAQIArrow(iaq);

    const char* status = "GOOD";
    if (iaq <= 50) status = "GOOD";
    else if (iaq <= 100) status = "MODERATE";
    else if (iaq <= 200) status = "UNHEALTHY";
    else status = "VERY BAD";
    lv_label_set_text(ui_AQIStatus, status);
  }

  // ---------- FULL FILTER ON AQI HISTORY ----------
  static unsigned long warmupStart = millis();
  static float lastValidIAQ = -1;
  static float smoothIAQ = 0;

  // 1️⃣ ignore first 2 minutes OR until accuracy OK
  if (millis() - warmupStart < 120000 || accuracy < 2) {
    return;
  }

  // 2️⃣ reject NAN or out-of-range values
  if (!(iaq > 0 && iaq < 500)) {
    Serial.println("⚠️ Rejected out-of-range value");
    return;
  }

  // 3️⃣ reject sudden spikes ( >60 jump )
  if (lastValidIAQ > 0 && abs(iaq - lastValidIAQ) > 60) {
    Serial.println("⚠️ Rejected spike");
    return;
  }

  // 4️⃣ smooth with weighted average
  if (lastValidIAQ < 0) smoothIAQ = iaq;
  else smoothIAQ = (smoothIAQ * 0.7f) + (iaq * 0.3f);

  lastValidIAQ = iaq;

  // 5️⃣ now update chart
  addAQIToChart(smoothIAQ);

  lv_timer_handler();
}

// -------- I2C SCANNER --------
uint8_t scanForBME() {
  Serial.println("Scanning I2C (shared Wire)...");
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found I2C device: 0x%02X\n", a);
      if (a == 0x76 || a == 0x77) found = a; // BME680
    }
    yield();
  }
  return found;
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\nBooting...");
  
  //Uncomment below lines to recalibrate sensor
  //prefs.begin("bsec", false);
  //prefs.clear();   // <<--- This wipes all saved baseline data!
  //prefs.end();
  //Serial.println("⚠️ BSEC baseline cleared — recalibration starting");

  // DISPLAY + LVGL
  if (!amoled.begin()) {
    Serial.println("❌ Display init failed!");
    while (1) delay(500);
  }
  beginLvglHelper(amoled);

  ui_init();   // Screen1 loads automatically
  Serial.println("Display initialized ✅");

  Wire.setClock(100000);

  // Chart setup (Screen2 only, time-based)
  initAQIChart();

  // WiFi + Time
  connectToWiFi();
  initTime();

  // BME680 + BSEC Setup
  detectedAddress = scanForBME();
  if (!detectedAddress) {
    Serial.println("❌ BME680 NOT FOUND");
    while (1) delay(500);
  }
  Serial.printf("✅ BME680 detected at 0x%02X\n", detectedAddress);

  if (!envSensor.begin(detectedAddress, Wire)) {
    Serial.println("❌ BSEC2 init failed");
    while (1) delay(500);
  }

  bsec_version_t v = envSensor.version;
  Serial.printf("BSEC2 %d.%d.%d.%d\n", v.major, v.minor, v.major_bugfix, v.minor_bugfix);

  bsec_virtual_sensor_t outputs[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE
  };
  envSensor.updateSubscription(outputs, 7, BSEC_SAMPLE_RATE_LP);
  envSensor.attachCallback(newDataCallback);

  prefs.begin("bsec", false);
  bsecStateSize = prefs.getBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
  if (bsecStateSize == BSEC_MAX_STATE_BLOB_SIZE && envSensor.setState(bsecState)) {
    baselineRestored = true;
    Serial.println("✅ Baseline restored");
  } else {
    Serial.println("ℹ️ No baseline saved yet");
  }

  Serial.println("✅ Env sensor ready!\n");
}

// -------- LOOP --------
void loop() {
  static unsigned long lastSave = 0;

  if (envSensor.run()) {
    if (lastAccuracy >= 2 && millis() - lastSave > 300000) {
      if (envSensor.getState(bsecState)) {
        prefs.putBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
        Serial.println("✅ Baseline saved");
        lastSave = millis();
      }
    }
  }

  updateTimeLabel();
  lv_timer_handler();
  delay(5);
}
