// Smart Siren System JavaScript
let currentUser = null;
let schedules = JSON.parse(localStorage.getItem('smartSirenSchedules') || '[]');
let statusUpdateInterval;

// Initialize application
window.onload = function() {
    console.log('Smart Siren System Loading...');
    checkAuth();
    loadSchedules();
    startStatusUpdates();
};

// Authentication Management
function checkAuth() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            if (data.authenticated) {
                currentUser = data.user;
                document.getElementById('currentUser').textContent = data.user;
                showMainApp();
            } else {
                showLoginForm();
            }
        })
        .catch(error => {
            console.error('Auth check failed:', error);
            showLoginForm();
        });
}

function login() {
    const username = document.getElementById('username').value.trim();
    const password = document.getElementById('password').value.trim();
    const errorDiv = document.getElementById('loginError');
    
    if (!username || !password) {
        errorDiv.textContent = 'Please enter both username and password';
        return;
    }
    
    // Show loading state
    const loginBtn = event.target;
    const originalText = loginBtn.textContent;
    loginBtn.innerHTML = '<span class="loading"></span> Logging in...';
    loginBtn.disabled = true;
    
    fetch('/api/login', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({username, password})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            errorDiv.textContent = '';
            checkAuth();
        } else {
            errorDiv.textContent = data.message || 'Invalid credentials';
        }
    })
    .catch(error => {
        console.error('Login failed:', error);
        errorDiv.textContent = 'Connection error. Please try again.';
    })
    .finally(() => {
        loginBtn.textContent = originalText;
        loginBtn.disabled = false;
    });
}

function logout() {
    fetch('/api/logout', {method: 'POST'})
        .then(() => {
            currentUser = null;
            stopStatusUpdates();
            showLoginForm();
        })
        .catch(error => {
            console.error('Logout failed:', error);
        });
}

// UI Management
function showLoginForm() {
    document.getElementById('loginForm').classList.remove('hidden');
    document.getElementById('mainApp').classList.add('hidden');
    document.getElementById('username').value = '';
    document.getElementById('password').value = '';
    document.getElementById('loginError').textContent = '';
}

function showMainApp() {
    document.getElementById('loginForm').classList.add('hidden');
    document.getElementById('mainApp').classList.remove('hidden');
}

// Status Updates
function startStatusUpdates() {
    updateStatus();
    statusUpdateInterval = setInterval(updateStatus, 1000); // Update every second
}

function stopStatusUpdates() {
    if (statusUpdateInterval) {
        clearInterval(statusUpdateInterval);
    }
}

function updateStatus() {
    if (!currentUser) return;
    
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            const statusDiv = document.getElementById('systemStatus');
            
            if (data.sirenActive) {
                statusDiv.textContent = '🚨 SIREN ACTIVE';
                statusDiv.className = 'status active';
            } else if (data.bellActive) {
                statusDiv.textContent = '🔔 BELL RINGING';
                statusDiv.className = 'status active';
            } else {
                statusDiv.textContent = '✅ System Ready';
                statusDiv.className = 'status ready';
            }
        })
        .catch(error => {
            console.error('Status update failed:', error);
            const statusDiv = document.getElementById('systemStatus');
            statusDiv.textContent = '❌ Connection Error';
            statusDiv.className = 'status active';
        });
}

// Siren Controls
function activateSiren() {
    if (!confirmAction('Activate Emergency Siren?')) return;
    
    fetch('/api/siren/activate', {method: 'POST'})
        .then(response => response.json())
        .then(data => {
            if (!data.success) {
                alert('Failed to activate siren: ' + (data.message || 'Unknown error'));
            }
        })
        .catch(error => {
            console.error('Siren activation failed:', error);
            alert('Connection error. Please try again.');
        });
}

function stopSiren() {
    fetch('/api/siren/stop', {method: 'POST'})
        .then(response => response.json())
        .then(data => {
            if (!data.success) {
                alert('Failed to stop siren: ' + (data.message || 'Unknown error'));
            }
        })
        .catch(error => {
            console.error('Siren stop failed:', error);
            alert('Connection error. Please try again.');
        });
}

function activateBell() {
    fetch('/api/bell/activate', {method: 'POST'})
        .then(response => response.json())
        .then(data => {
            if (!data.success) {
                alert('Failed to activate bell: ' + (data.message || 'Unknown error'));
            }
        })
        .catch(error => {
            console.error('Bell activation failed:', error);
            alert('Connection error. Please try again.');
        });
}

// Schedule Management
function addSchedule() {
    const time = document.getElementById('scheduleTime').value;
    const type = document.getElementById('scheduleType').value;
    const repeat = document.getElementById('scheduleRepeat').value;
    
    if (!time) {
        alert('Please select a time for the schedule');
        return;
    }
    
    const schedule = {
        id: Date.now(),
        time: time,
        type: type,
        repeat: repeat,
        enabled: true,
        created: new Date().toISOString()
    };
    
    schedules.push(schedule);
    saveSchedulesToStorage();
    loadSchedules();
    
    // Clear form
    document.getElementById('scheduleTime').value = '';
    
    // Show confirmation
    showNotification('Schedule added successfully!', 'success');
}

function deleteSchedule(id) {
    if (!confirm('Are you sure you want to delete this schedule?')) return;
    
    schedules = schedules.filter(s => s.id !== id);
    saveSchedulesToStorage();
    loadSchedules();
    
    showNotification('Schedule deleted successfully!', 'success');
}

function toggleSchedule(id) {
    const schedule = schedules.find(s => s.id === id);
    if (schedule) {
        schedule.enabled = !schedule.enabled;
        saveSchedulesToStorage();
        loadSchedules();
    }
}

function loadSchedules() {
    const list = document.getElementById('schedulesList');
    
    if (schedules.length === 0) {
        list.innerHTML = '<p style="color: #666; text-align: center; padding: 20px;">No schedules configured</p>';
        return;
    }
    
    list.innerHTML = '';
    
    schedules.forEach(schedule => {
        const item = document.createElement('div');
        item.className = 'schedule-item';
        
        const enabledBadge = schedule.enabled 
            ? '<span style="color: #2ed573;">●</span>' 
            : '<span style="color: #ff4757;">●</span>';
        
        item.innerHTML = `
            <div class="schedule-info">
                <div class="schedule-time">${enabledBadge} ${formatTime(schedule.time)}</div>
                <div class="schedule-details">
                    ${schedule.type.toUpperCase()} - ${schedule.repeat}
                </div>
            </div>
            <div>
                <button onclick="toggleSchedule(${schedule.id})" class="btn-primary" style="margin-right: 10px;">
                    ${schedule.enabled ? 'Disable' : 'Enable'}
                </button>
                <button onclick="deleteSchedule(${schedule.id})" class="btn-danger">Delete</button>
            </div>
        `;
        
        list.appendChild(item);
    });
}

function saveSchedulesToStorage() {
    localStorage.setItem('smartSirenSchedules', JSON.stringify(schedules));
}

// Schedule Execution
function checkSchedules() {
    if (!currentUser || schedules.length === 0) return;
    
    const now = new Date();
    const currentTime = now.getHours().toString().padStart(2, '0') + ':' + 
                       now.getMinutes().toString().padStart(2, '0');
    
    schedules.forEach(schedule => {
        if (schedule.time === currentTime && schedule.enabled) {
            console.log('Executing scheduled action:', schedule.type);
            
            if (schedule.type === 'siren') {
                activateSiren();
            } else if (schedule.type === 'bell') {
                activateBell();
            }
            
            // Handle repeat logic
            if (schedule.repeat === 'once') {
                schedule.enabled = false;
                saveSchedulesToStorage();
                loadSchedules();
                showNotification(`One-time ${schedule.type} schedule executed and disabled`, 'info');
            } else {
                showNotification(`${schedule.type.toUpperCase()} activated by schedule`, 'info');
            }
        }
    });
}

// Utility Functions
function formatTime(time) {
    const [hours, minutes] = time.split(':');
    const hour12 = hours % 12 || 12;
    const ampm = hours >= 12 ? 'PM' : 'AM';
    return `${hour12}:${minutes} ${ampm}`;
}

function confirmAction(message) {
    return confirm(message);
}

function showNotification(message, type = 'info') {
    // Simple notification system
    const notification = document.createElement('div');
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 15px 20px;
        border-radius: 8px;
        color: white;
        font-weight: bold;
        z-index: 1000;
        animation: slideIn 0.3s ease;
        max-width: 300px;
    `;
    
    switch(type) {
        case 'success':
            notification.style.background = '#2ed573';
            break;
        case 'error':
            notification.style.background = '#ff4757';
            break;
        default:
            notification.style.background = '#667eea';
    }
    
    notification.textContent = message;
    document.body.appendChild(notification);
    
    // Auto remove after 3 seconds
    setTimeout(() => {
        notification.remove();
    }, 3000);
}

// Event Listeners
document.addEventListener('keypress', function(e) {
    if (e.key === 'Enter') {
        const activeElement = document.activeElement;
        if (activeElement.id === 'username' || activeElement.id === 'password') {
            login();
        }
    }
});

// Schedule checker - runs every minute
setInterval(checkSchedules, 60000);

// Additional safety checks
window.addEventListener('beforeunload', function() {
    stopStatusUpdates();
});

console.log('Smart Siren System JavaScript Loaded Successfully');