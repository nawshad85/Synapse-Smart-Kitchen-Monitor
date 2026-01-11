#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <Firebase_ESP_Client.h>
#include <HTTPClient.h>

// ======================= 1. WIFI SETTINGS =======================
#define WIFI_SSID "nawshad"
#define WIFI_PASS "45683878"

// ======================= 2. FIREBASE SETTINGS =======================
#define API_KEY "AIzaSyCdRzx0ITGmNOttbBXchKvs0N5zx_m7_gc"
#define DATABASE_URL "kitchensafety-39c65-default-rtdb.firebaseio.com"

// AUTH CREDENTIALS
#define USER_EMAIL "camera@kitchen.com"
#define USER_PASS "12345678"

// ======================= 3. SUPABASE SETTINGS =======================
const String SUPABASE_URL = "https://ipbvdngqvwzjxfetenik.supabase.co";
const String SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImlwYnZkbmdxdnd6anhmZXRlbmlrIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjYyOTE2NjUsImV4cCI6MjA4MTg2NzY2NX0.DG1Rpq8k9A-U66vqFpGUrouRwDaMeLbLeRTYRWXIe88";
// Used lowercase based on your File Browser ID
const String BUCKET_NAME = "cam_photos"; 

// ====================================================================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// CAMERA PINS (AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // === MEMORY SAVING MODE (CRITICAL) ===
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 15;           
    config.fb_count = 1;                
  } else {
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_camera_init(&config);
}

void uploadToSupabase() {
  Serial.println(">> Capturing photo...");
  
  // Free up RAM by stopping any lingering Firebase connection
  fbdo.stopWiFiClient();
  
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  HTTPClient http;
  String filename = "alert_" + String(millis()) + ".jpg";
  
  String url = SUPABASE_URL + "/storage/v1/object/" + BUCKET_NAME + "/" + filename;
  Serial.println(">> Uploading to: " + url);
  
  http.setReuse(false);
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + SUPABASE_KEY);
  http.addHeader("Content-Type", "image/jpeg");
  
  // REMOVED: x-upsert header. (This often causes 400 errors if you only have INSERT permissions)

  int httpResponseCode = http.POST(fb->buf, fb->len);

  // === DEBUG OUTPUT ===
  if (httpResponseCode > 0) {
    Serial.printf(">> Response Code: %d\n", httpResponseCode);
    
    // CAPTURE THE ERROR MESSAGE FROM SUPABASE
    String payload = http.getString();
    Serial.println(">> Server Response: " + payload);
    
  } else {
    Serial.printf(">> UPLOAD FAILED. Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  esp_camera_fb_return(fb);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  initCamera();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  // Optimized Timeouts
  config.timeout.wifiReconnect = 10000;
  config.timeout.socketConnection = 15000; 
  config.timeout.sslHandshake = 45000;
  config.timeout.serverResponse = 10000;
  
  config.cert.data = nullptr; 

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASS;

  Serial.println("Initializing Firebase...");
  Firebase.begin(&config, &auth);
  
  // Small SSL Buffers
  fbdo.setBSSLBufferSize(1024, 512); 
  fbdo.setResponseSize(1024);

  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready()) {
    Serial.print("Checking... ");
    
    if (Firebase.RTDB.getBool(&fbdo, "/kitchen/ceiling/is_fire")) {
        bool isFire = fbdo.boolData();
        Serial.println(isFire ? "FIRE DETECTED!" : "Safe");
        
        if(isFire) {
            uploadToSupabase();
            
            delay(100); 
            
            Serial.println("Resetting Trigger...");
            if(Firebase.RTDB.setBool(&fbdo, "/kitchen/ceiling/is_fire", false)){
               Serial.println("Trigger Reset Done.");
            } else {
               Serial.println("Reset Failed: " + fbdo.errorReason());
            }
        }
    } else {
        Serial.print("Read Error: ");
        Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.print("Connecting... Status: ");
    
    if(config.signer.tokens.status == token_status_error){
       Serial.print("Auth Failed: ");
       Serial.println(config.signer.tokens.error.message.c_str());
    } else {
       Serial.println(config.signer.tokens.status); 
    }
  }
  
  delay(2000);
}