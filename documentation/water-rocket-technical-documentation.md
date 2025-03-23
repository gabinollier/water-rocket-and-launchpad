# Water Rocket Project: Technical Documentation

## Project Overview

This project involves the development of a water rocket system with onboard data collection capabilities and a remote control launchpad. The system consists of two main subsystems:

1. **Rocket Subsystem**: An ESP32-based flight control system that monitors flight parameters, controls parachute deployment, and stores flight data. The rocket body is constructed from soda bottles.
2. **Launchpad Subsystem**: An ESP32-based control system that manages water/air pressurization, interfaces with the user, and provides the WiFi network infrastructure. Features a co-axial tube configuration. A PVC tube forms the outer channel (carrying water) while a smaller inner tube (carrying air) runs coaxially inside. These two channels are kept separate by a custom connection (details of which are handled separately). The rocket slides in the PVC tube when inserted on the launchpad.

## System Architecture

### Hardware Components

#### Rocket Subsystem

- **Microcontroller**: ESP32 (with integrated WiFi)
- **Sensors**:
  - Altimeter/barometric pressure sensor (e.g., BMP280/BMP388)
  - Accelerometer (e.g., MPU6050)
- **Actuators**:
  - Servo motor for parachute deployment
- **Storage**:
  - SD card module for flight data logging
- **Power**:
  - LiPo battery with appropriate voltage regulation
- **Rocket Body & Filling Interface**:
  - Constructed from soda bottles. The bottom bottle's neck slides around the launchpad outer tube.

#### Launchpad Subsystem

- **Microcontroller**: ESP32 (with integrated WiFi)
- **Actuators**:
  - Solenoid valves for water filling
  - Solenoid valves for air pressurization (via the pressure distributor)
  - Servo motor for the rocket locking/release mechanism
- **Sensors**:
  - Flow meter for precise water volume measurement
  - Pressure sensors for monitoring tank pressure
- **Power**:
  - Higher capacity power supply (12V for solenoids with appropriate regulation)
- **Filling System Hardware**:
  - **Water Side**:
    - **Pipeline Sequence**: Water tank → pump → solenoid valve → flow meter → custom connection → rocket (outer tube).
  - **Air Side**:
    - **Pipeline Sequence**: Compressor → pressure distributor → custom connection → rocket (inner tube).
  - The pressure distributor allows the air channel to be connected either to the atmosphere, to the compressor, or to remain isolated so that the internal pressure is maintained.
- **Optional Indicators**:
  - Status LEDs for system state indication

### Network Architecture

The network architecture remains centralized around the launchpad ESP32:

1. **Launchpad ESP32**:
   - Operates in WiFi Access Point (AP) mode with a configurable SSID (default "RocketLaunchpad").
   - Uses a static IP (default 192.168.4.1) and handles DHCP for connected devices.
   - Hosts a web server for the control interface.
2. **User Device**:
   - Connects to the "RocketLaunchpad" WiFi network.
   - Accesses the control interface at 192.168.4.1.
3. **Rocket ESP32**:
   - Configured to connect to the "RocketLaunchpad" network upon landing.
   - Receives an IP via DHCP (e.g., 192.168.4.2) and continuously attempts reconnection until successful.

## Software Architecture

### Rocket ESP32 Software

#### Flight State Machine

1. **Pre-launch State**:
   - Initialize sensors and begin low-frequency data logging.
   - Wait for the launch acceleration threshold.
2. **Launch Detected State**:
   - Triggered by high vertical acceleration (>2g for >0.5s).
   - Increase data logging frequency and begin integrating acceleration data.
3. **Ascending State**:
   - Monitor altitude and acceleration.
   - Log flight data to the SD card at a high frequency (50-100Hz).
4. **Apogee Detection**:
   - Determine apogee using a combination of zero/negative vertical velocity and minimal altitude change.
5. **Descent State**:
   - Monitor free-fall conditions.
   - Deploy parachute when free-fall conditions are met.
6. **Landing State**:
   - Detect landing via impact or near-zero altitude change.
   - Reduce data logging frequency and start WiFi reconnection attempts.
7. **Data Transfer State**:
   - After establishing WiFi, initiate the web server for data transmission.
   - Transfer flight data to the launchpad/user device.

#### Data Logging Subsystem

- Timestamp each data point (milliseconds since boot).
- Log sensor readings to an SD card in CSV format with headers (Time, Altitude, Vertical Acceleration, 3-axis Acceleration, Temperature, System Events).

#### Parachute Deployment Algorithm

- Monitor vertical acceleration and compute a moving average.
- Deploy the parachute when:
  - Vertical acceleration is less than –9.5 m/s² (adjustable threshold) for more than 100ms.
  - Alternatively, deploy using a backup timer after apogee.

#### WiFi Client Implementation

- Store launchpad network credentials.
- Attempt reconnection after landing with exponential backoff.
- Timeout after 5 minutes if no connection is established, then enter sleep mode.

### Launchpad ESP32 Software

#### WiFi Access Point

- Initializes as an AP with a fixed SSID and password.
- Manages connected devices and supports power-saving features.

#### Web Server Implementation

- Serves the HTML/CSS/JavaScript control interface.
- Provides direct URL endpoints for control functions.
- Uses WebSocket for real-time status updates (flow rate, volume, pressure).

#### Water Flow Control

- Monitors the flow meter pulses to compute water volume.
- Uses the formula:
  - **Flow rate (L/min)**: Pulse frequency / K-factor.
  - **Volume (mL)**: (Pulse count / K-factor) × 1000.
- Automatically closes the water valve when the target volume is reached.

#### Filling Process Control and State Machine

The filling process integrates both water and air management and is carried out as follows:

0. **Pre-Filling – Safety Lock**:  
   - Confirm that the rocket locking system is engaged via a servo motor to prevent premature lift-off.
  
1. **Atmospheric Air Connection**:  
   - Via the distributor, connect the rocket’s inner air tube to the atmosphere. This allows water to flow into the outer tube.
  
2. **Water Filling**:  
   - Activate the pump and monitor the flow meter to fill the rocket with a precise volume of water through the water pipeline (water tank → pump → valve → flow meter → custom connection → rocket).
  
3. **Transition to Pressurization**:  
   - Once the desired water volume is achieved, close the water valve.
   - Reconfigure the distributor to connect the rocket’s air channel to the compressor, thereby pressurizing the rocket while preventing water from escaping.
  
4. **Locking the Pressure**:  
   - After pressurization, the distributor disconnects the rocket’s air chamber from the compressor without venting it to the atmosphere; it simply locks the chamber to maintain pressure.
  
5. **Launch Command**:  
   - Upon user command, the servo controlling the rocket locking mechanism rotates to release the rocket. The rocket lifts off.
  
6. **Post-Launch Safety**:  
   - A few seconds after launch, the servo re-locks the mechanism (its default state ensures the rocket can be reinserted safely).
   - The distributor remains in the all-blocked position to prevent any unintended air release.

#### Data Management

- Logs launch parameters including water volume, pressure, launch time, and environmental conditions.
- Receives flight data from the rocket and provides options for downloading via the web interface.

#### Safety Systems

- Implements pressure threshold limiters.
- Uses valve timeout safeguards and system state verification.
- Provides an emergency pressure release capability.

## Communication Implementation

### Direct Web Interface

- **URL Endpoints**:
  - `/status`: Returns system status.
  - `/water-control`: Controls the water valve and reports flow meter readings.
  - `/pressure-control`: Controls the air valve and reports pressure readings.
  - `/launch`: Executes the launch sequence.
  - `/data`: Retrieves flight data.

### WebSocket for Real-time Updates

- Establishes a WebSocket connection to send periodic updates on flow rate, water volume, and pressure.
- Updates client-side gauges and displays accordingly.

### Code examples

#### C++

``` cpp
// Set up the WebSocket server
AsyncWebSocket ws("/ws");
server.addHandler(&ws);

// In your main loop or water level monitoring function
void updateWaterLevel() {
  static float lastReportedLevel = -1;
  float currentLevel = readWaterLevelSensor();
  
  // Send an update when the level changes or every second
  if (currentLevel != lastReportedLevel || millis() - lastUpdateTime > 1000) {
    lastReportedLevel = currentLevel;
    lastUpdateTime = millis();
    
    DynamicJsonDocument doc(64);
    doc["level"] = currentLevel;
    doc["max_level"] = MAX_TANK_CAPACITY;
    String json;
    serializeJson(doc, json);
    
    ws.textAll(json);
  }
}
```

#### JS

``` js
const socket = new WebSocket('ws://' + window.location.hostname + '/ws');

socket.onmessage = function(event) {
  const data = JSON.parse(event.data);
  
  // Update your gauge visualization
  updateGauge(data.level, data.max_level);
};

// Function to control filling
function startFilling(targetAmount) {
  fetch('/api/tank/fill', { 
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({ amount: targetAmount })
  });
}
```

## Flow Meter Integration

### Hardware Connection

- Connect the flow meter signal pin to an interrupt-capable GPIO.
- Power the flow meter at the appropriate voltage (typically 5V) and include a pull-up resistor if needed.

### Software Implementation

- **Interrupt-Based Pulse Counting**:

  ```cpp
  const byte FLOW_SENSOR_PIN = 27;
  volatile int flowPulseCount = 0;
  
  void IRAM_ATTR flowPulseCounter() {
    flowPulseCount++;
  }
  
  void setup() {
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, RISING);
  }
  ```

- **Flow Rate and Volume Calculation**:

  ```cpp
  const float FLOW_CALIBRATION_FACTOR = 7.5; // Pulses per liter (calibrate as needed)
  float flowRate = 0.0;
  float totalVolume = 0.0;
  unsigned long oldTime = 0;
  
  void calculateFlow() {
    unsigned long currentTime = millis();
    if (currentTime - oldTime > 1000) { // Update every second
      detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
      
      flowRate = (flowPulseCount / FLOW_CALIBRATION_FACTOR) * 60; // L/min
      float volume = (flowPulseCount / FLOW_CALIBRATION_FACTOR) * 1000; // mL
      totalVolume += volume;
      
      flowPulseCount = 0;
      oldTime = currentTime;
      
      attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, RISING);
    }
  }
  ```

- **Automatic Valve Control**:

  ```cpp
  void controlWaterFill(float targetVolumeMl) {
    if (totalVolume < targetVolumeMl) {
      digitalWrite(WATER_VALVE_PIN, HIGH); // Open valve
    } else {
      digitalWrite(WATER_VALVE_PIN, LOW);  // Close valve
      waterFillComplete = true;
    }
  }
  ```

## Operational Workflow

1. **System Setup**:
   - Power on the launchpad ESP32.
   - Connect a laptop or user device to the launchpad WiFi network.
   - Access the control interface via a web browser (default: 192.168.4.1).

2. **Pre-Launch & Filling Process**:
   - **Rocket Insertion & Locking**:  
     Insert the rocket (Coke bottle with co-axial PVC/inner tube system) into the launch mechanism. Ensure the rocket locking system is engaged via a servo motor.
   - **Air Connection for Water Filling**:  
     Using the pressure distributor, connect the rocket’s air channel to the atmosphere. This ensures that as water enters the outer PVC tube, air can escape.
   - **Water Filling**:  
     Activate the pump and monitor the flow meter while water is pumped from the water tank through the pipeline (tank → pump → solenoid valve → flow meter → custom connection) into the rocket. Fill until the target volume is reached.
   - **Pressurization**:  
     Close the water valve. Reconfigure the pressure distributor to connect the rocket’s air channel to the compressor. The compressor pressurizes the rocket, ensuring the water remains inside.
   - **Locking Pressure**:  
     Disconnect the rocket’s air channel from the compressor (without venting to the atmosphere) by locking the distributor, maintaining the internal pressure.

3. **Launch Sequence**:
   - When the user initiates the launch, the servo controlling the rocket locking mechanism rotates to release the rocket.
   - The rocket takes off, and shortly afterward, the servo returns to the locked position to secure the mechanism for the next launch.
   - The pressure distributor remains in its locked (all-blocked) position to retain the pressurized state.

4. **Flight Phase & Data Retrieval**:
   - The rocket ESP32 detects launch, records flight data, and handles parachute deployment as required.
   - After landing, the rocket reconnects to the launchpad WiFi, and flight data is transferred for visualization and download via the web interface.

## Implementation Considerations

### Power Management

- **Rocket ESP32**:
  - Operates on a 3.7V LiPo battery (≥500mAh) with deep sleep modes and battery status indication.
- **Launchpad ESP32**:
  - Utilizes a stable 12V supply for solenoids and a separate 5V regulated supply for the ESP32, servo, and flow meter. Power filtering is recommended to mitigate noise from solenoid operation.

### Environmental Factors

- Waterproof rocket electronics.
- Compensation for barometric and temperature changes.
- Shock resistance for the accelerometer.

### Flow Meter Calibration & Safety

- Calibrate the flow meter to determine the precise pulse/liter ratio.
- Regularly verify calibration and account for variations in water pressure.
- Integrate pressure threshold limiters, valve timeouts, and redundant sensor checks.

### Failure Modes and Recovery

- SD card, WiFi, sensor, and battery failures should trigger safe shutdown or recovery procedures.
- The distributor and valve control are designed with safety in mind to prevent unintended pressure loss.

### Testing Procedures

- Perform component-level and integration testing.
- Verify flow meter calibration and safety protocols through simulated and low-altitude tests.
- Execute full system tests including recovery verification.

## Future Expansion Possibilities

- Integration of a GPS module for location tracking.
- Camera integration for flight recording.
- Bluetooth backup communication.
- Simultaneous tracking for multiple rockets.
- Extended telemetry and ground station environmental monitoring.
