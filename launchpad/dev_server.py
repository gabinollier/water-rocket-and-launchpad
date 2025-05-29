#!/usr/bin/env python3
"""
Development server for ESP32 water rocket launchpad frontend
Mimics the ESP32 server's static file serving behavior for quick frontend testing
"""

import http.server
import socketserver
import os
import urllib.parse
import json
import random
import time
from pathlib import Path

class ESP32DevHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        # Set the directory to serve files from
        self.frontend_dir = Path(__file__).parent / "frontend" / "src"
        super().__init__(*args, directory=str(self.frontend_dir), **kwargs)
    
    def do_GET(self):
        # Parse the URL path
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path
        query_params = urllib.parse.parse_qs(parsed_path.query)
        
        # API endpoints
        if path == "/api/get-all-flight-timestamps":
            self.handle_get_all_flight_timestamps()
        elif path == "/api/get-flight-data":
            self.handle_get_flight_data(query_params)
        # Routes that should serve index.html (mimicking ESP32 behavior)
        elif path in ["", "/", "/launch", "/flight-data", "/flight-data-list", "/debug"]:
            # Serve index.html for these routes
            self.serve_index()
        elif path.startswith(("/pages/", "/css/", "/js/")):
            # Serve static files from these directories
            super().do_GET()
        elif path == "/favicon.ico":
            # Serve favicon
            super().do_GET()
        else:
            # For any other path, serve index.html (mimicking the 404 handler)
            print(f"404: Client tried to access {path} but it doesn't exist. Serving index.html instead.")
            self.serve_index()
            
    def serve_index(self):
        """Serve the index.html file"""
        try:
            index_path = self.frontend_dir / "index.html"
            with open(index_path, 'rb') as f:
                content = f.read()
            
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=UTF-8')
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            self.wfile.write(content)
            
        except FileNotFoundError:
            self.send_error(404, "index.html not found")
    
    def send_json_response(self, data):
        """Send a JSON response"""
        try:
            json_data = json.dumps(data).encode('utf-8')
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(json_data)))
            self.send_header('Access-Control-Allow-Origin', '*')  # Allow CORS for development
            self.end_headers()
            self.wfile.write(json_data)
            
        except Exception as e:
            print(f"Error sending JSON response: {e}")
            self.send_error(500, f"Error generating response: {e}")
    
    def handle_get_all_flight_timestamps(self):
        """Handle /api/get-all-flight-timestamps endpoint with dummy data"""
        # Generate some fake flight timestamps (last 5 flights)
        current_time = int(time.time() * 1000)  # Current time in milliseconds
        fake_timestamps = []
        
        for i in range(5):
            # Generate timestamps for flights in the past few days
            days_ago = i * 2 + 1  # 1, 3, 5, 7, 9 days ago
            timestamp = current_time - (days_ago * 24 * 60 * 60 * 1000)
            fake_timestamps.append(timestamp)
        
        # Sort timestamps in descending order (newest first)
        fake_timestamps.sort(reverse=True)
        
        response_data = fake_timestamps
        
        self.send_json_response(response_data)
        print(f"API: Served {len(fake_timestamps)} flight timestamps")
    
    def handle_get_flight_data(self, query_params):
        """Handle /api/get-flight-data endpoint with dummy data"""
        if 'timestamp' not in query_params:
            self.send_error(400, "Missing timestamp parameter")
            return
        
        timestamp = query_params['timestamp'][0]
        
        # Generate fake flight data
        flight_data = []
        duration_ms = 15000  # 15 second flight
        start_time = int(timestamp)
        
        for i in range(0, duration_ms, 50):  # Data point every 50ms
            time_offset = i
            
            # Simulate flight phases
            if i < 2000:  # Launch phase (0-2 seconds)
                altitude = (i / 2000.0) * 150  # Rapid ascent to 150m
                velocity = 75 - (i / 2000.0) * 30  # Start at 75 m/s, decrease
                acceleration = 20 - (i / 100.0)  # High initial acceleration, decreasing
            elif i < 8000:  # Ascent phase (2-8 seconds)
                progress = (i - 2000) / 6000.0
                altitude = 150 + progress * 250  # Continue to 400m peak
                velocity = 45 - progress * 60  # Decelerate to -15 m/s
                acceleration = -9.81 - progress * 5  # Gravity plus drag
            elif i < 12000:  # Descent phase (8-12 seconds)
                progress = (i - 8000) / 4000.0
                altitude = 400 - progress * 300  # Fall to 100m (parachute deploy)
                velocity = -15 - progress * 10  # Accelerate downward to -25 m/s
                acceleration = -9.81
            else:  # Parachute phase (12-15 seconds)
                progress = (i - 12000) / 3000.0
                altitude = 100 - progress * 100  # Gentle descent to ground
                velocity = -25 + progress * 20  # Slow to -5 m/s
                acceleration = 5  # Upward acceleration from parachute
            
            # Add some realistic noise
            altitude += random.uniform(-2, 2)
            velocity += random.uniform(-1, 1)
            acceleration += random.uniform(-0.5, 0.5)
            
            # Simulate sensor data
            data_point = {
                "timestamp": start_time + time_offset,
                "altitude": round(altitude, 2),
                "velocity": round(velocity, 2),
                "acceleration": round(acceleration, 2),
                "pressure": round(1013.25 - (altitude * 0.12), 2),  # Atmospheric pressure
                "temperature": round(15 - (altitude * 0.0065), 1),  # Temperature lapse rate
                "battery_voltage": round(3.7 - (i / duration_ms) * 0.3, 2)  # Battery drain
            }
            
            flight_data.append(data_point)
        
        self.send_json_response(flight_data)
        print(f"API: Served flight data for timestamp {timestamp} ({len(flight_data)} data points)")
    
    def send_json_response(self, data):
        """Send a JSON response"""
        try:
            json_data = json.dumps(data).encode('utf-8')
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(json_data)))
            self.send_header('Access-Control-Allow-Origin', '*')  # Allow CORS for development
            self.end_headers()
            self.wfile.write(json_data)
            
        except Exception as e:
            print(f"Error sending JSON response: {e}")
            self.send_error(500, f"Error generating response: {e}")
    
    def log_message(self, format, *args):
        """Override to provide cleaner logging"""
        print(f"[{self.address_string()}] {format % args}")

def run_server(port=8080):
    """Run the development server"""
    print(f"🚀 Water Rocket Dev Server")
    print(f"Starting development server on port {port}")
    print(f"Serving files from: {Path(__file__).parent / 'frontend' / 'src'}")
    print(f"Open your browser to: http://localhost:{port}")
    print(f"Routes serving index.html: /, /launch, /flight-data, /flight-data-list, /debug")
    print(f"Static file routes: /pages/, /css/, /js/")
    print(f"API endpoints available:")
    print(f"  - GET /api/get-all-flight-timestamps (dummy data)")
    print(f"  - GET /api/get-flight-data?timestamp=<timestamp> (dummy data)")
    print("-" * 50)
    
    try:
        with socketserver.TCPServer(("", port), ESP32DevHandler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n🛑 Server stopped by user")
    except OSError as e:
        if e.errno == 48:  # Address already in use
            print(f"❌ Port {port} is already in use. Try a different port:")
            print(f"   python dev_server.py --port 8081")
        else:
            print(f"❌ Error starting server: {e}")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="ESP32 Water Rocket Development Server")
    parser.add_argument("--port", "-p", type=int, default=8080, 
                       help="Port to run the server on (default: 8080)")
    
    args = parser.parse_args()
    run_server(args.port)
