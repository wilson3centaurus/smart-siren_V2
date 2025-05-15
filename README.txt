Rusununguko ZIMFEP High Smart Siren System

Components Required:
-------------------
1. ESP32 Development Board
2. Piezo Buzzer (for prototype testing)
3. Relay Module (5V) - 2 channels (for connecting to actual siren and bell)
4. Power Supply (5V for ESP32)
5. Jumper Wires
6. Breadboard (for prototype)
7. LED (optional, for status indication)
8. Resistor (220-330 ohm for LED)

Connections:
-----------
ESP32 Pin 23 ────────────┬─── Buzzer/Siren (for prototype)
                         └─── Relay Channel 1 Input (for actual implementation)
                                      │
                                      └─── [To School Siren]

ESP32 Pin 22 ────────────┬─── Bell Sound (for prototype) 
                         └─── Relay Channel 2 Input (for actual implementation)
                                      │
                                      └─── [To Dining Hall Bell]

ESP32 Pin 2 (Built-in LED) ─── Status LED (already on board)

ESP32 GND ──────────────────── Common Ground
                        │
                        ├───── Buzzer/Siren GND
                        │
                        ├───── Bell GND
                        │
                        └───── Relay Module GND

ESP32 5V/3.3V ─────────────── Relay Module VCC

Implementation Notes:
-------------------
1. For prototype: Using a piezo buzzer directly connected to pins 22 and 23
2. For final implementation: Using 5V relay modules to control:
   - Main school siren (typically high voltage)
   - Dining hall bell system

3. ESP32 connects to school WiFi to:
   - Synchronize time with NTP server
   - Host web interface for administrators
   - Allow remote siren/bell activation

4. The system can be expanded by adding:
   - Additional relay channels for more siren locations
   - Amplifiers for louder sound in prototype
   - Battery backup system for power outages