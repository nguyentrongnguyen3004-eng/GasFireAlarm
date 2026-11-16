const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');
const ExcelJS = require('exceljs');

const PORT = 5000;
const FIREBASE_HOST = "smartgas-5b127-default-rtdb.asia-southeast1.firebasedatabase.app";

// Bộ nhớ đệm lưu trạng thái thiết bị
let deviceState = {
  gas: 420,
  temp: 28.5,
  gasAlarm: false,
  tempAlarm: false,
  danger: false,
  valveClosed: false,
  fanActive: false,
  cookingMode: false,
  gasThreshold: 1800,
  tempThreshold: 50.0,
  lastUpdate: new Date().toISOString()
};

// Danh sách các client đang lắng nghe SSE (Server-Sent Events)
let clients = [];
let pendingUserControls = {}; // Khóa chống rollback trạng thái khi người dùng vừa bấm nút

// ========================================================
// LƯU TRỮ VĨNH VIỄN NHẬT KÝ SỰ CỐ (SURVIVES F5 & REBOOTS)
// ========================================================
const LOGS_FILE = path.join(__dirname, 'logs_history.json');
let persistedLogs = [];
try {
  if (fs.existsSync(LOGS_FILE)) {
    persistedLogs = JSON.parse(fs.readFileSync(LOGS_FILE, 'utf-8'));
  }
} catch (e) {
  persistedLogs = [];
}

function saveLogsToFile() {
  try {
    fs.writeFileSync(LOGS_FILE, JSON.stringify(persistedLogs.slice(0, 300), null, 2), 'utf-8');
  } catch (e) {}
}

function appendServerLog(msg, colorClass = "text-slate-500") {
  if (!msg) return;
  const time = new Date().toLocaleTimeString('vi-VN');
  const date = new Date().toLocaleDateString('vi-VN');
  // Chống ghi lặp cùng 1 thông điệp liên tiếp trong vòng 3 giây
  if (persistedLogs.length > 0 && persistedLogs[0].msg === msg && (Date.now() - (persistedLogs[0].timestamp || 0) < 3000)) {
    return;
  }
  const item = {
    id: Date.now() + '-' + Math.random().toString(36).substr(2, 4),
    time,
    date,
    msg,
    colorClass,
    timestamp: Date.now()
  };
  persistedLogs.unshift(item);
  if (persistedLogs.length > 300) persistedLogs.pop();
  saveLogsToFile();
}

// ========================================================
// TỰ ĐỘNG ĐỒNG BỘ 2 CHIỀU VỚI FIREBASE REALTIME DATABASE
// ========================================================
function fetchFirebaseData() {
  const options = {
    hostname: FIREBASE_HOST,
    port: 443,
    path: '/system.json',
    method: 'GET'
  };

  const req = https.request(options, (res) => {
    let raw = '';
    res.on('data', chunk => raw += chunk);
    res.on('end', () => {
      if (res.statusCode === 200 && raw && raw !== 'null') {
        try {
          const fbData = JSON.parse(raw);
          let changed = false;
          const now = Date.now();

          for (let key in fbData) {
            // Nếu người dùng vừa bấm điều khiển thủ công phím này trong vòng 4s
            if (pendingUserControls[key] && (now - pendingUserControls[key].timestamp < 4000)) {
              if (fbData[key] === pendingUserControls[key].value) {
                delete pendingUserControls[key]; // Thiết bị thật đã khớp trạng thái
              } else {
                continue; // Chưa khớp thì giữ nguyên lệnh của người dùng, không cho Firebase đè lại
              }
            }

            if (deviceState[key] !== fbData[key]) {
              changed = true;
              deviceState[key] = fbData[key];
            }
          }

          if (changed) {
            deviceState.lastUpdate = new Date().toISOString();
            // Phát cho toàn bộ Web Dashboard đang mở
            clients.forEach(client => {
              client.write(`data: ${JSON.stringify(deviceState)}\n\n`);
            });
          }
        } catch (e) {}
      }
    });
  });
  req.on('error', () => {});
  req.end();
}

// Kéo dữ liệu từ Firebase mỗi 250ms (giảm độ trễ tối đa)
setInterval(fetchFirebaseData, 250);

// Gửi lệnh điều khiển lên Firebase (/controls.json và /system.json)
function sendControlsToFirebase(controls) {
  const payload = JSON.stringify(controls);
  
  // 1. Cập nhật nhánh /controls.json để ESP32 kéo về
  const optControls = {
    hostname: FIREBASE_HOST,
    port: 443,
    path: '/controls.json',
    method: 'PATCH',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(payload)
    }
  };
  const reqControls = https.request(optControls, () => {});
  reqControls.on('error', () => {});
  reqControls.write(payload);
  reqControls.end();

  // 2. Cập nhật ngay nhánh /system.json để Web không bị giật lùi trạng thái
  const optSystem = {
    hostname: FIREBASE_HOST,
    port: 443,
    path: '/system.json',
    method: 'PATCH',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(payload)
    }
  };
  const reqSystem = https.request(optSystem, () => {});
  reqSystem.on('error', () => {});
  reqSystem.write(payload);
  reqSystem.end();
}

// ========================================================
// HÀM TẠO BÁO CÁO EXCEL ĐỊNH DẠNG ĐẲNG CẤP VỚI EXCELJS
// ========================================================
async function createStyledExcelWorkbook(logEntries) {
  const workbook = new ExcelJS.Workbook();
  workbook.creator = 'Hệ Thống IoT HUIT';
  workbook.lastModifiedBy = 'Admin Lab IoT';
  workbook.created = new Date();

  const ws = workbook.addWorksheet('Nhật Ký Vận Hành', {
    views: [{ showGridLines: true }]
  });

  // Độ rộng các cột
  ws.columns = [
    { key: 'stt', width: 9 },
    { key: 'time', width: 22 },
    { key: 'msg', width: 68 },
    { key: 'type', width: 22 }
  ];

  // Hàng 1: Cơ quan / Trường học
  ws.mergeCells('A1:D1');
  const cellA1 = ws.getCell('A1');
  cellA1.value = 'TRƯỜNG ĐẠI HỌC CÔNG THƯƠNG TP. HỒ CHÍ MINH (HUIT) - KHOA CÔNG NGHỆ THÔNG TIN';
  cellA1.font = { name: 'Calibri', size: 10, bold: true, color: { argb: 'FF1E3A8A' } };
  cellA1.alignment = { vertical: 'middle', horizontal: 'left' };

  // Hàng 2: Tên đề tài môn học
  ws.mergeCells('A2:D2');
  const cellA2 = ws.getCell('A2');
  cellA2.value = 'HỆ THỐNG CẢNH BÁO RÒ RỈ KHÍ GAS VÀ CHÁY THÔNG MINH (SMART IOT GAS & FIRE ALERT)';
  cellA2.font = { name: 'Calibri', size: 9, italic: true, color: { argb: 'FF64748B' } };
  cellA2.alignment = { vertical: 'middle', horizontal: 'left' };

  // Hàng 4: Banner Tiêu Đề Báo Cáo
  ws.mergeCells('A4:D4');
  const cellTitle = ws.getCell('A4');
  cellTitle.value = 'BÁO CÁO NHẬT KÝ SỰ CỐ & VẬN HÀNH THIẾT BỊ';
  cellTitle.font = { name: 'Calibri', size: 13, bold: true, color: { argb: 'FFFFFFFF' } };
  cellTitle.fill = {
    type: 'pattern',
    pattern: 'solid',
    fgColor: { argb: 'FF0F766E' } // Teal sang trọng
  };
  cellTitle.alignment = { vertical: 'middle', horizontal: 'center' };
  ws.getRow(4).height = 30;

  // Hàng 5: Metadata thời gian
  ws.mergeCells('A5:D5');
  const cellMeta = ws.getCell('A5');
  const nowStr = new Date().toLocaleString('vi-VN');
  cellMeta.value = `Thời gian xuất báo cáo: ${nowStr}  |  Tổng số bản ghi: ${logEntries.length} sự kiện  |  Kênh truyền: Firebase RTDB`;
  cellMeta.font = { name: 'Calibri', size: 9, italic: true, color: { argb: 'FF334155' } };
  cellMeta.fill = {
    type: 'pattern',
    pattern: 'solid',
    fgColor: { argb: 'FFF1F5F9' }
  };
  cellMeta.alignment = { vertical: 'middle', horizontal: 'center' };
  ws.getRow(5).height = 20;

  // Hàng 7: Tiêu đề cột
  const headerRow = ws.getRow(7);
  headerRow.values = ['STT', 'Thời Gian Ghi Nhận', 'Chi Tiết Sự Kiện & Hành Động', 'Phân Loại Mức Độ'];
  headerRow.height = 26;
  headerRow.eachCell((cell) => {
    cell.font = { name: 'Calibri', size: 10.5, bold: true, color: { argb: 'FFFFFFFF' } };
    cell.fill = {
      type: 'pattern',
      pattern: 'solid',
      fgColor: { argb: 'FF1E293B' } // Dark Slate
    };
    cell.alignment = { vertical: 'middle', horizontal: 'center' };
    cell.border = {
      top: { style: 'thin', color: { argb: 'FF94A3B8' } },
      bottom: { style: 'medium', color: { argb: 'FF0F172A' } },
      left: { style: 'thin', color: { argb: 'FF94A3B8' } },
      right: { style: 'thin', color: { argb: 'FF94A3B8' } }
    };
  });

  // Hàng dữ liệu
  const startRow = 8;
  logEntries.forEach((item, index) => {
    const row = ws.getRow(startRow + index);
    const stt = logEntries.length - index;
    const time = item.time || '';
    const msg = item.msg || '';

    let tag = "BÌNH THƯỜNG";
    let tagBg = 'FFDCFCE7'; // Light green
    let tagFg = 'FF166534'; // Dark green

    if (msg.includes("BÁO ĐỘNG ĐỎ")) {
      tag = "🔥 BÁO ĐỘNG ĐỎ";
      tagBg = 'FFFEE2E2';
      tagFg = 'FF991B1B';
    } else if (msg.includes("CẢNH BÁO")) {
      tag = "⚠️ CẢNH BÁO GAS";
      tagBg = 'FFFEF3C7';
      tagFg = 'FF92400E';
    } else if (msg.includes("LỆNH ĐIỀU KHIỂN") || msg.includes("ĐIỀU KHIỂN")) {
      tag = "🎮 ĐIỀU KHIỂN";
      tagBg = 'FFE0F2FE';
      tagFg = 'FF0369A1';
    } else if (msg.includes("Giờ nấu ăn") || msg.includes("GIỜ NẤU")) {
      tag = "🍳 GIỜ NẤU ĂN";
      tagBg = 'FFFFEDD5';
      tagFg = 'FFC2410C';
    }

    row.values = [stt, time, msg, tag];
    row.height = 22;

    const rowBg = (index % 2 === 0) ? 'FFFFFFFF' : 'FFF8FAFC';

    // Cell 1: STT
    row.getCell(1).alignment = { vertical: 'middle', horizontal: 'center' };
    row.getCell(1).font = { name: 'Calibri', size: 10, bold: true, color: { argb: 'FF475569' } };
    row.getCell(1).fill = { type: 'pattern', pattern: 'solid', fgColor: { argb: rowBg } };

    // Cell 2: Time
    row.getCell(2).alignment = { vertical: 'middle', horizontal: 'center' };
    row.getCell(2).font = { name: 'Calibri', size: 10, color: { argb: 'FF334155' } };
    row.getCell(2).fill = { type: 'pattern', pattern: 'solid', fgColor: { argb: rowBg } };

    // Cell 3: Msg
    row.getCell(3).alignment = { vertical: 'middle', horizontal: 'left', wrapText: true };
    row.getCell(3).font = { name: 'Calibri', size: 9.5, color: { argb: 'FF0F172A' } };
    row.getCell(3).fill = { type: 'pattern', pattern: 'solid', fgColor: { argb: rowBg } };

    // Cell 4: Tag
    row.getCell(4).alignment = { vertical: 'middle', horizontal: 'center' };
    row.getCell(4).font = { name: 'Calibri', size: 9.5, bold: true, color: { argb: tagFg } };
    row.getCell(4).fill = { type: 'pattern', pattern: 'solid', fgColor: { argb: tagBg } };

    for (let c = 1; c <= 4; c++) {
      row.getCell(c).border = {
        top: { style: 'thin', color: { argb: 'FFE2E8F0' } },
        bottom: { style: 'thin', color: { argb: 'FFE2E8F0' } },
        left: { style: 'thin', color: { argb: 'FFE2E8F0' } },
        right: { style: 'thin', color: { argb: 'FFE2E8F0' } }
      };
    }
  });

  const endRow = startRow + logEntries.length + 1;
  ws.mergeCells(`A${endRow}:D${endRow}`);
  const footerNote = ws.getCell(`A${endRow}`);
  footerNote.value = `* Báo cáo này được kết xuất tự động từ hệ thống IoT ESP32 & Web Dashboard. Dữ liệu thời gian thực được lưu trữ qua Firebase Realtime Database.`;
  footerNote.font = { name: 'Calibri', size: 8.5, italic: true, color: { argb: 'FF64748B' } };
  footerNote.alignment = { vertical: 'middle', horizontal: 'left' };

  const signRow = endRow + 2;
  ws.mergeCells(`A${signRow}:B${signRow}`);
  const signLeft = ws.getCell(`A${signRow}`);
  signLeft.value = 'NGƯỜI LẬP BÁO CÁO\n(Ký và ghi rõ họ tên)';
  signLeft.font = { name: 'Calibri', size: 9.5, bold: true };
  signLeft.alignment = { vertical: 'middle', horizontal: 'center', wrapText: true };

  ws.mergeCells(`C${signRow}:D${signRow}`);
  const signRight = ws.getCell(`C${signRow}`);
  signRight.value = 'GIẢNG VIÊN HƯỚNG DẪN\n(Ký và ghi rõ họ tên)';
  signRight.font = { name: 'Calibri', size: 9.5, bold: true };
  signRight.alignment = { vertical: 'middle', horizontal: 'center', wrapText: true };
  ws.getRow(signRow).height = 42;

  return workbook;
}

const server = http.createServer((req, res) => {
  // Bật CORS cho mọi request
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  const url = new URL(req.url, `http://${req.headers.host}`);

  // 1. Phục vụ trang HTML chính
  if (url.pathname === '/' || url.pathname === '/index.html') {
    const filePath = path.join(__dirname, 'index.html');
    fs.readFile(filePath, (err, data) => {
      if (err) {
        res.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
        res.end('Không tìm thấy file index.html');
        return;
      }
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(data);
    });
    return;
  }

  // 2. API nhận dữ liệu từ ESP32 gửi lên (nếu dùng local)
  if (url.pathname === '/api/sensor' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        const now = Date.now();
        for (let k in data) {
          pendingUserControls[k] = { value: data[k], timestamp: now };
        }
        deviceState = { ...deviceState, ...data, lastUpdate: new Date().toISOString() };

        // Phát tín hiệu cập nhật cho các web đang mở
        clients.forEach(client => {
          client.write(`data: ${JSON.stringify(deviceState)}\n\n`);
        });

        res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ status: 'success', controls: {
          valveClosed: deviceState.valveClosed,
          fanActive: deviceState.fanActive,
          cookingMode: deviceState.cookingMode,
          gasThreshold: deviceState.gasThreshold,
          tempThreshold: deviceState.tempThreshold
        }}));
      } catch (e) {
        res.writeHead(400, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ status: 'error', message: 'Invalid JSON' }));
      }
    });
    return;
  }

  // 3. API điều khiển thiết bị từ Web xuống
  if (url.pathname === '/api/control' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        const now = Date.now();
        for (let k in data) {
          pendingUserControls[k] = { value: data[k], timestamp: now };
        }
        deviceState = { ...deviceState, ...data, lastUpdate: new Date().toISOString() };

        // Gửi sang cả Firebase để Wokwi cập nhật
        sendControlsToFirebase(data);

        clients.forEach(client => {
          client.write(`data: ${JSON.stringify(deviceState)}\n\n`);
        });

        res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ status: 'success', state: deviceState }));
      } catch (e) {
        res.writeHead(400, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ status: 'error', message: 'Invalid JSON' }));
      }
    });
    return;
  }

  // 4. API đọc trạng thái hiện tại
  if (url.pathname === '/api/state' && req.method === 'GET') {
    res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify(deviceState));
    return;
  }

  // 5. Server-Sent Events (SSE) để Web nhận dữ liệu tức thì không cần F5
  if (url.pathname === '/api/events') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream; charset=utf-8',
      'Cache-Control': 'no-cache',
      'Connection': 'keep-alive'
    });
    res.write(`data: ${JSON.stringify(deviceState)}\n\n`);
    clients.push(res);

    req.on('close', () => {
      clients = clients.filter(c => c !== res);
    });
    return;
  }

  // 6. API Xuất Báo Cáo Excel Đầy Đủ Định Dạng Sang Trọng (ExcelJS)
  if (url.pathname === '/api/export-excel' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', async () => {
      try {
        const payload = JSON.parse(body || '{}');
        const logs = payload.logEntries || [];
        const workbook = await createStyledExcelWorkbook(logs);
        const fileName = `BaoCao_HeThongGasChay_HUIT_${new Date().toISOString().slice(0, 10)}.xlsx`;
        
        res.writeHead(200, {
          'Content-Type': 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
          'Content-Disposition': `attachment; filename="${fileName}"`
        });
        await workbook.xlsx.write(res);
        res.end();
      } catch (err) {
        res.writeHead(500, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ error: err.message }));
      }
    });
    return;
  }

  // 7. API Quản lý Nhật ký Sự cố Vĩnh viễn (GET / POST / DELETE)
  if (url.pathname === '/api/logs' && req.method === 'GET') {
    res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify(persistedLogs));
    return;
  }

  if (url.pathname === '/api/logs' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const item = JSON.parse(body || '{}');
        if (item && item.msg) {
          appendServerLog(item.msg, item.colorClass);
        }
        res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ status: 'success', count: persistedLogs.length }));
      } catch (e) {
        res.writeHead(400, { 'Content-Type': 'application/json; charset=utf-8' });
        res.end(JSON.stringify({ error: 'Invalid JSON' }));
      }
    });
    return;
  }

  if (url.pathname === '/api/logs' && req.method === 'DELETE') {
    persistedLogs = [];
    saveLogsToFile();
    res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify({ status: 'cleared' }));
    return;
  }

  // Nếu không khớp route nào
  res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
  res.end('Route không tồn tại');
});

server.listen(PORT, () => {
  console.log(`=======================================================`);
  console.log(`🚀 WEB DASHBOARD IOT ĐÃ KHỞI ĐỘNG THÀNH CÔNG!`);
  console.log(`🔗 Truy cập trực tiếp tại: http://localhost:${PORT}`);
  console.log(`☁️ Firebase Cloud Sync:    https://${FIREBASE_HOST}`);
  console.log(`📡 API Nhận dữ liệu ESP32: http://localhost:${PORT}/api/sensor`);
  console.log(`🎮 API Điều khiển:         http://localhost:${PORT}/api/control`);
  console.log(`=======================================================`);
});
