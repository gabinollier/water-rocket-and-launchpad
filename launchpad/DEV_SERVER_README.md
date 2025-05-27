# Water Rocket Development Server

A simple Python development server that mimics your ESP32 server's behavior for quick frontend testing.

## Features

- Serves `index.html` for routes: `/`, `/launch`, `/flight-data`, `/debug`
- Serves static files from `/pages/`, `/css/`, `/js/` directories
- Mimics the ESP32's 404 behavior (serves `index.html` for unknown routes)
- Clean logging and error handling

## Usage

### Option 1: Python directly
```bash
python dev_server.py
```

### Option 2: With custom port
```bash
python dev_server.py --port 8081
```

### Option 3: Windows batch file
```bash
start_dev_server.bat
```

## Access

Once running, open your browser to:
- http://localhost:8080 (or your chosen port)
- The server will automatically route `/launch`, `/flight-data`, and `/debug` to serve your SPA

## Notes

- The server serves files from `frontend/src/` directory
- API endpoints are NOT implemented (they'll return 404 or serve index.html)
- Perfect for testing your frontend routing and UI without uploading to ESP32
- Use Ctrl+C to stop the server

## Testing Your Routes

- `http://localhost:8080/` → serves index.html
- `http://localhost:8080/launch` → serves index.html (your router.js handles the routing)
- `http://localhost:8080/flight-data` → serves index.html
- `http://localhost:8080/debug` → serves index.html
- `http://localhost:8080/css/styles.css` → serves the actual CSS file
- `http://localhost:8080/js/framework/router.js` → serves the actual JS file
- `http://localhost:8080/pages/launch.html` → serves the actual HTML file
