// Update the current time
function updateTime() {
    const now = new Date();
    document.getElementById('current-time').textContent = now.toLocaleTimeString();
    document.getElementById('current-date').textContent = now.toLocaleDateString('en-US', { 
        weekday: 'long', 
        year: 'numeric', 
        month: 'long', 
        day: 'numeric' 
    });
}

// Update time every second
setInterval(updateTime, 1000);
updateTime(); // Initial call

// Modal functionality
const modal = document.getElementById('schedule-modal');
const addScheduleBtn = document.getElementById('add-schedule');
const closeModalBtn = document.querySelector('.close-modal');
const cancelBtn = document.querySelector('.btn-cancel');

addScheduleBtn.addEventListener('click', () => {
    modal.classList.add('active');
});

function closeModal() {
    modal.classList.remove('active');
}

closeModalBtn.addEventListener('click', closeModal);
cancelBtn.addEventListener('click', closeModal);

// Close modal when clicking outside
window.addEventListener('click', (e) => {
    if (e.target === modal) {
        closeModal();
    }
});

// Quick action buttons
document.getElementById('ring-now').addEventListener('click', () => {
    alert('Ringing school siren now!');
    // Here you would add code to send a command to the ESP32 to ring the siren
});

document.getElementById('ring-bell').addEventListener('click', () => {
    alert('Ringing dining hall bell now!');
    // Here you would add code to send a command to the ESP32 to ring the bell
});

document.getElementById('emergency-alarm').addEventListener('click', () => {
    if (confirm('Are you sure you want to trigger the EMERGENCY alarm?')) {
        alert('EMERGENCY ALARM ACTIVATED!');
        // Here you would add code to send a command to the ESP32 to ring the emergency alarm
    }
});

// Form submission (for demo purposes)
document.getElementById('schedule-form').addEventListener('submit', (e) => {
    e.preventDefault();
    alert('Schedule saved! In a real implementation, this would save to a database and update the ESP32.');
    closeModal();
});

// Edit buttons (for demo purposes)
document.querySelectorAll('.btn-edit').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelector('.modal-title').textContent = 'Edit Schedule';
        modal.classList.add('active');
        // In a real implementation, this would populate the form with existing data
    });
});

// Delete buttons (for demo purposes)
document.querySelectorAll('.btn-delete').forEach(btn => {
    btn.addEventListener('click', () => {
        if(confirm('Are you sure you want to delete this schedule?')) {
            alert('Schedule deleted! In a real implementation, this would remove from database.');
            // Here you would delete the schedule from the database
        }
    });
});
