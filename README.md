# Water Rocket Project

This repository contains firmware and software for an ESP32-based water rocket system and its launchpad. The project includes both the rocket-side and launchpad-side code, allowing for automated pressurization, data collection, and wireless communication.
It was developed as part of a school project at INSA Lyon engineering school, in 2025.

## Project Overview

The project is based on two ESP32:

- **Launchpad Subsystem**: An ESP32, located in the lanchpad, that manages waterfilling, air pressurization, rocket locking/releasing system. Is it also both the central WiFi access point and the web server for the UI.
- **Rocket Subsystem**: An ESP32, located in the rocket, that monitors flight parameters, controls parachute deployment, and stores flight data.

## Technical Features

### Launchpad
- ESP32 runs as a WiFi access point. Both the rocket and the user laptop/phone are connected to it.
- Serves a responsive web UI (HTML/CSS/JS with TailwindCSS) available within the local network, so the user can access it.
- The UI allows the user to control the launch sequence and visualize data after the flight.
- Real-time monitoring via WebSocket for water level, pressure, and system status
- API endpoints for precise control of water filling, pressurization, and launch sequence
- Flow meter integration for accurate water volume measurement
- HTTP and Websocket backend is written in C++

### Rocket
- In-flight data logging (altitude, acceleration, etc.)
- Automatic parachute deployment
- Post-flight reconnection to launchpad WiFi access point for data transfer
- Power-efficient operation via LiPo battery

## Repository Structure

```
/
├── launchpad/       # Launchpad ESP32 firmware
│   ├── src/         # C++ source files
│   └── data/        # Frontend web UI files (HTML/CSS/JS)
├── rocket/          # Rocket ESP32 firmware
│   └── src/         # C++ source files
└── docs/            # Technical documentation
```

## License

This project is for educational purposes. See LICENSE file for details.
