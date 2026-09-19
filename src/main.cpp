#include <Arduino.h>
#include <ESP32Servo.h> // Thư viện điều khiển Servo trên ESP32

// ==========================================
// THÀNH VIÊN 1: KHAI BÁO CHÂN KẾT NỐI (PINS)
// ==========================================
// 1. Cảm biến đầu vào (Input)
const int MQ2_PIN = 34;      // Chân Analog A0 đọc cảm biến Gas MQ-2
const int FLAME_PIN = 35;    // Chân Digital đọc cảm biến lửa
const int DHT_PIN = 32;      // Chân Digital đọc nhiệt độ (Nếu có dùng)

// 2. Thiết bị đầu ra (Output)
const int BUZZER_PIN = 25;   // Còi báo động & Đèn chớp
const int RELAY_PIN = 26;    // Relay bật quạt thông gió
const int SERVO_PIN = 27;    // Cấp xung PWM cho Servo khóa van gas

// Khởi tạo đối tượng Servo
Servo gasValveServo;

// Biến lưu trữ dữ liệu
int gasValue = 0;
int flameValue = 0;

void setup() {
  Serial.begin(115200); // Khởi tạo giao tiếp Serial để xem kết quả trên máy tính

  // Cấu hình chân đầu vào
  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  // (Nếu dùng DHT11, thêm code khởi tạo thư viện DHT tại đây)

  // Cấu hình chân đầu ra
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Khởi tạo Servo
  gasValveServo.attach(SERVO_PIN);
  gasValveServo.write(0); // Đặt van ở vị trí mở (0 độ) ban đầu

  // Đảm bảo còi và relay đang tắt
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // [Yêu cầu hệ thống] Khởi động và làm nóng cảm biến Gas
  Serial.println("Hệ thống đang khởi động...");
  Serial.println("Đang làm nóng cảm biến MQ-2 (Gỉa lập chờ 10s)...");
  // Trong thực tế cần 1-3 phút, ở mô phỏng ta chỉ chờ 10s để test nhanh
  delay(10000); 
  Serial.println("Hoàn tất làm nóng. Bắt đầu thu thập dữ liệu!");
}

void loop() {
  // ==========================================
  // THÀNH VIÊN 1: THU THẬP DỮ LIỆU & ĐIỀU KHIỂN
  // ==========================================
  
  // 1. Lấy mẫu liên tục (đọc cảm biến)
  gasValue = analogRead(MQ2_PIN);
  flameValue = digitalRead(FLAME_PIN); 

  // In kết quả ra Serial Monitor để kiểm tra
  Serial.print("Nồng độ Gas: ");
  Serial.print(gasValue);
  Serial.print(" | Cảm biến Lửa: ");
  Serial.println(flameValue == LOW ? "PHÁT HIỆN LỬA!" : "An toàn"); 
  // (Cảm biến lửa thường báo LOW khi có lửa)

  // 2. Lập trình điều khiển thiết bị đầu ra (Test tính năng xuất tín hiệu)
  // GHI CHÚ CHO TV2: Thành viên 2 sẽ sửa lại logic if/else này kết hợp với hàm lọc nhiễu
  
  if (gasValue > 2000 || flameValue == LOW) { // Giả sử 2000 là ngưỡng nguy hiểm
    // Trạng thái báo động
    digitalWrite(BUZZER_PIN, HIGH);     // Bật còi & đèn
    digitalWrite(RELAY_PIN, HIGH);      // Bật quạt thông gió
    gasValveServo.write(90);            // Quay servo 90 độ để khóa van gas
  } else {
    // Trạng thái an toàn
    digitalWrite(BUZZER_PIN, LOW);      // Tắt còi
    digitalWrite(RELAY_PIN, LOW);       // Tắt quạt
    gasValveServo.write(0);             // Mở van gas
  }

  // Tần số lấy mẫu: 500ms/lần theo yêu cầu tài liệu
  delay(500); 
}