#include<WiFi.h>
#include<Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "NOS-4086"
#define WIFI_PASSWORD ""
#define API_KEY "AIzaSyC0mBg8Ut1i-FH2_dVntjXfn4RZoEgP_tA"
#define DATABASE_URL "https://damflow-scmu-default-rtdb.europe-west1.firebasedatabase.app/"

const int trig_pin = 14;
const int echo_pin = 13;
const int buzzer_pin = 27;

// Sound speed in air
#define SOUND_SPEED 340
#define TRIG_PULSE_DURATION_US 10

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK;
int sensorData;
float voltage = 0.0;

long ultrason_duration;
float distance_cm;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP:");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if(Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Signup OK");
    signupOk = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str);
  }

  config.token_status_callback = tokenStatusCallBack;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(trig_pin, OUTPUT); // We configure the trig as output
  pinMode(echo_pin, INPUT); // We configure the echo as input
  pinMode(buzzer_pin, OUTPUT);
}

void loop() {
  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // ----------------- STORE sensor data to a RTDB
    
  }

  digitalWrite(buzzer_pin, HIGH);
  delay(1000);
  digitalWrite(buzzer_pin, LOW);
  // Set up the signal
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
 // Create a 10 µs impulse
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(TRIG_PULSE_DURATION_US);
  digitalWrite(trig_pin, LOW);

  // Return the wave propagation time (in µs)
  ultrason_duration = pulseIn(echo_pin, HIGH);

//distance calculation
  distance_cm = ultrason_duration * SOUND_SPEED/2 * 0.0001;
  Serial.print("Ultrasound duration (ms): ");
  Serial.print(ultrason_duration);

  // We print the distance on the serial port 
  Serial.print("Distance (cm): ");
  Serial.println(distance_cm);

  delay(1000);
}