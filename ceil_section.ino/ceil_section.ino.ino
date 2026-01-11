#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// ================= SETTINGS =================
#define WIFI_SSID "nawshad"
#define WIFI_PASSWORD "45683878"

// FIREBASE
#define API_KEY "AIzaSyCdRzx0ITGmNOttbBXchKvs0N5zx_m7_gc" 
#define DATABASE_URL "kitchensafety-39c65-default-rtdb.firebaseio.com" 

// ================= HARDWARE PINS =================
#define PIN_MQ2       A0    // Analog - Smoke Sensor
#define PIN_FLAME     5     // D1 - Flame Sensor
#define PIN_DHT       4     // D2 - DHT22 Sensor
#define PIN_MOTOR     12    // D6 - Fan Transistor

#define SMOKE_THRESHOLD 100 // Adjust based on calibration
#define DHTTYPE       DHT22 

// ================= GLOBALS =================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(PIN_DHT, DHTTYPE);

// Variables
bool floorGasAlert = false;
bool localSmokeAlert = false;
bool localFireAlert = false;
float temperature = 0.0;
float humidity = 0.0;
int smokeLevel = 0;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);
  
  // 1. Hardware Init
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_FLAME, INPUT);
  digitalWrite(PIN_MOTOR, LOW); // Fan OFF
  
  dht.begin();

  // 2. Wi-Fi Connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected to Wi-Fi");

  // 3. Firebase Init
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Firebase Auth Successful");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // ==========================================
  // 1. READ ALL SENSORS
  // ==========================================
  
  // A. DHT22 (Temp & Humidity)
  float newT = dht.readTemperature();
  float newH = dht.readHumidity();
  // Check if read failed and keep old value if so
  if (!isnan(newT)) temperature = newT;
  if (!isnan(newH)) humidity = newH;

  // B. Smoke (MQ-2)
  smokeLevel = analogRead(PIN_MQ2);
  localSmokeAlert = (smokeLevel > SMOKE_THRESHOLD);

  // C. Fire (Flame Sensor) - LOW usually means Fire detected
  int flameState = digitalRead(PIN_FLAME); 
  localFireAlert = (flameState == LOW); 

  // ==========================================
  // 2. READ FLOOR STATUS (DOWNLOAD)
  // ==========================================
  if (Firebase.ready() && signupOK) {
    if (Firebase.RTDB.getBool(&fbdo, "/kitchen/floor/is_critical")) {
      if (fbdo.dataType() == "boolean") {
        floorGasAlert = fbdo.boolData();
      }
    }
  }

  // ==========================================
  // 3. SAFETY LOGIC (FAN CONTROL)
  // ==========================================
  
  bool fanStatus = false;
  
  if (floorGasAlert || localSmokeAlert) {
    digitalWrite(PIN_MOTOR, HIGH); // Turn Fan ON
    fanStatus = true;
    Serial.println("🚨 DANGER! FAN ACTIVATED!");
  } else {
    digitalWrite(PIN_MOTOR, LOW);  // Turn Fan OFF
    fanStatus = false;
  }

  // ==========================================
  // 4. PRINT & UPLOAD STATUS (UPLOAD)
  // ==========================================
  
  // Print to Serial Monitor
  Serial.print("Temp: "); Serial.print(temperature); Serial.print("°C");
  Serial.print(" | Hum: "); Serial.print(humidity); Serial.print("%");
  Serial.print(" | Smoke: "); Serial.print(smokeLevel);
  Serial.print(" | Fire: "); Serial.print(localFireAlert ? "YES" : "NO");
  Serial.print(" | FloorGas: "); Serial.println(floorGasAlert ? "YES" : "NO");

  // Upload to Firebase (Every 2 seconds)
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    FirebaseJson json;
    json.set("temperature", temperature);
    json.set("humidity", humidity);
    json.set("smoke_level", smokeLevel);
    json.set("is_fire", localFireAlert);
    json.set("fan_status", fanStatus);

    // Save to "/kitchen/ceiling"
    if (Firebase.RTDB.setJSON(&fbdo, "/kitchen/ceiling", &json)) {
       // Upload success
    } else {
       Serial.print("Upload Failed: ");
       Serial.println(fbdo.errorReason());
    }
  }

  delay(500);
}