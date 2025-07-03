#include <WiFi.h>
#include "time.h"
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define API_KEY ""
#define DATABASE_URL ""

#define SOUND_SPEED 340
#define TRIG_PULSE_DURATION_US 10
#define DEBOUNCE_TIME 3000

// Firebase Paths
#define HIGHER_THRESHOLD "/esp/higher_water_threshold"
#define LOWER_THRESHOLD "/esp/lower_water_threshold"
#define HATCH_REQUEST "/esp/open_hatch_request"
#define HATCH_IS_OPEN "/esp/hatch_is_open"
#define BUZZER_REQUEST "/esp/sound_buzzer_request"
#define BUZZER_IS_ON "/esp/buzzer_is_on"
#define CURRENT_WATER_LEVEL "/esp/current_water_level"
#define WATER_LEVELS "/esp/water_levels/"
#define IR_IS_TRIGGERED "/esp/ir_is_triggered"

// Pin configuration
static const int trig_pin = 14;
static const int echo_pin = 13;
static const int relay_pin = 4;
static const int ir_sensor_pin = 27;
static const int buzzer_pin = 26;
static const int led_pin = 25;

// Thresholds
float threshold_high = 50.0;
float threshold_low = 20.0;

// Firebase
FirebaseData fbdo, buzzer_stream, hatch_stream, lower_t_stream, higher_t_stream;
FirebaseAuth auth;
FirebaseConfig config;

// Auxiliary Variables
unsigned long sendDataPrevMillis = 0;
unsigned long int currentTime = 0;
time_t now;
bool signupOK;
bool hatchIsOpen, hatchRequest, buzzerIsOn, buzzerRequest, irIsTriggered;

void setup()
{
  Serial.begin(115200);

  // US sensor pins
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);

  // DC motor pin
  pinMode(relay_pin, OUTPUT);

  // IC sensor pin
  pinMode(ir_sensor_pin, INPUT);

  // Sirene pins
  pinMode(buzzer_pin, OUTPUT);
  pinMode(led_pin, OUTPUT);

  // Connect to Wi-Fi
  connectWifi();

  // Setup Firebase
  setupFirebase();

  // Synchronize time
  configTzTime("WET0WEST,M3.5.0/1,M10.5.0/2", "pool.ntp.org");
  Serial.println("Waiting for NTP time sync...");

  // Set initial thresholds to Firebase
  Firebase.RTDB.setFloat(&fbdo, HIGHER_THRESHOLD, threshold_high);
  Firebase.RTDB.setFloat(&fbdo, LOWER_THRESHOLD, threshold_low);

  // Set initial commands to Firebase
  Firebase.RTDB.setBool(&fbdo, HATCH_REQUEST, false);
  Firebase.RTDB.setBool(&fbdo, HATCH_IS_OPEN, hatchIsOpen);
  Firebase.RTDB.setBool(&fbdo, BUZZER_REQUEST, false);
  Firebase.RTDB.setBool(&fbdo, BUZZER_IS_ON, buzzerIsOn);
  Firebase.RTDB.setBool(&fbdo, IR_IS_TRIGGERED, false);
}

void loop()
{
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    // Get current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }

    // Format time
    char datetimeStr[30];
    strftime(datetimeStr, sizeof(datetimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    time_t now;
    time(&now);

    // Read water level and IR sensors
    float waterLevel = getWaterLevel();
    bool irIsTriggered = digitalRead(ir_sensor_pin) == HIGH;

    if (0.0 < waterLevel < 100.0)
    {
      // Push current water level into history
      FirebaseJson json;
      json.set("level", waterLevel);
      json.set("hatch_status", hatchIsOpen ? "open" : "closed");
      Firebase.RTDB.setJSON(&fbdo, WATER_LEVELS + String(now), &json);

      // Push current water level
      json.set("timestamp", now);
      json.set("datetime", datetimeStr);
      Firebase.RTDB.setJSON(&fbdo, CURRENT_WATER_LEVEL, &json);
    }

    // Push IR sensor trigger
    Firebase.RTDB.setBool(&fbdo, IR_IS_TRIGGERED, irIsTriggered);

    if (Firebase.ready() && signupOK)
    {
      if (!Firebase.RTDB.readStream(&hatch_stream))
        Serial.printf("Hatch Request Stream read error, %s\n\n", hatch_stream.errorReason().c_str());
      if (hatch_stream.streamAvailable())
      {
        if (hatch_stream.dataType() == "boolean")
        {
          hatchRequest = hatch_stream.boolData();
          Serial.println("Successful READ from " + hatch_stream.dataPath() + ": " + hatchRequest);
        }
      }
    }

    if (Firebase.ready() && signupOK)
    {
      if (!Firebase.RTDB.readStream(&buzzer_stream))
        Serial.printf("Buzzer Request Stream read error, %s\n\n", buzzer_stream.errorReason().c_str());
      if (buzzer_stream.streamAvailable())
      {
        if (buzzer_stream.dataType() == "boolean")
        {
          buzzerRequest = buzzer_stream.boolData();
          Serial.println("Successful READ from " + buzzer_stream.dataPath() + ": " + buzzerRequest);
        }
      }
    }

    if (Firebase.ready() && signupOK)
    {
      if (!Firebase.RTDB.readStream(&lower_t_stream))
        Serial.printf("Lower Threshold Stream read error, %s\n\n", lower_t_stream.errorReason().c_str());
      if (lower_t_stream.streamAvailable())
      {
        if (lower_t_stream.dataType() == "float")
        {
          threshold_low = lower_t_stream.floatData();
          Serial.println("Successful READ from " + lower_t_stream.dataPath() + ": " + threshold_low);
        }
      }
    }

    if (Firebase.ready() && signupOK)
    {
      if (!Firebase.RTDB.readStream(&higher_t_stream))
        Serial.printf("Higher Threshold Stream read error, %s\n\n", higher_t_stream.errorReason().c_str());
      if (higher_t_stream.streamAvailable())
      {
        if (higher_t_stream.dataType() == "float")
        {
          threshold_high = higher_t_stream.floatData();
          Serial.println("Successful READ from " + higher_t_stream.dataPath() + ": " + threshold_high);
        }
      }
    }

    // Check buzzer request
    if (buzzerRequest)
      soundBuzzer();

    if ((1.0 < waterLevel) && (waterLevel < 100.0))
    { // Valid Water Level Reading
      if (hatchRequest && waterLevel > threshold_low)
      {
        if (hatchIsOpen)
          closeHatch();
        else
          openHatch();
        // Reset Request
        hatchRequest = false;
        Firebase.RTDB.setBool(&fbdo, HATCH_REQUEST, false);
      }
      else if (waterLevel > threshold_high && !hatchIsOpen && !irIsTriggered)
        openHatch();
      else if (waterLevel <= threshold_low && hatchIsOpen)
      {
        closeHatch();
      }
    }

    // Print current stats
    Serial.printf("Water Level: %.2f cm | IR: %s | Request: %s | HighT: %.2f | LowT: %.2f\n",
                  waterLevel, irIsTriggered ? "YES" : "NO", hatchRequest ? "YES" : "NO", threshold_high, threshold_low);
  }
}

float getWaterLevel()
{
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(TRIG_PULSE_DURATION_US);
  digitalWrite(trig_pin, LOW);

  long duration = pulseIn(echo_pin, HIGH);
  float distance_cm = duration * SOUND_SPEED / 2 * 0.0001;
  return distance_cm;
}

void openHatch()
{
  beep(1500, 200, 500, 8);
  Serial.println("Opening hatch...");
  digitalWrite(relay_pin, HIGH); // Relay ON
  hatchIsOpen = true;
  Firebase.RTDB.setBool(&fbdo, HATCH_IS_OPEN, hatchIsOpen);
}

void closeHatch()
{
  beep(1500, 200, 500, 8);
  Serial.println("Closing hatch...");
  digitalWrite(relay_pin, LOW); // Relay OFF
  hatchIsOpen = false;
  Firebase.RTDB.setBool(&fbdo, HATCH_IS_OPEN, hatchIsOpen);
}

void soundBuzzer()
{
  Serial.println("Buzzer ON");
  buzzerIsOn = true;
  Firebase.RTDB.setBool(&fbdo, BUZZER_IS_ON, buzzerIsOn);
  while (buzzerRequest)
  {
    sirene(1000, 2000, 10);
    if (Firebase.ready() && signupOK)
    {
      if (!Firebase.RTDB.readStream(&buzzer_stream))
        Serial.printf("Buzzer Request Stream read error, %s\n\n", buzzer_stream.errorReason().c_str());
      if (buzzer_stream.streamAvailable())
      {
        if (buzzer_stream.dataType() == "boolean")
        {
          buzzerRequest = buzzer_stream.boolData();
          Serial.println("Successful READ from " + buzzer_stream.dataPath() + ": " + buzzerRequest);
        }
      }
    }
  }
  Serial.println("Buzzer OFF");
  buzzerIsOn = false;
  Firebase.RTDB.setBool(&fbdo, BUZZER_IS_ON, buzzerIsOn);
  noTone(buzzer_pin);
}

void beep(int freq, int beepDuration, int pauseDuration, int cycles)
{
  for (int i = 0; i < cycles; i++)
  {
    tone(buzzer_pin, freq);
    digitalWrite(led_pin, HIGH);
    delay(beepDuration);

    noTone(buzzer_pin);
    digitalWrite(led_pin, LOW);
    delay(pauseDuration);
  }
}

void sirene(int minfreq, int maxfreq, int stepDelay)
{
  digitalWrite(led_pin, HIGH);

  for (int freq = minfreq; freq <= maxfreq; freq += 10)
  {
    tone(buzzer_pin, freq);
    delay(stepDelay);
  }

  digitalWrite(led_pin, LOW);

  // Sweep down
  for (int freq = maxfreq; freq >= minfreq; freq -= 10)
  {
    tone(buzzer_pin, freq);
    delay(stepDelay);
  }

  noTone(buzzer_pin); // stop tone between cycles if needed
}

void connectWifi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected with IP: " + WiFi.localIP().toString());
}

void setupFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("Signup OK");
    signupOK = true;
  }
  else
  {
    Serial.printf("Signup error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Setup Firebase streams
  if (!Firebase.RTDB.beginStream(&hatch_stream, HATCH_REQUEST))
    Serial.printf("Hatch request stream begin error, %s\n\n", hatch_stream.errorReason().c_str());

  if (!Firebase.RTDB.beginStream(&buzzer_stream, BUZZER_REQUEST))
    Serial.printf("Buzzer request stream begin error, %s\n\n", buzzer_stream.errorReason().c_str());

  if (!Firebase.RTDB.beginStream(&lower_t_stream, LOWER_THRESHOLD))
    Serial.printf("Buzzer request stream begin error, %s\n\n", lower_t_stream.errorReason().c_str());

  if (!Firebase.RTDB.beginStream(&higher_t_stream, HIGHER_THRESHOLD))
    Serial.printf("Buzzer request stream begin error, %s\n\n", higher_t_stream.errorReason().c_str());
}
