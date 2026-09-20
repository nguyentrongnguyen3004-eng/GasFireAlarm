#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>

// =================================================
// KHAI BAO CHAN ESP32-S2-MINI-1
// =================================================

#define MQ2_PIN       13
#define DS18B20_PIN   2
#define RELAY_PIN     26
#define BUZZER_PIN    15
#define LED_GREEN     16
#define LED_RED       17
#define SERVO_PIN     33
#define SDA_PIN       21
#define SCL_PIN       19


// =================================================
// LCD I2C
// =================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);


// =================================================
// DS18B20
// =================================================

OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);


// =================================================
// SERVO
// =================================================

Servo gasValve;


// =================================================
// NGUONG CANH BAO
// =================================================

// Gia tri MQ-2 co the thay doi tuy mo phong
int gasThreshold = 1800;

// Nhiet do canh bao
float temperatureThreshold = 50.0;


// =================================================
// TRANG THAI HE THONG
// =================================================

bool gasAlarm = false;
bool temperatureAlarm = false;


// =================================================
// DS18B20 KHONG CHAN CHUONG TRINH
// =================================================

// Bien luu nhiet do hien tai
float temperature = 25.0;

// Thoi gian doc DS18B20
unsigned long temperaturePreviousMillis = 0;

// Chu ky cap nhat nhiet do
const unsigned long temperatureInterval = 100;


// =================================================
// PHAN NHAP NHAY LED
// =================================================

// Thoi gian truoc do LED duoc doi trang thai
unsigned long previousMillis = 0;

// Trang thai hien tai cua LED do
bool redLedState = false;


// =================================================
// TRANG THAI LCD
// =================================================

// Luu noi dung LCD hien tai
String currentLine1 = "";
String currentLine2 = "";


// =================================================
// HAM HIEN THI LCD
// =================================================
// Chi cap nhat LCD khi noi dung thay doi
// Khong lcd.clear() lien tuc
// =================================================

void showLCD(String line1, String line2)
{
  if (line1 != currentLine1 || line2 != currentLine2)
  {
    currentLine1 = line1;
    currentLine2 = line2;

    lcd.setCursor(0, 0);
    lcd.print("                ");

    lcd.setCursor(0, 0);
    lcd.print(line1);

    lcd.setCursor(0, 1);
    lcd.print("                ");

    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}


// =================================================
// HAM NHAP NHAY LED DO
// =================================================

void blinkRedLED(unsigned long blinkInterval)
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= blinkInterval)
  {
    previousMillis = currentMillis;

    redLedState = !redLedState;

    digitalWrite(LED_RED, redLedState);
  }
}


// =================================================
// TRANG THAI AN TOAN
// =================================================

void normalState(float temperature, int gasValue)
{
  // LED
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);

  // Reset trang thai LED do
  redLedState = false;
  previousMillis = millis();

  // Tat coi
  //digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);

  // Tat relay
  digitalWrite(RELAY_PIN, LOW);

  // Mo van gas
  gasValve.write(0);

  // LCD
  String line1 = "HE THONG AN TOAN";

  String line2 = "T:";
  line2 += String(temperature, 1);
  line2 += "C G:";
  line2 += String(gasValue);

  showLCD(line1, line2);
}


// =================================================
// TRANG THAI RO RI GAS
// =================================================

void gasAlarmState()
{
  // LED
  digitalWrite(LED_GREEN, LOW);

  // LED DO CHOP 1 GIAY / LAN
  // 500ms sang + 500ms tat
  blinkRedLED(500);

  // Bat coi
  //digitalWrite(BUZZER_PIN, 2000);
  tone(BUZZER_PIN, 2000);

  // Bat relay
  digitalWrite(RELAY_PIN, HIGH);

  // Dong van
  gasValve.write(90);

  // LCD
  showLCD("CANH BAO RO GAS", "VAN: DONG");
}


// =================================================
// TRANG THAI NHIET DO CAO
// =================================================

void temperatureAlarmState()
{
  // LED
  digitalWrite(LED_GREEN, LOW);

  // LED DO CHOP 1 GIAY / LAN
  // 500ms sang + 500ms tat
  blinkRedLED(500);

  // Bat coi
  //digitalWrite(BUZZER_PIN, 2000);
  tone(BUZZER_PIN, 2000);

  // Bat relay
  digitalWrite(RELAY_PIN, HIGH);

  // Dong van
  gasValve.write(90);

  // LCD
  showLCD("NHIET DO CAO", "VAN: DONG");
}


// =================================================
// TRANG THAI NGUY HIEM
// =================================================

void dangerState()
{
  // LED
  digitalWrite(LED_GREEN, LOW);

  // LED DO CHOP 0.5 GIAY / LAN
  // 250ms sang + 250ms tat
  blinkRedLED(250);

  // Bat coi
  //digitalWrite(BUZZER_PIN, 2000);
  tone(BUZZER_PIN, 2000);

  // Bat relay
  digitalWrite(RELAY_PIN, HIGH);

  // Dong van
  gasValve.write(90);

  // LCD
  showLCD("!!! NGUY HIEM !!!", "GAS + NHIET CAO");
}


// =================================================
// SETUP
// =================================================

void setup()
{
  Serial.begin(115200);


  // -------------------------
  // Cau hinh chan
  // -------------------------

  pinMode(MQ2_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_GREEN, OUTPUT);

  pinMode(LED_RED, OUTPUT);


  // -------------------------
  // I2C
  // -------------------------

  Wire.begin(SDA_PIN, SCL_PIN);


  // -------------------------
  // LCD
  // -------------------------

  lcd.init();
  lcd.backlight();


  // -------------------------
  // DS18B20
  // -------------------------

  temperatureSensor.begin();

  // Cho phep DS18B20 do nhiet do
  // ma khong dung chuong trinh
  temperatureSensor.setResolution(9);

  temperatureSensor.setWaitForConversion(false);

  // Bat dau lan do dau tien
  temperatureSensor.requestTemperatures();

  temperaturePreviousMillis = millis();


  // -------------------------
  // SERVO
  // -------------------------

  gasValve.attach(SERVO_PIN);

  gasValve.write(0);


  // -------------------------
  // Trang thai ban dau
  // -------------------------

  digitalWrite(RELAY_PIN, LOW);

  //digitalWrite(BUZZER_PIN, LOW);
  noTone(BUZZER_PIN);

  digitalWrite(LED_GREEN, HIGH);

  digitalWrite(LED_RED, LOW);


  // -------------------------
  // Man hinh khoi dong
  // -------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("HE THONG CANH");

  lcd.setCursor(0, 1);
  lcd.print("BAO GAS");

  currentLine1 = "HE THONG CANH";
  currentLine2 = "BAO GAS";

  delay(2000);

  lcd.clear();

  currentLine1 = "";
  currentLine2 = "";
}


// =================================================
// LOOP
// =================================================

void loop()
{
  // =================================================
  // DOC CAM BIEN MQ-2
  // =================================================

  int gasValue = analogRead(MQ2_PIN);


  // =================================================
  // DOC DS18B20 KHONG CHAN CHUONG TRINH
  // =================================================

  if (millis() - temperaturePreviousMillis >= temperatureInterval)
  {
    temperaturePreviousMillis = millis();

    // Lay ket qua cua lan do truoc
    temperature = temperatureSensor.getTempCByIndex(0);

    // Bat dau lan do moi
    temperatureSensor.requestTemperatures();
  }


  // =================================================
  // KIEM TRA GAS
  // =================================================

  if (gasValue >= gasThreshold)
  {
    gasAlarm = true;
  }
  else
  {
    gasAlarm = false;
  }


  // =================================================
  // KIEM TRA NHIET DO
  // =================================================

  if (temperature >= temperatureThreshold)
  {
    temperatureAlarm = true;
  }
  else
  {
    temperatureAlarm = false;
  }


  // =================================================
  // HIEN THI SERIAL MONITOR
  // =================================================

  Serial.print("MQ-2 = ");
  Serial.print(gasValue);

  Serial.print(" | Nhiet do = ");
  Serial.print(temperature);

  Serial.print(" C | Trang thai: ");


  // =================================================
  // XU LY CAC TRANG THAI
  // =================================================

  if (gasAlarm && temperatureAlarm)
  {
    Serial.println("NGUY HIEM");

    dangerState();
  }

  else if (gasAlarm)
  {
    Serial.println("RO RI GAS");

    gasAlarmState();
  }

  else if (temperatureAlarm)
  {
    Serial.println("NHIET DO CAO");

    temperatureAlarmState();
  }

  else
  {
    Serial.println("AN TOAN");

    normalState(temperature, gasValue);
  }


  // Cho chuong trinh chay lien tuc
  // de LED nhap nhay dung toc do
  delay(10);
}