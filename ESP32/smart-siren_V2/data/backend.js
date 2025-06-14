// ======================
// BACKEND.JS - ESP32 Smart Siren System
// Handles authentication, data fetching, and real-time updates
// ======================

// --- Global Variables ---
let currentUser = null;
let alarms = [];
let activityLogs = [];
let systemStatus = {
  wifi: { connected: false, ssid: "" },
  siren: { status: "inactive", lastRing: null },
  bell: { status: "inactive", lastRing: null },
  power: { source: "main", battery: 100 }
};

// --- DOM Elements ---
const loginForm = document.getElementById("login-form");
const dashboard = document.getElementById("dashboard");
const usernameDisplay = document.getElementById("username-display");
const roleDisplay = document.getElementById("role-display");
const wifiStatus = document.getElementById("wifi-status");
const sirenStatus = document.getElementById("siren-status");
const bellStatus = document.getElementById("bell-status");
const nextAlarm = document.getElementById("next-alarm");
const activityLog = document.getElementById("activity-log");

// --- Initialize System ---
document.addEventListener("DOMContentLoaded", async () => {
  // Load data from ESP32
  await fetchSystemStatus();
  await fetchAlarms();
  await fetchActivityLogs();

  // Check if user is already logged in (from session)
  const sessionUser = localStorage.getItem("currentUser");
  if (sessionUser) {
    currentUser = JSON.parse(sessionUser);
    showDashboard();
  } else {
    showLogin();
  }

  // Update UI every 5 seconds
  setInterval(updateUI, 5000);
});

// --- Login Function ---
loginForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  
  const username = document.getElementById("username").value;
  const password = document.getElementById("password").value;

  try {
    const response = await fetch("/api/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password })
    });

    const user = await response.json();
    
    if (user) {
      currentUser = user;
      localStorage.setItem("currentUser", JSON.stringify(user));
      showDashboard();
      logActivity(`${user.username} logged in`, "info");
    } else {
      alert("Invalid credentials!");
    }
  } catch (err) {
    console.error("Login error:", err);
    alert("Connection error. Try again.");
  }
});

// --- Logout Function ---
function logout() {
  localStorage.removeItem("currentUser");
  currentUser = null;
  showLogin();
  logActivity("User logged out", "info");
}

// --- Fetch Data from ESP32 ---
async function fetchSystemStatus() {
  try {
    const res = await fetch("/api/status");
    systemStatus = await res.json();
  } catch (err) {
    console.error("Failed to fetch system status:", err);
    systemStatus.wifi.connected = false;
  }
}

async function fetchAlarms() {
  try {
    const res = await fetch("/api/alarms");
    alarms = await res.json();
  } catch (err) {
    console.error("Failed to fetch alarms:", err);
    alarms = [];
  }
}

async function fetchActivityLogs() {
  try {
    const res = await fetch("/api/logs");
    activityLogs = await res.json();
  } catch (err) {
    console.error("Failed to fetch logs:", err);
    activityLogs = [];
  }
}

// --- Update UI Dynamically ---
function updateUI() {
  if (!currentUser) return;

  // User info
  usernameDisplay.textContent = currentUser.username;
  roleDisplay.textContent = currentUser.role;

  // System status
  wifiStatus.innerHTML = `
    <span class="status-indicator ${systemStatus.wifi.connected ? "connected" : "disconnected"}"></span>
    ${systemStatus.wifi.connected ? systemStatus.wifi.ssid : "Disconnected"}
  `;

  sirenStatus.innerHTML = `
    <span class="status-indicator ${systemStatus.siren.status}"></span>
    ${systemStatus.siren.status.toUpperCase()}
    ${systemStatus.siren.lastRing ? `<small>Last: ${formatTime(systemStatus.siren.lastRing)}</small>` : ""}
  `;

  bellStatus.innerHTML = `
    <span class="status-indicator ${systemStatus.bell.status}"></span>
    ${systemStatus.bell.status.toUpperCase()}
    ${systemStatus.bell.lastRing ? `<small>Last: ${formatTime(systemStatus.bell.lastRing)}</small>` : ""}
  `;

  // Next alarm
  const nextAlarm = getNextAlarm();
  if (nextAlarm) {
    nextAlarmDisplay.innerHTML = `
      <h3>${nextAlarm.name}</h3>
      <div class="time">${formatTime(nextAlarm.time)}</div>
      <div class="type">${nextAlarm.type === "siren" ? "🚨 Siren" : "🔔 Bell"}</div>
    `;
  }

  // Activity log
  activityLog.innerHTML = activityLogs.slice(0, 5).map(log => `
    <div class="log-entry">
      <span class="time">${formatTime(log.timestamp)}</span>
      <span class="message ${log.type}">${log.message}</span>
    </div>
  `).join("");
}

// --- Helper Functions ---
function formatTime(timestamp) {
  return new Date(timestamp).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function getNextAlarm() {
  const now = new Date();
  return alarms.find(alarm => new Date(alarm.time) > now);
}

function logActivity(message, type = "info") {
  activityLogs.unshift({
    timestamp: new Date().toISOString(),
    message,
    type
  });
  // Send to ESP32 to save
  fetch("/api/logs", {
    method: "POST",
    body: JSON.stringify({ message, type })
  });
}

// --- Page Navigation ---
function showLogin() {
  document.getElementById("login-page").style.display = "block";
  dashboard.style.display = "none";
}

function showDashboard() {
  document.getElementById("login-page").style.display = "none";
  dashboard.style.display = "block";
  updateUI();
}