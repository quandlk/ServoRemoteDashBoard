// Biến toàn cục để lưu session ID từ cookie
let sessionID = document.cookie.split('; ').find(row => row.startsWith('sessionID='))?.split('=')[1] || null;

// Hàm fetch với session ID từ cookie
function fetchWithSession(url, options = {}) {
  options.headers = options.headers || {};
  if (sessionID) {
    options.headers['X-Session-ID'] = sessionID; // Giữ header cho tương thích (tùy chọn)
  }
  options.credentials = 'include'; // Đảm bảo gửi cookie
  return fetch(url, options).then(response => {
    if (response.status === 401) {
      document.cookie = 'sessionID=; Max-Age=0; Path=/'; // Xóa cookie
      sessionID = null;
      window.location.href = '/';
      throw new Error('Unauthorized');
    }
    return response;
  });
}

// Flash Message Management
const flashMessages = []; // Danh sách lưu trữ các tin nhắn hiện tại
const MAX_FLASH_MESSAGES = 3; // Giới hạn số lượng tin nhắn tối đa

// Flash Message Function
function showFlashMessage(message, type = 'success') {
  const flash = document.createElement('div');
  flash.className = `flash-message flash-${type}`;
  flash.innerHTML = message; // Sử dụng innerHTML để hỗ trợ nút
  document.body.appendChild(flash);

  flashMessages.unshift(flash);

  if (flashMessages.length > MAX_FLASH_MESSAGES) {
    const oldestFlash = flashMessages.pop();
    if (oldestFlash) oldestFlash.remove();
  }

  flashMessages.forEach((msg, index) => {
    msg.style.top = `${20 + (index * 60)}px`;
  });

  // Chỉ tự động xóa nếu không có nút xác nhận
  if (!message.includes('<button')) {
    setTimeout(() => {
      const index = flashMessages.indexOf(flash);
      if (index !== -1) {
        flashMessages.splice(index, 1);
        flash.remove();
        flashMessages.forEach((msg, idx) => {
          msg.style.top = `${20 + (idx * 60)}px`;
        });
      }
    }, 4500);
  }
}

// Navigation
document.addEventListener('DOMContentLoaded', () => {
  sessionID = document.cookie.split('; ').find(row => row.startsWith('sessionID='))?.split('=')[1] || null;

  if (sessionID) {
    fetchWithSession('/status')
      .then(response => {
        if (!response.ok) throw new Error('Session invalid');
        return response.json();
      })
      .catch(error => {
        console.error('Session check failed:', error);
        document.cookie = 'sessionID=; Max-Age=0; Path=/';
        sessionID = null;
        showFlashMessage('Session invalid, redirecting to login...', 'error');
        setTimeout(() => window.location.href = '/', 1500);
      });
  }

  const hash = window.location.hash.substring(1) || 'home';
  const initialItem = document.querySelector(`.nav-item [data-section="${hash}"]`)?.parentElement;
  if (initialItem) {
    document.querySelectorAll('.nav-item').forEach(nav => nav.classList.remove('active'));
    initialItem.classList.add('active');
    const section = initialItem.querySelector('.nav-link').getAttribute('data-section');
    document.querySelectorAll('.content-section').forEach(content => {
      content.style.display = 'none';
    });
    document.querySelector(`.content-section.${section}`).style.display = 'flex';
    document.querySelector(`.content-buttons`).style.display = 'flex';

    if (section === "home") {
      fetchWithSession('/status')
        .then(response => response.json())
        .then(data => {
          const wifiName = document.getElementById('wifi-name');
          const wifiStrength = document.getElementById('wifi-strength');
          const localIp = document.getElementById('local-ip');
          const publicIp = document.getElementById('public-ip');
          const temperature = document.getElementById('temperature');
          if (wifiName) wifiName.textContent = data.wifiName || 'N/A';
          if (wifiStrength) wifiStrength.textContent = data.wifiStrength || 'N/A';
          if (localIp) localIp.textContent = data.localIp || 'N/A';
          if (publicIp) publicIp.textContent = data.publicIp || 'N/A';
          if (temperature) temperature.textContent = data.temperature || 'N/A';
        })
        .catch(error => {
          console.error('Error fetching status:', error);
          showFlashMessage('Failed to load system status', 'error');
          document.getElementById('wifi-name').textContent = 'Error';
          document.getElementById('wifi-strength').textContent = 'Error';
          document.getElementById('local-ip').textContent = 'Error';
          document.getElementById('public-ip').textContent = 'Error';
          document.getElementById('temperature').textContent = 'Error';
        });
    }

    if (section === "settings") {
      fetchWithSession('/get-servo-settings')
        .then(response => response.json())
        .then(data => {
          document.getElementById('servo1-angle').value = data.servo1 || 180;
          document.getElementById('servo2-angle').value = data.servo2 || 180;
          document.getElementById('servo3-angle').value = data.servo3 || 180;
        })
        .catch(error => {
          console.error('Error fetching servo settings:', error);
          showFlashMessage('Failed to load servo settings', 'error');
        });
    }
  }

  const themeSwitch = document.getElementById('theme-switch');
  const themeLabel = document.getElementById('theme-label');
  if (localStorage.getItem('theme') === 'dark') {
    document.body.classList.add('dark-mode');
    themeSwitch.checked = true;
    themeLabel.textContent = 'Light Mode';
  } else {
    themeSwitch.checked = false;
    themeLabel.textContent = 'Dark Mode';
  }

  const themeToggle = document.querySelector('.theme-toggle');
  themeToggle.addEventListener('click', (e) => {
    if (e.target !== themeSwitch) {
      themeSwitch.checked = !themeSwitch.checked;
      toggleTheme();
    }
  });
});

document.querySelectorAll('.nav-item').forEach(item => {
  item.addEventListener('click', function(e) {
    const section = this.querySelector('.nav-link').getAttribute('data-section');
    if (!section) return;

    e.preventDefault();
    document.querySelectorAll('.nav-item').forEach(nav => nav.classList.remove('active'));
    this.classList.add('active');
    window.location.hash = section;
    document.querySelectorAll('.content-section').forEach(content => {
      content.style.display = 'none';
    });
    document.querySelector(`.content-section.${section}`).style.display = 'flex';
    document.querySelector(`.content-buttons`).style.display = 'flex';

    if (section === "home") {
      fetchWithSession('/status')
        .then(response => response.json())
        .then(data => {
          const wifiName = document.getElementById('wifi-name');
          const wifiStrength = document.getElementById('wifi-strength');
          const localIp = document.getElementById('local-ip');
          const publicIp = document.getElementById('public-ip');
          const temperature = document.getElementById('temperature');
          if (wifiName) wifiName.textContent = data.wifiName || 'N/A';
          if (wifiStrength) wifiStrength.textContent = data.wifiStrength || 'N/A';
          if (localIp) localIp.textContent = data.localIp || 'N/A';
          if (publicIp) publicIp.textContent = data.publicIp || 'N/A';
          if (temperature) temperature.textContent = data.temperature || 'N/A';
        })
        .catch(error => {
          console.error('Error fetching status:', error);
          showFlashMessage('Failed to load system status', 'error');
          document.getElementById('wifi-name').textContent = 'Wi-Fi Name: Error';
          document.getElementById('wifi-strength').textContent = 'Wi-Fi Strength: Error';
          document.getElementById('local-ip').textContent = 'Local IP: Error';
          document.getElementById('public-ip').textContent = 'Public IP: Error';
          document.getElementById('temperature').textContent = 'Temperature: Error';
        });
    }

    if (section === "settings") {
      fetchWithSession('/get-servo-settings')
        .then(response => response.json())
        .then(data => {
          document.getElementById('servo1-angle').value = data.servo1 || 180;
          document.getElementById('servo2-angle').value = data.servo2 || 180;
          document.getElementById('servo3-angle').value = data.servo3 || 180;
        })
        .catch(error => {
          console.error('Error fetching servo settings:', error);
          showFlashMessage('Failed to load servo settings', 'error');
        });
    }
  });
});

const sidebar = document.querySelector(".sidebar");
const sidebarToggler = document.querySelector(".sidebar-toggler");
const menuToggler = document.querySelector(".menu-toggler");

let collapsedSidebarHeight = "56px";
let fullSidebarHeight = "calc(100vh - 32px)";

sidebarToggler.addEventListener("click", () => {
  sidebar.classList.toggle("collapsed");
});

const toggleMenu = (isMenuActive) => {
  sidebar.style.height = isMenuActive ? `${sidebar.scrollHeight}px` : collapsedSidebarHeight;
  menuToggler.querySelector("span").innerText = isMenuActive ? "close" : "menu";
};

menuToggler.addEventListener("click", () => {
  toggleMenu(sidebar.classList.toggle("menu-active"));
});

window.addEventListener("resize", () => {
  if (window.innerWidth >= 1024) {
    sidebar.style.height = fullSidebarHeight;
    sidebar.classList.remove("collapsed");
  } else {
    sidebar.classList.remove("collapsed");
    sidebar.style.height = "auto";
    toggleMenu(sidebar.classList.contains("menu-active"));
  }
});

// Functions
// Hàm điều khiển servo
function controlServo(servoId) {
  const servoElement = document.getElementById(`servo${servoId}`);
  if (servoElement && servoElement.classList.contains('disabled')) {
    showFlashMessage(`Servo ${servoId} is already moving`, 'warning');
    return;
  }

  fetchWithSession(`/servo${servoId}`, { method: 'GET' })
    .then(response => {
      if (!response.ok) {
        if (response.status === 409) {
          showFlashMessage(`Servo ${servoId} is already moving`, 'warning');
        } else {
          throw new Error('Network response was not ok');
        }
      }
      return response.text();
    })
    .then(data => {
      console.log(data);
      showFlashMessage(`Servo ${servoId} moved`, 'success');
    })
    .catch(error => {
      console.error('Error:', error);
      showFlashMessage(`Failed to move Servo ${servoId}`, 'error');
    });
}

// Hàm cập nhật trạng thái servo
function updateServoStatus(servoId, isMoving) {
  const statusElement = document.getElementById(`servo${servoId}-status`);
  const servoElement = document.getElementById(`servo${servoId}`);
  const servoIcon = servoElement ? servoElement.querySelector('.servo-icon') : null;

  if (statusElement && servoElement && servoIcon) {
    if (isMoving) {
      statusElement.textContent = 'Moving';
      statusElement.style.color = 'red';
      servoElement.classList.add('disabled');
      servoIcon.textContent = 'radio_button_checked';
    } else {
      statusElement.textContent = 'Idle';
      statusElement.style.color = 'green';
      servoElement.classList.remove('disabled');
      servoIcon.textContent = 'radio_button_unchecked';
    }
  } else {
    console.error(`Element with ID 'servo${servoId}-status' or 'servo${servoId}' not found`);
  }
}

// Kết nối WebSocket và quản lý trạng thái kết nối
let reconnectTimeout;
let isConnected = false;
let lastMessageTime = Date.now(); // Thời gian nhận tin nhắn cuối cùng

const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onopen = () => {
  console.log('WebSocket connected');
  isConnected = true;
  lastMessageTime = Date.now(); // Cập nhật thời gian khi kết nối
  showFlashMessage('WebSocket connected', 'success');
  clearTimeout(reconnectTimeout); // Xóa timeout nếu kết nối thành công
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('WebSocket data received:', data);
  lastMessageTime = Date.now(); // Cập nhật thời gian nhận tin nhắn

  // Cập nhật trạng thái hệ thống
  const wifiName = document.getElementById('wifi-name');
  const wifiStrength = document.getElementById('wifi-strength');
  const localIp = document.getElementById('local-ip');
  const publicIp = document.getElementById('public-ip');
  const temperature = document.getElementById('temperature');
  if (wifiName) wifiName.textContent = data.wifiName || 'N/A';
  if (wifiStrength) wifiStrength.textContent = data.wifiStrength || 'N/A';
  if (localIp) localIp.textContent = data.localIp || 'N/A';
  if (publicIp) publicIp.textContent = data.publicIp || 'N/A';
  if (temperature) temperature.textContent = data.temperature || 'N/A';

  // Cập nhật trạng thái servo
  updateServoStatus(1, data.servo1Moving);
  updateServoStatus(2, data.servo2Moving);
  updateServoStatus(3, data.servo3Moving);
};

ws.onclose = () => {
  console.log('WebSocket disconnected');
  if (isConnected) {
    showFlashMessage('Lost connection to server. Attempting to reconnect...', 'warning');
  }
  isConnected = false;
  attemptReconnect();
};

ws.onerror = (error) => {
  console.error('WebSocket error:', error);
  if (!isConnected) {
    showFlashMessage('WebSocket connection error', 'error');
  }
};

// Hàm thử kết nối lại
function attemptReconnect() {
  clearTimeout(reconnectTimeout);
  reconnectTimeout = setTimeout(() => {
    console.log('Attempting to reconnect...');
    const newWs = new WebSocket(`ws://${window.location.hostname}/ws`);
    newWs.onopen = () => {
      ws = newWs; // Cập nhật ws với kết nối mới
      isConnected = true;
      lastMessageTime = Date.now();
      showFlashMessage('Reconnected to server', 'success');
    };
    newWs.onmessage = ws.onmessage;
    newWs.onclose = ws.onclose;
    newWs.onerror = ws.onerror;
  }, 2000); // Thử lại sau 2 giây
}

// Kiểm tra mất kết nối dựa trên thời gian không nhận được tin nhắn
setInterval(() => {
  const now = Date.now();
  const timeSinceLastMessage = now - lastMessageTime;

  // Nếu đã kết nối nhưng không nhận được tin nhắn trong 10 giây
  if (isConnected && timeSinceLastMessage > 10000) {
    console.log('No messages received for 10 seconds, assuming disconnected');
    showFlashMessage('Lost connection to server. Attempting to reconnect...', 'warning');
    isConnected = false;
    ws.close(); // Đóng kết nối WebSocket để kích hoạt ws.onclose
    attemptReconnect();
  }

  // Kiểm tra trạng thái WebSocket
  if (isConnected && ws.readyState !== WebSocket.OPEN) {
    console.log('WebSocket state is not OPEN, assuming disconnected');
    showFlashMessage('Lost connection to server. Attempting to reconnect...', 'warning');
    isConnected = false;
    attemptReconnect();
  }
}, 3000); // Kiểm tra mỗi 3 giây

function changePassword() {
  const currentPassword = document.getElementById('current-password').value;
  const newPassword = document.getElementById('new-password').value;
  const confirmPassword = document.getElementById('confirm-password').value;

  if (!currentPassword || !newPassword || !confirmPassword) {
    showFlashMessage('Please fill in all password fields', 'warning');
    return;
  }

  if (newPassword !== confirmPassword) {
    showFlashMessage('New password and confirmation do not match', 'error');
    return;
  }

  fetchWithSession('/change-password', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ currentPassword, newPassword })
  })
    .then(response => response.text())
    .then(data => {
      if (data === "Password changed successfully") {
        showFlashMessage(data, 'success');
        document.getElementById('current-password').value = '';
        document.getElementById('new-password').value = '';
        document.getElementById('confirm-password').value = '';
      } else {
        showFlashMessage(data, 'error');
      }
    })
    .catch(error => {
      console.error('Error changing password:', error);
      showFlashMessage('Failed to change password', 'error');
    });
}

function saveServoSettings() {
  const servo1Value = document.getElementById('servo1-angle').value;
  const servo2Value = document.getElementById('servo2-angle').value;
  const servo3Value = document.getElementById('servo3-angle').value;

  if (!servo1Value || !servo2Value || !servo3Value) {
    showFlashMessage('Please enter angles for all servos', 'warning');
    return;
  }

  const servo1 = parseInt(servo1Value);
  const servo2 = parseInt(servo2Value);
  const servo3 = parseInt(servo3Value);

  if (isNaN(servo1) || isNaN(servo2) || isNaN(servo3) || 
      servo1 < 0 || servo1 > 180 || 
      servo2 < 0 || servo2 > 180 || 
      servo3 < 0 || servo3 > 180) {
    showFlashMessage('Angles must be numbers between 0 and 180', 'error');
    return;
  }

  const angles = { servo1, servo2, servo3 };

  fetchWithSession('/save-servo-settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(angles)
  })
    .then(response => response.text())
    .then(data => {
      showFlashMessage(data, 'success');
    })
    .catch(error => {
      console.error('Error saving servo settings:', error);
      showFlashMessage('Failed to save servo settings', 'error');
    });
}

function connectWiFi() {
  const ssid = document.getElementById('ssid').value;
  const wifiPassword = document.getElementById('wifi-password').value;

  if (!ssid || !wifiPassword) {
    showFlashMessage('Please enter both SSID and password', 'warning');
    return;
  }

  // Hiển thị flash message để xác nhận với nút OK và Cancel
  showFlashMessage(
    `Please verify: SSID: ${ssid}, Password: ${wifiPassword}. Proceed with update?<br>` +
    `<button onclick="proceedWiFiUpdate('${ssid}', '${wifiPassword}')">OK</button> ` +
    `<button onclick="cancelWiFiUpdate()">Cancel</button>`,
    'warning'
  );
}

function proceedWiFiUpdate(ssid, wifiPassword) {
  const wifiData = { ssid, password: wifiPassword };

  fetchWithSession('/connect-wifi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(wifiData)
  })
    .then(response => response.text())
    .then(data => {
      showFlashMessage(data, 'success'); // Hiển thị "WiFi credentials updated, restarting ESP32..."
      document.getElementById('wifi-password').value = ''; // Xóa trường mật khẩu
      setTimeout(() => {
        window.location.reload(); // Tải lại trang sau khi ESP32 khởi động lại
      }, 5000); // Chờ 5 giây để ESP32 khởi động
    })
    .catch(error => {
      console.error('Error connecting WiFi:', error);
      showFlashMessage('Failed to update WiFi credentials', 'error');
    });
}

function cancelWiFiUpdate() {
  showFlashMessage('WiFi update cancelled', 'info');
}

// Toggle Theme
function toggleTheme() {
  document.body.classList.toggle('dark-mode');
  const isDarkMode = document.body.classList.contains('dark-mode');
  localStorage.setItem('theme', isDarkMode ? 'dark' : 'light');
  document.getElementById('theme-switch').checked = isDarkMode;
  document.getElementById('theme-label').textContent = isDarkMode ? 'Light Mode' : 'Dark Mode';
  showFlashMessage(`Switched to ${isDarkMode ? 'Dark' : 'Light'} Mode`, 'success');
}

function logout() {
  fetchWithSession('/logout', { method: 'POST' })
    .then(() => {
      localStorage.removeItem('sessionID');
      sessionID = null;
      showFlashMessage('Logged out successfully', 'success');
      setTimeout(() => window.location.href = '/', 1000);
    })
    .catch(error => {
      console.error('Error logging out:', error);
      showFlashMessage('Failed to log out', 'error');
    });
}