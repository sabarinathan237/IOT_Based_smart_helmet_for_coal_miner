/*************************************************
 * Smart Helmet for Coal Miner Safety Monitoring
 * ESP8266 + MAX30100 + DHT11 + MQ2 + Vibration + GPS6MV2 + Blynk IoT
 * Accurate values only - NO default/fallback values
 *************************************************/

// ----------- BLYNK SETTINGS -----------
#define BLYNK_TEMPLATE_ID   "TMPL3KD138ian"
#define BLYNK_TEMPLATE_NAME "Smart Helmet for Coal Miner Safety Monitoring"
#define BLYNK_AUTH_TOKEN    "8a3zUuA5A5thsJjTCcYBEh366IX_qFUq"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include "DHT.h"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// ------------- PIN DEFINITIONS -------------
#define SDA_PIN       D2          // MAX30100 SDA  (GPIO4)
#define SCL_PIN       D1          // MAX30100 SCL  (GPIO5)

#define DHTPIN        D5          // DHT11 data pin
#define DHTTYPE       DHT11

#define MQ2_PIN       A0          // MQ2 analog output
#define VIB_PIN       D6          // Vibration sensor digital output
#define BUZZER_PIN    D7          // Buzzer pin

#define GPS_RX_PIN    D3          // ESP8266 RX (connect to GPS TX)
#define GPS_TX_PIN    D4          // ESP8266 TX (usually unused)

// ------------- THRESHOLDS -------------
#define HR_MIN        50
#define HR_MAX        120
#define SPO2_MIN      94

#define TEMP_MAX      38.0
#define HUMID_MAX     80.0

#define MQ2_THRESHOLD 400

// ------------- Wi-Fi CREDENTIALS -------------
char ssid[] = "helmet123";
char pass[] = "helmet321";

// ------------- BLYNK VIRTUAL PINS -------------
// V0 → Heart Rate
// V1 → SpO2
// V2 → Temperature
// V3 → Humidity
// V4 → MQ2 gas value
// V5 → Vibration
// V8 → Latitude
// V9 → Longitude
#define VPIN_HR       V0
#define VPIN_SPO2     V1
#define VPIN_TEMP     V2
#define VPIN_HUM      V3
#define VPIN_MQ2      V4
#define VPIN_VIB      V5
#define VPIN_LAT      V8
#define VPIN_LON      V9

// ------------- OBJECTS -------------
PulseOximeter pox;
DHT dht(DHTPIN, DHTTYPE);
TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);

BlynkTimer timer;

bool max30100Ok = false;

// Optional heartbeat callback
void onBeatDetected() {
  Serial.println("Beat detected!");
}

// -------- GPS HANDLER --------
void updateGPS() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

// -------- MAIN SENSOR FUNCTION --------
void readAndCheckSensors() {
  bool abnormal = false;

  // -------- MAX30100 (Heart Rate & SpO2) --------
  float hr = NAN;
  float spo2 = NAN;
  bool hrValid = false;
  bool spo2Valid = false;

  if (max30100Ok) {
    hr = pox.getHeartRate();
    spo2 = pox.getSpO2();

    hrValid = !(isnan(hr) || hr < 30 || hr > 220);
    spo2Valid = !(isnan(spo2) || spo2 < 70 || spo2 > 100);
  }

  Serial.print("HR: ");
  if (hrValid) {
    Serial.print(hr);
    Serial.print(" bpm");
  } else {
    Serial.print("INVALID");
  }

  Serial.print(" | SpO2: ");
  if (spo2Valid) {
    Serial.print(spo2);
    Serial.print(" %");
  } else {
    Serial.print("INVALID");
  }

  if (hrValid && spo2Valid) {
    if (hr < HR_MIN || hr > HR_MAX || spo2 < SPO2_MIN) {
      abnormal = true;
    }
  }

  // -------- DHT11 (Temp & Humidity) --------
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  Serial.print(" | Temp: ");
  if (!isnan(t)) {
    Serial.print(t);
    Serial.print(" °C");
    if (t > TEMP_MAX) {
      abnormal = true;
    }
  } else {
    Serial.print("INVALID");
  }

  Serial.print(" | Hum: ");
  if (!isnan(h)) {
    Serial.print(h);
    Serial.print(" %");
    if (h > HUMID_MAX) {
      abnormal = true;
    }
  } else {
    Serial.print("INVALID");
  }

  // -------- MQ2 (Gas Sensor) --------
  int mq2Raw = analogRead(MQ2_PIN);
  Serial.print(" | MQ2: ");
  Serial.print(mq2Raw);

  if (mq2Raw > MQ2_THRESHOLD) {
    abnormal = true;
  }

  // -------- Vibration Sensor --------
  int vib = digitalRead(VIB_PIN);
  Serial.print(" | Vib: ");
  Serial.print(vib);

  if (vib == HIGH) {
    abnormal = true;
  }

  // -------- GPS --------
  bool gpsOk = gps.location.isValid() && gps.location.age() < 10000;

  Serial.print(" | GPS: ");
  if (gpsOk) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.print(gps.location.lng(), 6);
    Serial.print(" (GPS)");
  } else {
    Serial.print("NO FIX");
  }

  // -------- BUZZER --------
  if (abnormal) {
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.print(" | STATUS: ABNORMAL (BUZZER ON)");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    Serial.print(" | STATUS: NORMAL");
  }

  Serial.println();

  // -------- SEND TO BLYNK ONLY IF VALID --------
  if (Blynk.connected()) {
    if (hrValid) {
      Blynk.virtualWrite(VPIN_HR, hr);
    }

    if (spo2Valid) {
      Blynk.virtualWrite(VPIN_SPO2, spo2);
    }

    if (!isnan(t)) {
      Blynk.virtualWrite(VPIN_TEMP, t);
    }

    if (!isnan(h)) {
      Blynk.virtualWrite(VPIN_HUM, h);
    }

    Blynk.virtualWrite(VPIN_MQ2, mq2Raw);
    Blynk.virtualWrite(VPIN_VIB, vib);

    if (gpsOk) {
      Blynk.virtualWrite(VPIN_LAT, gps.location.lat());
      Blynk.virtualWrite(VPIN_LON, gps.location.lng());
    }
  }
}

// Optional: sync when connected
BLYNK_CONNECTED() {
  Blynk.syncAll();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Smart Helmet - Accurate Sensor Mode");

  // ---------- WiFi + Blynk ----------
  Serial.println("Connecting to WiFi & Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("WiFi + Blynk connected.");

  // ---------- I2C + MAX30100 ----------
  Wire.begin(SDA_PIN, SCL_PIN);

#ifdef ESP8266
  Wire.setClock(100000);
  Wire.setClockStretchLimit(200000L);
#endif

  max30100Ok = pox.begin();
  if (max30100Ok) {
    Serial.println("MAX30100 initialized successfully.");
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
  } else {
    Serial.println("ERROR: MAX30100 initialization failed.");
    Serial.println("HR and SpO2 will remain INVALID until sensor works.");
  }

  // ---------- DHT ----------
  dht.begin();

  // ---------- GPIO ----------
  pinMode(MQ2_PIN, INPUT);
  pinMode(VIB_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ---------- GPS ----------
  gpsSerial.begin(9600);

  // ---------- Timer ----------
  timer.setInterval(1000L, readAndCheckSensors);

  Serial.println("Setup complete.");
}

void loop() {
  Blynk.run();
  timer.run();

  if (max30100Ok) {
    pox.update();
  }

  updateGPS();
}