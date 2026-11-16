# 🌐 Web Dashboard - Hệ Thống Cảnh Báo Rò Rỉ Khí Gas & Cháy Thông Minh

Hệ thống Web Dashboard giám sát thời gian thực và điều khiển từ xa hai chiều cho đồ án IoT Cảnh báo Rò rỉ Khí Gas & Hỏa hoạn (Khoa CNTT - Trường Đại học Công Thương TP.HCM - HUIT).

---

## 🚀 Các Tính Năng Nổi Bật

1. **Giám sát thời gian thực (Real-time Monitoring):**
   - Hiển thị trực quan nồng độ khí Gas (MQ-2 theo thang đo ADC 12-bit & cảnh báo).
   - Hiển thị nhiệt độ môi trường (DS18B20 độ chính xác cao 12-bit).
   - Biểu đồ thời gian thực trực quan (Chart.js) theo dõi biến thiên thông số.
   - Nhận diện đa trạng thái: **An toàn**, **Rò rỉ Gas**, **Nhiệt độ cao**, **Báo động đỏ khẩn cấp**.

2. **Điều khiển từ xa 2 chiều (Bidirectional Remote Control):**
   - **Khóa/Mở Van Gas tự động**: Điều khiển Servo đóng ngắt nguồn cấp gas tức thì khi có sự cố.
   - **Bật/Tắt Quạt hút thông gió**: Điều khiển Relay kích hoạt quạt giải phóng khí độc.
   - **Chế độ Giờ Nấu Ăn Thông Minh (Smart Cooking Mode)**: Tự động nâng ngưỡng cảnh báo gas thêm +500 ADC để chống báo động giả trong giờ nấu nướng gia đình.
   - Cơ chế khóa chống giật lag / rollback trạng thái (Optimistic UI lock 4s).

3. **Lưu trữ & Xuất báo cáo chuyên nghiệp:**
   - **Lưu trữ vĩnh viễn (Persistent Log)**: Nhật ký sự cố được lưu tự động xuống cơ sở dữ liệu file (`logs_history.json`) và bộ nhớ trình duyệt (`localStorage`), không bao giờ bị mất khi bấm F5 reload.
   - **Xuất báo cáo Excel cao cấp (.xlsx)**: Tạo file Excel định dạng chuẩn bảng biểu, có tiêu đề trường HUIT, dải màu phân biệt mức độ cảnh báo (Xanh lá / Vàng / Đỏ), kẻ khung bảng và khung chữ ký người lập báo cáo.

---

## 🛠️ Cài Đặt & Khởi Chạy

### Yêu cầu hệ thống:
* **Node.js** (phiên bản v16 trở lên)
* **npm** (đi kèm Node.js)

### Các bước khởi chạy:

1. Mở Terminal / PowerShell và chuyển vào thư mục:
```bash
cd web-dashboard
```

2. Cài đặt các thư viện cần thiết:
```bash
npm install
```

3. Khởi chạy Server:
```bash
npm start
```

4. Truy cập giao diện trên trình duyệt:
```
http://localhost:5000
```

---

## 📡 Kiến Trúc Tích Hợp Firebase & ESP32

* **Firebase Realtime Database Endpoint:**
  `https://smartgas-5b127-default-rtdb.asia-southeast1.firebasedatabase.app`
* **Node dữ liệu thiết bị:** `/system.json`
* **Node dữ liệu điều khiển:** `/controls.json`
* **Vi điều khiển ESP32:** Chạy FreeRTOS Đa nhân (Dual-Core), Core 1 xử lý phần cứng 100Hz, Core 0 đồng bộ Firebase qua HTTPS không gây trễ hệ thống.
