#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

const char* DEVICE_ID    = "H001";
const char* DEVICE_TYPE  = "home";
const char* DEVICE_TOKEN = "token456";

const char* WIFI_SSID     = "X7";
const char* WIFI_PASSWORD = "12345678";
const char* SERVER_HOST   = "10.204.222.2";
const int   SERVER_PORT   = 5000;

const int PIN_ACS712         = 34;
const int PIN_VOLTAGE_SENSOR = 35;
const int PIN_LED_GREEN      = 25;
const int PIN_LED_RED        = 26;
const int PIN_LED_BLUE       = 27;

const float VOLTAGE_SENSOR_RATIO   = 5.0;
const float NO_VOLTAGE_THRESHOLD_V = 1.0;

const float ACS712_MV_PER_AMP  = 100.0;
const float ACS712_MIDPOINT_MV = 2504.5;

const unsigned long SEND_INTERVAL_MS = 5000;

unsigned long lastSend       = 0;
unsigned long lastBlueToggle = 0;
bool blueLedState = false;
bool lastKnownOn  = false;
bool timeSynced   = false;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting to wifi");

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(300);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nwifi up, ip = ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nno wifi yet, will retry in loop");
  }
}

void syncTimeFromNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("grabbing ntp time");

  time_t now = time(nullptr);
  int tries = 0;
  while (now < 8 * 3600 * 2 && tries < 15) {
    delay(300);
    Serial.print(".");
    now = time(nullptr);
    tries++;
  }

  timeSynced = now >= 8 * 3600 * 2;
  if (timeSynced) {
    Serial.println(" got it");
  } else {
    Serial.println(" no internet, falling back to uptime");
  }
}

String isoTimestampNow() {
  if (!timeSynced) {
    return "1970-01-01T00:00:" + String(millis() / 1000) + "Z";
  }

  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

float readAveragedMv(int pin, int samples = 64) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  return total / (float)samples;
}

void updateLeds(bool isOn, bool wifiConnected) {
  digitalWrite(PIN_LED_GREEN, isOn ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, isOn ? LOW : HIGH);

  if (wifiConnected) {
    digitalWrite(PIN_LED_BLUE, HIGH);
    return;
  }

  if (millis() - lastBlueToggle > 300) {
    blueLedState = !blueLedState;
    digitalWrite(PIN_LED_BLUE, blueLedState ? HIGH : LOW);
    lastBlueToggle = millis();
  }
}

bool postJson(const String& url, const String& body) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-ID", DEVICE_ID);
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  int code = http.POST(body);
  Serial.printf("  -> http %d\n", code);

  bool ok = (code == 200);
  if (code > 0 && !ok) {
    Serial.println(http.getString());
  }
  http.end();
  return ok;
}

void sendTelemetry(const char* status, float lineVoltage, float currentAmps) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("wifi dropped, reconnecting...");
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
  }

  String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/api/telemetry";

  String payload = "{";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"type\":\"" + String(DEVICE_TYPE) + "\",";
  payload += "\"status\":\"" + String(status) + "\",";
  payload += "\"voltage\":" + String(lineVoltage, 2) + ",";
  payload += "\"current_amps\":" + String(currentAmps, 3) + ",";
  payload += "\"timestamp\":\"" + isoTimestampNow() + "\"";
  payload += "}";

  postJson(url, payload);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ACS712, ADC_11db);
  analogSetPinAttenuation(PIN_VOLTAGE_SENSOR, ADC_11db);

  connectWiFi();
  syncTimeFromNTP();
}

void loop() {
  bool wifiConnected = WiFi.status() == WL_CONNECTED;

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();

    float sensorMv    = readAveragedMv(PIN_VOLTAGE_SENSOR);
    float lineVoltage = (sensorMv / 1000.0) * VOLTAGE_SENSOR_RATIO;
    lastKnownOn = lineVoltage > NO_VOLTAGE_THRESHOLD_V;

    float acsMv       = readAveragedMv(PIN_ACS712);
    float currentAmps = fabs(acsMv - ACS712_MIDPOINT_MV) / ACS712_MV_PER_AMP;

    Serial.printf("[%s] %s  current=%.3fA (raw %.1fmV)  voltage=%.2fV (raw %.1fmV)\n",
                  DEVICE_ID, lastKnownOn ? "ON" : "OFF", currentAmps, acsMv, lineVoltage, sensorMv);

    sendTelemetry(lastKnownOn ? "ON" : "OFF", lineVoltage, currentAmps);
  }

  updateLeds(lastKnownOn, wifiConnected);
}
