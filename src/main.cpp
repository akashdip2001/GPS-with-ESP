#include <WiFi.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WebServer.h>

// ==== Station (STA) mode : Replace with your WiFi credentials ====
const char* ssid = "spa";
const char* password = "12345678";

// ==== Web server on port 80 ====
WebServer server(80);

// ==== GPS Setup ====
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Use UART2
const int RXD2 = 16;
const int TXD2 = 17;

// ==== HTML Page Template ====
String htmlPage() {
  return R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32 GPS Tracker</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
    <style>
      body { margin:0; font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
      h2 { color: #2c3e50; margin: 10px 0; }
      #map { height: 80vh; width: 100%; margin: 10px 0; border: 2px solid #2c3e50; border-radius: 10px; }
      .info { font-size: 18px; padding: 10px; }
      .custom-pin {
        background-color: red;
        border-radius: 50%;
        width: 14px;
        height: 14px;
        display: block;
        border: 2px solid white;
      }
    </style>
  </head>
  <body>
    <h2>ESP32 GPS Tracker</h2>
    <div id="map"></div>
    <div class="info" id="info">
      Loading GPS data...
    </div>

    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script>
      let map = L.map('map').setView([0, 0], 2);
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap contributors'
      }).addTo(map);

      const icon = L.divIcon({ className: 'custom-pin' });
      const marker = L.marker([0, 0], { icon: icon }).addTo(map);

      async function updateLocation() {
        try {
          const res = await fetch('/gps');
          const data = await res.json();

          const lat = data.lat;
          const lng = data.lng;

          marker.setLatLng([lat, lng]);

          document.getElementById('info').innerHTML = `
            Latitude: ${lat}<br>
            Longitude: ${lng}<br>
            Satellites: ${data.sat}<br>
            Speed: ${data.spd} km/h
          `;
        } catch (err) {
          console.error("Failed to update location", err);
        }
      }

      // First load
      updateLocation();
      // Repeat every 5 seconds
      setInterval(updateLocation, 5000);
    </script>
  </body>
  </html>
  )rawliteral";
}


void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // ==== Connect to your router ====
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // ==== Start Web Server ====
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");

  // ==== a New Endpoint for GPS Data ====
  server.on("/gps", []() {
  String json = "{";
  if (gps.location.isValid()) {
    json += "\"lat\":" + String(gps.location.lat(), 6) + ",";
    json += "\"lng\":" + String(gps.location.lng(), 6) + ",";
    json += "\"sat\":" + String(gps.satellites.value()) + ",";
    json += "\"spd\":" + String(gps.speed.kmph());
  } else {
    json += "\"lat\":0,\"lng\":0,\"sat\":0,\"spd\":0";
  }
  json += "}";
  server.send(200, "application/json", json);
});

}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  server.handleClient();
}
