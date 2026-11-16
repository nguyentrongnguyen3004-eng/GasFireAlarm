#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// =================================================
// KHAI BAO CHAN ESP32 DEV-KIT
// =================================================
#define MQ2_PIN       35    
#define DS18B20_PIN   2
#define RELAY_PIN     26
#define BUZZER_PIN    15
#define LED_GREEN     16
#define LED_RED       17
#define SERVO_PIN     33
#define SDA_PIN       21
#define SCL_PIN       19

// =================================================
// CAU HINH WIFI & FIREBASE REALTIME DATABASE (SINGAPORE)
// =================================================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
String FIREBASE_HOST = "https://smartgas-5b127-default-rtdb.asia-southeast1.firebasedatabase.app";

// =================================================
// LCD I2C & CAM BIEN
// =================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);
Servo gasValve;

int currentServoAngle = -1;
void setValveAngle(int angle) {
  if (currentServoAngle != angle) {
    currentServoAngle = angle;
    gasValve.write(angle);
  }
}

// Ngưỡng cảnh báo:
int gasThresholdHigh = 2500;     // Kéo quá 2500 là BÁO ĐỘNG
int gasThresholdLow  = 2000;     // Kéo về dưới 2000 là TẮT CÒI
float tempThresholdHigh = 50.0;
float tempThresholdLow  = 46.0;

bool gasAlarm = false;
bool temperatureAlarm = false;
bool isCookingMode = false;
bool remoteValveClose = false;
bool remoteFanActive  = false;

float temperature = 25.0;
unsigned long temperaturePreviousMillis = 0;
const unsigned long temperatureInterval = 200;

unsigned long previousMillis = 0;
bool redLedState = false;
unsigned long lastLcdUpdate = 0;

String currentLine1 = "", currentLine2 = "";

// Cập nhật LCD mượt mà, không làm nghẽn bus I2C
void showLCD(String line1, String line2) {
  if (millis() - lastLcdUpdate >= 120) {
    if (line1 != currentLine1 || line2 != currentLine2) {
      currentLine1 = line1;
      currentLine2 = line2;
      lcd.setCursor(0, 0); lcd.print("                ");
      lcd.setCursor(0, 0); lcd.print(line1);
      lcd.setCursor(0, 1); lcd.print("                ");
      lcd.setCursor(0, 1); lcd.print(line2);
      lastLcdUpdate = millis();
    }
  }
}

void blinkRedLED(unsigned long blinkInterval) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= blinkInterval) {
    previousMillis = currentMillis;
    redLedState = !redLedState;
    digitalWrite(LED_RED, redLedState);
  }
}

int readGasFiltered() {
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(MQ2_PIN);
  }
  return (int)(sum / 4);
}

// =================================================
// BIẾN CHIA SẺ GIỮA 2 NHÂN (CORE 1: MẠCH & CORE 0: MẠNG)
// =================================================
volatile int syncGas = 420;
volatile float syncTemp = 28.0;
volatile bool syncGasAlarm = false;
volatile bool syncTempAlarm = false;
volatile bool syncDanger = false;
volatile bool syncValveClosed = false;
volatile bool syncFanActive = false;
volatile bool syncCookingMode = false;
volatile bool needCloudSync = false;

int lastSentGas = -999;
float lastSentTemp = -999;
bool lastSentGasAlarm = false;
bool lastSentTempAlarm = false;
unsigned long lastHeartbeat = 0;

// Kiểm tra và tự động kết nối lại Wi-Fi ngầm
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("\n[WiFi] Dang ket noi lai Wokwi-GUEST (Channel 6)");
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin("Wokwi-GUEST", "", 6);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(200);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WiFi] Da ket noi lai thanh cong! IP: " + WiFi.localIP().toString());
    }
  }
}

// Hàm gửi dữ liệu Firebase chạy ngầm trên Core 0
void sendToFirebaseCore0() {
  checkWiFiConnection();
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(3000);
  if (http.begin(client, FIREBASE_HOST + "/system.json")) {
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"gas\":" + String(syncGas) + ",";
    json += "\"temp\":" + String(syncTemp, 1) + ",";
    json += "\"gasAlarm\":" + String(syncGasAlarm ? "true" : "false") + ",";
    json += "\"tempAlarm\":" + String(syncTempAlarm ? "true" : "false") + ",";
    json += "\"danger\":" + String(syncDanger ? "true" : "false") + ",";
    json += "\"valveClosed\":" + String(syncValveClosed ? "true" : "false") + ",";
    json += "\"fanActive\":" + String(syncFanActive ? "true" : "false") + ",";
    json += "\"cookingMode\":" + String(syncCookingMode ? "true" : "false");
    json += "}";

    int httpCode = http.PUT(json);
    if (httpCode > 0) {
      Serial.printf("[Cloud] Dong bo OK! Gas=%d | Temp=%.1f C\n", syncGas, syncTemp);
    }
    http.end();
  }
  client.stop(); // Giải phóng RAM SSL ngay lập tức
}

// Đọc lệnh điều khiển (Giờ nấu ăn, Van, Quạt, Ngưỡng động) từ Firebase
void readControlsFromFirebaseCore0() {
  checkWiFiConnection();
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(2500);
  if (http.begin(client, FIREBASE_HOST + "/controls.json")) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      
      // 1. Chế độ Giờ Nấu Ăn
      if (payload.indexOf("\"cookingMode\":true") >= 0) {
        if (!isCookingMode) { isCookingMode = true; syncCookingMode = true; needCloudSync = true; }
      } else if (payload.indexOf("\"cookingMode\":false") >= 0) {
        if (isCookingMode) { isCookingMode = false; syncCookingMode = false; needCloudSync = true; }
      }

      // 2. Lệnh Khóa/Mở Van Gas (Servo)
      if (payload.indexOf("\"valveClosed\":true") >= 0) {
        if (!remoteValveClose) { remoteValveClose = true; syncValveClosed = true; needCloudSync = true; }
      } else if (payload.indexOf("\"valveClosed\":false") >= 0) {
        if (remoteValveClose) { remoteValveClose = false; syncValveClosed = false; needCloudSync = true; }
      }

      // 3. Lệnh Bật/Tắt Quạt Hút (Relay)
      if (payload.indexOf("\"fanActive\":true") >= 0) {
        if (!remoteFanActive) { remoteFanActive = true; syncFanActive = true; needCloudSync = true; }
      } else if (payload.indexOf("\"fanActive\":false") >= 0) {
        if (remoteFanActive) { remoteFanActive = false; syncFanActive = false; needCloudSync = true; }
      }

      // 4. Đồng bộ ngưỡng Gas cài đặt từ thanh trượt Web
      int gIdx = payload.indexOf("\"gasThreshold\":");
      if (gIdx >= 0) {
        int val = payload.substring(gIdx + 15).toInt();
        if (val >= 800 && val <= 3800 && val != gasThresholdHigh) {
          gasThresholdHigh = val;
          gasThresholdLow = val - 500;
          needCloudSync = true;
        }
      }

      // 5. Đồng bộ ngưỡng Nhiệt độ cài đặt từ thanh trượt Web
      int tIdx = payload.indexOf("\"tempThreshold\":");
      if (tIdx >= 0) {
        float val = payload.substring(tIdx + 16).toFloat();
        if (val >= 35.0 && val <= 90.0 && abs(val - tempThresholdHigh) > 0.5) {
          tempThresholdHigh = val;
          tempThresholdLow = val - 4.0;
          needCloudSync = true;
        }
      }
    }
    http.end();
  }
  client.stop();
}

// Tác vụ mạng FreeRTOS độc lập trên CORE 0 (KHÔNG LÀM CHẶN MẠCH Ở CORE 1)
void firebaseNetworkTask(void *pvParameters) {
  unsigned long lastSendTime = 0;
  unsigned long lastReadControls = millis(); // Khởi tạo mốc để không đọc ngốn mạng khi mới boot

  for (;;) {
    // 1. ƯU TIÊN SỐ 1: Bắn dữ liệu cảm biến lên Cloud ngay khi có biến động (Gas, nhiệt, cảnh báo)
    // Phản hồi siêu tốc trong 250ms (giảm 70% độ trễ trên Wokwi)
    if (needCloudSync && (millis() - lastSendTime >= 250)) {
      needCloudSync = false;
      lastSendTime = millis();
      sendToFirebaseCore0();
    }

    // 2. Đọc lệnh điều khiển từ Web: CHỈ đọc khi Core 0 rảnh (không có dữ liệu cảm biến chờ gửi)
    // và cách nhau 3.5 giây SAU KHI hoàn tất lần đọc trước (tránh nghẽn kênh SSL trên Wokwi)
    if (!needCloudSync && (millis() - lastReadControls >= 3500)) {
      readControlsFromFirebaseCore0();
      lastReadControls = millis(); // Cập nhật mốc thời gian SAU KHI đọc xong
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // Nhường CPU 20ms
  }
}

// =================================================
// LOGIC CÁC TRẠNG THÁI HỆ THỐNG
// =================================================
void normalState(float temp, int gasVal) {
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  redLedState = false;
  noTone(BUZZER_PIN);

  if (remoteFanActive) digitalWrite(RELAY_PIN, HIGH);
  else digitalWrite(RELAY_PIN, LOW);

  if (remoteValveClose) setValveAngle(90);
  else setValveAngle(0);

  if (remoteValveClose && remoteFanActive) {
    showLCD("WEB: VAN+QUAT", "T:" + String(temp, 1) + "C G:" + String(gasVal));
  } else if (remoteValveClose) {
    showLCD("WEB: KHOA VAN", "T:" + String(temp, 1) + "C G:" + String(gasVal));
  } else if (remoteFanActive) {
    showLCD("WEB: BAT QUAT", "T:" + String(temp, 1) + "C G:" + String(gasVal));
  } else if (isCookingMode) {
    showLCD("GIO NAU AN (+500)", "T:" + String(temp, 1) + "C G:" + String(gasVal));
  } else {
    showLCD("HE THONG AN TOAN", "T:" + String(temp, 1) + "C G:" + String(gasVal));
  }
}

void gasAlarmState(int gasVal) {
  digitalWrite(LED_GREEN, LOW);
  blinkRedLED(500);
  tone(BUZZER_PIN, 2000);
  digitalWrite(RELAY_PIN, HIGH);
  setValveAngle(90);
  showLCD("CANH BAO RO GAS", "GAS:" + String(gasVal) + " VAN:DONG");
}

void temperatureAlarmState(float temp) {
  digitalWrite(LED_GREEN, LOW);
  blinkRedLED(500);
  tone(BUZZER_PIN, 2000);
  digitalWrite(RELAY_PIN, HIGH);
  setValveAngle(90);
  showLCD("NHIET DO CAO", "T:" + String(temp, 1) + "C QUAT:BAT");
}

void dangerState(int gasVal, float temp) {
  digitalWrite(LED_GREEN, LOW);
  blinkRedLED(250);
  tone(BUZZER_PIN, 2500);
  digitalWrite(RELAY_PIN, HIGH);
  setValveAngle(90);
  showLCD("! NGUY HIEM !", "G:" + String(gasVal) + " T:" + String(temp, 1) + "C");
}

void setup() {
  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  temperatureSensor.begin();
  temperatureSensor.setResolution(12); // Nâng lên 12-bit để độ chính xác 0.06°C (khớp từng 0.1°C với Wokwi)
  temperatureSensor.setWaitForConversion(false);
  temperatureSensor.requestTemperatures();
  temperaturePreviousMillis = millis();

  gasValve.attach(SERVO_PIN);
  setValveAngle(0);

  digitalWrite(RELAY_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("HE THONG IOT GAS");
  lcd.setCursor(0, 1); lcd.print("DANG KHOI DONG..");

  Serial.println("\n--- HE THONG IOT CANH BAO GAS & CHAY (DUAL-CORE RTOS) ---");
  Serial.print("[WiFi] Dang ket noi Wokwi-GUEST");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Wokwi-GUEST", "", 6);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(200);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] KET NOI THANH CONG! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Dang cap DHCP ngam.");
  }

  // KHỞI TẠO TÁC VỤ MẠNG CHẠY RIÊNG TRÊN CORE 0 (TÁCH BIỆT KHỎI CORE 1)
  xTaskCreatePinnedToCore(
    firebaseNetworkTask,
    "FirebaseSyncTask",
    8192,
    NULL,
    1,
    NULL,
    0 // Chạy trên Core 0
  );

  delay(400);
  lcd.clear();
  currentLine1 = ""; currentLine2 = "";
}

// =================================================
// VÒNG LẶP CHÍNH CHẠY TRÊN CORE 1 (TỐC ĐỘ 100Hz, PHẢN HỒI TỨC THÌ 0 GIÂY)
// =================================================
void loop() {
  int gasValue = readGasFiltered();

  // Đọc cảm biến nhiệt độ không chặn
  if (millis() - temperaturePreviousMillis >= temperatureInterval) {
    temperaturePreviousMillis = millis();
    float t = temperatureSensor.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C && t > -40.0 && t < 125.0) {
      temperature = t;
    }
    temperatureSensor.requestTemperatures();
  }

  int currentGasThresh = isCookingMode ? (gasThresholdHigh + 500) : gasThresholdHigh;

  if (gasValue >= currentGasThresh) gasAlarm = true;
  else if (gasValue < gasThresholdLow) gasAlarm = false;

  if (temperature >= tempThresholdHigh) temperatureAlarm = true;
  else if (temperature < tempThresholdLow) temperatureAlarm = false;

  // Xử lý cơ cấu chấp hành tức thì (LCD, Còi, Quạt, Van phản hồi < 10ms)
  if (gasAlarm && temperatureAlarm) {
    dangerState(gasValue, temperature);
  } else if (gasAlarm) {
    gasAlarmState(gasValue);
  } else if (temperatureAlarm) {
    temperatureAlarmState(temperature);
  } else {
    normalState(temperature, gasValue);
  }

  // =================================================
  // CƠ CHẾ KÍCH HOẠT ĐỒNG BỘ THEO BIẾN ĐỘNG (EVENT-TRIGGERED) SIÊU NHẠY
  // Kéo thanh trượt lệch >= 20 ADC là lập tức bắn lên Cloud ngay!
  // =================================================
  bool stateChanged = (gasAlarm != lastSentGasAlarm) || (temperatureAlarm != lastSentTempAlarm);
  bool gasChanged = abs(gasValue - lastSentGas) >= 20; // Giảm từ 60 xuống 20 để cực kỳ nhạy khi kéo slider
  bool tempChanged = abs(temperature - lastSentTemp) >= 0.25; // Giảm từ 0.8 xuống 0.25 độ C
  bool heartbeatDue = (millis() - lastHeartbeat >= 1600); // 1.6s gửi duy trì định kỳ (giảm từ 3.5s)

  if (stateChanged || gasChanged || tempChanged || heartbeatDue) {
    lastSentGas = gasValue;
    lastSentTemp = temperature;
    lastSentGasAlarm = gasAlarm;
    lastSentTempAlarm = temperatureAlarm;
    lastHeartbeat = millis();

    // Snapshot dữ liệu an toàn sang biến chia sẻ
    syncGas = gasValue;
    syncTemp = temperature;
    syncGasAlarm = gasAlarm;
    syncTempAlarm = temperatureAlarm;
    syncDanger = gasAlarm && temperatureAlarm;
    syncValveClosed = (currentServoAngle == 90);
    syncFanActive = (digitalRead(RELAY_PIN) == HIGH);
    syncCookingMode = isCookingMode;

    needCloudSync = true; // Đánh thức Core 0 gửi lên Cloud lập tức!
  }

  delay(10); // Chu kỳ quét 10ms siêu nhanh
}
