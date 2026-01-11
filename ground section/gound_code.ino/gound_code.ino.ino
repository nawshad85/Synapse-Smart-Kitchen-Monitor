#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

// ==========================================
// 1. YOUR SETTINGS
// ==========================================
#define WIFI_SSID "nawshad"
#define WIFI_PASSWORD "45683878"

// Credentials
#define API_KEY "AIzaSyCdRzx0ITGmNOttbBXchKvs0N5zx_m7_gc" 
#define DATABASE_URL "kitchensafety-39c65-default-rtdb.firebaseio.com" 

// ==========================================

#define MQ6_PIN A0

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);
  
  // FIXED LINE BELOW:
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

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
  int gasLevel = analogRead(MQ6_PIN);
  bool isCritical = (gasLevel > 400); 

  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    
    FirebaseJson json;
    json.set("lpg_value", gasLevel);
    json.set("is_critical", isCritical);

    if (Firebase.RTDB.setJSON(&fbdo, "/kitchen/floor", &json)) {
      Serial.print("SENT -> LPG: "); 
      Serial.print(gasLevel);
      Serial.print(" | Critical: ");
      Serial.println(isCritical ? "YES" : "NO");
    } else {
      Serial.print("FAILED: ");
      Serial.println(fbdo.errorReason());
    }
  }
}