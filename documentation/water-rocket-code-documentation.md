# Water Rocket Project: Networking and Code Documentation

This document outlines the networking architecture and software implementation for the water rocket launchpad. It details how the launchpad and rocket communicate, how the web UI is hosted and served, and which parts of the system are handled in C++ for critical safety and control.

---

## 1. Project Overview

The water rocket system consists of two primary subsystems:

- **Launchpad Subsystem**: Provides the user interface (hosted on an ESP32) and manages water/air filling, pressurization, and launch operations.
- **Rocket Subsystem**: Executes the flight control, logs in-flight data to an SD card, and reconnects post-flight to transfer data.

This complementary documentation focuses on:

- The networking design and communication protocols.
- The code architecture for the ESP32 firmware on the launchpad.
- Integration of HTML/CSS/JS (with TailwindCSS) for the UI.
- RESTful endpoints (using POST for control actions and GET for status queries).
- Real-time updates via WebSocket.
- Safety-critical code implemented in C++.

---

## 2. Networking Architecture

### 2.1 Launchpad ESP32 as a WiFi Access Point

- **AP Mode Configuration**:  
  The launchpad ESP32 initializes as an Access Point (AP) with a static IP (typically `192.168.4.1`) and a configurable SSID (e.g., "RocketLaunchpad"). This allows user devices to directly connect to the launchpad.
  
- **DHCP and Routing**:  
  The ESP32 acts as a DHCP server, assigning IP addresses (e.g., `192.168.4.2` for the rocket) and routing requests to the onboard web server.

### 2.2 Rocket ESP32 as a WiFi Client

- **Reconnection Protocol**:  
  After landing, the rocket’s ESP32 automatically reconnects to the launchpad network. A connection check (in C++) ensures that the rocket is online before transferring flight data.
  
- **Data Transfer**:  
  Once reconnected, the rocket transmits its SD card data (stored in CSV format) to the launchpad. This data is then made available in the "Flights Data" tab on the web interface.

---

## 3. Software Architecture and Code Structure

### 3.1 Launchpad Firmware (ESP32)

#### 3.1.1 Web Server & REST API

- **Serving the UI**:  
  The launchpad hosts an HTML/CSS/JavaScript UI built with TailwindCSS. The static files are stored in flash and served over HTTP.

- **Endpoints**:
  - **GET `/status`**: Returns current system status (e.g., water level, pressure, fill status).
  - **POST `/water-control`**: Initiates or stops the water filling process. The payload includes the target water volume.
  - **POST `/pressure-control`**: Manages air pressurization. The payload can specify target pressure.
  - **POST `/launch`**: Starts the launch sequence after verifying safety conditions.
  - **GET `/data`**: Retrieves flight data (CSV format) recorded on the rocket’s SD card.

*Example:*

```cpp
// REST endpoint for water control
server.on("/water-control", HTTP_POST, [](AsyncWebServerRequest *request) {
  // Read JSON payload for target water volume
  // Validate and initiate water fill process
  request->send(200, "application/json", "{\"status\":\"filling started\"}");
});
```

#### 3.1.2 WebSocket for Real-Time Updates

- **Continuous Updates**:  
  A WebSocket connection (`/ws`) provides live updates on water level, pressure, and fill progress. This allows the UI to refresh gauges and indicators without page reloads.
  
- **Implementation**:

  ```cpp
  AsyncWebSocket ws("/ws");
  server.addHandler(&ws);

  void updateStatus() {
    DynamicJsonDocument doc(64);
    doc["waterLevel"] = currentWaterLevel;
    doc["pressure"] = currentPressure;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
  }
  ```
  
- **Critical C++ Safety Checks**:  
  Prior to starting any filling process, C++ code verifies:
  - The rocket lock mechanism is engaged.
  - The rocket ESP32 is connected and ready.
  This safety logic is embedded in the firmware to prevent accidental launches or unsafe pressurization.

### 3.2 Rocket Firmware (ESP32)

- **Flight Data Logging**:  
  During flight, the rocket logs sensor data (e.g., acceleration, altitude) to an onboard SD card in CSV format.
  
- **Post-Flight Data Transfer**:  
  After landing, the rocket reconnects to the launchpad network and transfers the logged data using the `/data` endpoint.
  
- **Safety Mechanisms**:  
  Critical checks (e.g., verifying locking mechanism and network connectivity) are implemented in C++ to ensure all conditions are met before launch.

---

## 4. Web UI Implementation

### 4.1 Front-End Technology Stack

- **HTML/CSS/JS**:  
  The user interface is built using standard web technologies.
  
- **TailwindCSS**:  
  TailwindCSS is used to create a responsive and modern UI design. It streamlines styling and ensures consistency across different devices.

### 4.2 UI Features

- **Control Panel**:  
  - **Input Fields**: Users can set the precise water volume and pressure.
  - **Buttons**:  
    - **Start Filling**: Sends a POST request to `/water-control` with the user-specified parameters.
    - **Abort**: Immediately disengages the condenser and releases pressure in an emergency by sending a POST command.
    - **Launch**: Triggers the launch sequence via a POST request to `/launch`.

- **Real-Time Monitoring**:  
  - **WebSocket Client**:  
    A JavaScript client connects to the `/ws` endpoint to update gauges and numerical displays in real time.
  
- **Flight Data Visualization**:  
  - **Flights Data Tab**:  
    After each flight, the UI shows a new entry with the flight date and time. Users can view the CSV data directly in a table or download it for analysis.

*Example JavaScript code snippet:*

```js
// Establishing WebSocket connection for real-time updates
const socket = new WebSocket(`ws://${window.location.hostname}/ws`);

socket.onmessage = event => {
  const data = JSON.parse(event.data);
  updateGauges(data.waterLevel, data.pressure);
};

// Sending a POST request to start the filling process
function startFilling(targetVolume) {
  fetch('/water-control', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({volume: targetVolume})
  }).then(response => response.json())
    .then(data => console.log(data));
}
```

---

## 5. Critical Safety and Control Code

The most safety-critical operations are implemented in C++. These include:

- **Pre-Filling Validation**:  
  Before starting the water filling process, the code checks that the lock mechanism is engaged.
  
- **Connectivity Check**:  
  The firmware continuously monitors the connectivity status of the rocket ESP32. If not connected, the filling or launch sequence is halted.
  
- **Emergency Handling**:  
  The "Abort" command immediately triggers a routine that:
  - Disengages the condenser.
  - Connects the rocket to the atmosphere to release excess pressure.
  
*Sample C++ function for safety check:*

```cpp
bool canStartFilling() {
  // Check if the locking mechanism is engaged
  if (!isLockEngaged()) {
    Serial.println("Error: Lock mechanism not engaged!");
    return false;
  }
  // Check connectivity with the rocket ESP32
  if (!isRocketConnected()) {
    Serial.println("Error: Rocket not connected!");
    return false;
  }
  return true;
}
```

---

## 6. Flight Data Management

### 6.1 Logging and Storage

- **In-Flight Data Logging**:  
  The rocket logs telemetry (e.g., acceleration, altitude, temperature) to an SD card in CSV format, ensuring detailed data capture.
  
- **Post-Flight Data Transfer**:  
  After landing and reconnecting to the launchpad network, the rocket sends its flight log to the launchpad via the `/data` endpoint. This data is then displayed under the "Flights Data" tab on the UI, complete with timestamps.

### 6.2 User Interaction

- **Viewing Flight Data**:  
  The UI provides an interface for users to view the logged data in a table format.
- **Downloading CSV Files**:  
  Users can also download the raw CSV files for further analysis.

---

## 7. Conclusion and Future Expansion

This networking and code documentation details the inner workings of the water rocket launchpad’s communication and control systems. By combining robust C++ safety checks with a modern web interface built on HTML/CSS/JS/TailwindCSS, the system ensures:

- Real-time monitoring via WebSocket.
- Safe and controlled operations via REST endpoints.
- Seamless post-flight data transfer and visualization.

**Future enhancements** may include:

- Enhanced security protocols for the web server.
- More granular telemetry on the UI.
- Additional safety routines and fallback mechanisms.

This integrated approach ensures that both the hardware and software systems work in harmony for a safe and engaging water rocket launch experience.

---

This documentation should serve as a technical reference for developers and engineers looking to understand or extend the networking and code aspects of the water rocket project.
