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
// HAM HIEN THI LCD
// =================================================

void showLCD(String line1, String line2)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);
}


// =================================================
// TRANG THAI AN TOAN
// =================================================

void normalState(float temperature, int gasValue)
{
  // LED
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);

  // Tat coi
  digitalWrite(BUZZER_PIN, LOW);

  // Tat relay
  digitalWrite(RELAY_PIN, LOW);

  // Mo van gas
  gasValve.write(0);

  // LCD
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("HE THONG AN TOAN");

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C G:");
  lcd.print(gasValue);
}


// =================================================
// TRANG THAI RO RI GAS
// =================================================

void gasAlarmState()
{
  // LED
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);

  // Bat coi
  digitalWrite(BUZZER_PIN, HIGH);

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
  digitalWrite(LED_RED, HIGH);

  // Bat coi
  digitalWrite(BUZZER_PIN, HIGH);

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
  digitalWrite(LED_RED, HIGH);

  // Bat coi
  digitalWrite(BUZZER_PIN, HIGH);

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


  // -------------------------
  // SERVO
  // -------------------------

  gasValve.attach(SERVO_PIN);

  gasValve.write(0);


  // -------------------------
  // Trang thai ban dau
  // -------------------------

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

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

  delay(2000);

  lcd.clear();
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
  // DOC CAM BIEN DS18B20
  // =================================================

  temperatureSensor.requestTemperatures();

  float temperature =
    temperatureSensor.getTempCByIndex(0);


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


  delay(1000);
}