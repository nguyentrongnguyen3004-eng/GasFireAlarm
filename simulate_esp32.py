"""
Tool Giả Lập Mạch ESP32 IoT (Chạy Local trên VS Code Terminal)
Đề tài: Hệ thống cảnh báo rò rỉ khí gas và cháy thông minh (HUIT)
"""

import time
import json
import urllib.request
import threading
import sys
import math

LOCAL_API_URL = "http://localhost:5000/api/sensor"
FIREBASE_URL = "https://smartgas-5b127-default-rtdb.asia-southeast1.firebasedatabase.app/system.json"
FIREBASE_CONTROLS_URL = "https://smartgas-5b127-default-rtdb.asia-southeast1.firebasedatabase.app/controls.json"

# Trạng thái thiết bị giả lập
state = {
    "gas": 420,
    "temp": 28.5,
    "gasAlarm": False,
    "tempAlarm": False,
    "danger": False,
    "valveClosed": False,
    "fanActive": False,
    "cookingMode": False
}

mode = "manual"  # 'manual' hoặc 'auto'
running = True

def send_update():
    """Gửi dữ liệu đồng thời lên Local Server (localhost:5000) và Firebase Cloud"""
    global state
    # Xác định trạng thái logic
    thresh_gas = 3000 if state["cookingMode"] else 2500
    state["gasAlarm"] = state["gas"] >= thresh_gas
    state["tempAlarm"] = state["temp"] >= 50.0
    state["danger"] = state["gasAlarm"] and state["tempAlarm"]

    if state["danger"] or state["gasAlarm"]:
        state["valveClosed"] = True
        state["fanActive"] = True
    elif state["tempAlarm"]:
        state["fanActive"] = True
        state["valveClosed"] = True

    payload = json.dumps(state).encode('utf-8')

    # 1. Gửi lên Local Dashboard
    try:
        req = urllib.request.Request(LOCAL_API_URL, data=payload, headers={'Content-Type': 'application/json'}, method='POST')
        urllib.request.urlopen(req, timeout=1.5)
    except Exception:
        pass

    # 2. Gửi lên Firebase Realtime Database
    try:
        req_fb = urllib.request.Request(FIREBASE_URL, data=payload, headers={'Content-Type': 'application/json'}, method='PUT')
        urllib.request.urlopen(req_fb, timeout=2.0)
    except Exception:
        pass

def listen_remote_controls():
    """Lắng nghe lệnh điều khiển từ Web gửi xuống"""
    global state, running
    while running:
        try:
            req = urllib.request.Request(FIREBASE_CONTROLS_URL)
            with urllib.request.urlopen(req, timeout=2.0) as res:
                if res.status == 200:
                    data = json.loads(res.read().decode('utf-8'))
                    if data:
                        updated = False
                        if "valveClosed" in data and state["valveClosed"] != data["valveClosed"]:
                            state["valveClosed"] = data["valveClosed"]
                            print(f"\n[WEB CONTROL] Người dùng {'KHÓA' if state['valveClosed'] else 'MỞ'} Van Gas từ Web!")
                            updated = True
                        if "fanActive" in data and state["fanActive"] != data["fanActive"]:
                            state["fanActive"] = data["fanActive"]
                            print(f"\n[WEB CONTROL] Người dùng {'BẬT' if state['fanActive'] else 'TẮT'} Quạt Hút từ Web!")
                            updated = True
                        if "cookingMode" in data and state["cookingMode"] != data["cookingMode"]:
                            state["cookingMode"] = data["cookingMode"]
                            print(f"\n[WEB CONTROL] Chế độ 'Giờ nấu ăn': {'BẬT' if state['cookingMode'] else 'TẮT'}!")
                            updated = True
                        if updated:
                            send_update()
        except Exception:
            pass
        time.sleep(1.5)

def print_status():
    status_text = "AN TOÀN"
    if state["danger"]:
        status_text = "🔥 KHẨN CẤP / BÁO CHÁY 🔥"
    elif state["gasAlarm"]:
        status_text = "⚠️ RÒ RỈ KHÍ GAS"
    elif state["tempAlarm"]:
        status_text = "🌡️ NHIỆT ĐỘ QUÁ CAO"

    valve_text = "ĐÃ ĐÓNG (90°)" if state["valveClosed"] else "MỞ (0°)"
    fan_text = "BẬT" if state["fanActive"] else "TẮT"
    cook_text = "BẬT (+500)" if state["cookingMode"] else "TẮT"

    print(f"\r[STATUS] Gas: {state['gas']:4d} ADC | Temp: {state['temp']:4.1f}°C | Van: {valve_text:12s} | Quạt: {fan_text:4s} | Nấu: {cook_text:10s} | Trạng thái: {status_text}   ", end="", flush=True)

def auto_loop():
    """Vòng lặp sinh số liệu tự động mượt mà"""
    global running, mode
    step = 0
    while running:
        if mode == "auto":
            step += 0.1
            # Gas dao động từ 400 đến 2800
            base_gas = 1400 + 1300 * math.sin(step)
            state["gas"] = int(max(350, min(3500, base_gas)))
            # Nhiệt độ dao động từ 28 đến 55
            state["temp"] = round(38 + 15 * math.sin(step * 0.7), 1)
            send_update()
            print_status()
        time.sleep(1.0)

def main():
    global state, mode, running

    print("=" * 65)
    print("🔥 TOOL GIẢ LẬP ESP32 IOT CHẠY LOCAL (VS CODE) 🔥")
    print("=" * 65)
    print("Các phím tắt điều khiển nhanh để demo:")
    print(" [1] - Trạng thái An toàn (Gas: 420 ADC, Temp: 28.5°C)")
    print(" [2] - Báo động Rò rỉ Gas (Gas: 2800 ADC, Temp: 30.0°C)")
    print(" [3] - Báo động Quá nhiệt (Gas: 450 ADC, Temp: 56.0°C)")
    print(" [4] - BÁO ĐỘNG ĐỎ KHẨN CẤP (Gas: 3200 ADC, Temp: 62.0°C)")
    print(" [5] - Bật / Tắt Giờ nấu ăn (Smart Cooking Mode)")
    print(" [6] - Chế độ Tự động sinh dữ liệu lượn sóng (Auto Demo)")
    print(" [q] - Thoát chương trình")
    print("=" * 65)

    # Khởi động luồng điều khiển web và tự động
    t_ctrl = threading.Thread(target=listen_remote_controls, daemon=True)
    t_ctrl.start()

    t_auto = threading.Thread(target=auto_loop, daemon=True)
    t_auto.start()

    send_update()
    print_status()

    while running:
        try:
            choice = input().strip().lower()
            if choice == '1':
                mode = "manual"
                state["gas"] = 420
                state["temp"] = 28.5
                state["valveClosed"] = False
                state["fanActive"] = False
                send_update()
                print_status()
            elif choice == '2':
                mode = "manual"
                state["gas"] = 2850
                state["temp"] = 30.2
                send_update()
                print_status()
            elif choice == '3':
                mode = "manual"
                state["gas"] = 480
                state["temp"] = 56.5
                send_update()
                print_status()
            elif choice == '4':
                mode = "manual"
                state["gas"] = 3300
                state["temp"] = 62.8
                send_update()
                print_status()
            elif choice == '5':
                state["cookingMode"] = not state["cookingMode"]
                send_update()
                print_status()
            elif choice == '6':
                mode = "auto" if mode != "auto" else "manual"
                print(f"\n[CHẾ ĐỘ] Đã chuyển sang: {'TỰ ĐỘNG SINH SỐ LIỆU' if mode == 'auto' else 'THỦ CÔNG'}")
                print_status()
            elif choice == 'q':
                running = False
                print("\nĐã dừng giả lập.")
                break
        except (KeyboardInterrupt, EOFError):
            running = False
            break

if __name__ == "__main__":
    main()
