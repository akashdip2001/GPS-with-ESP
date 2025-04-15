#include <WiFi.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WebServer.h>

// ==== Replace with your WiFi credentials ====
const char* ssid = "spa";
const char* password = "12345678";

// ==== Web server on port 80 ====
WebServer server(80);

// ==== GPS Setup ====
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Use UART2
const int RXD2 = 16;
const int TXD2 = 17;

// ==== HTML page with Leaflet.js Map ====
String getHtmlPage(double lat, double lng) {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32 GPS Tracker</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <link rel="stylesheet" href="https://unpkg.com/leaflet/dist/leaflet.css" />
      <style>
        body { margin:0; font-family: Arial; text-align:center; }
        #map { height: 90vh; width: 100%; }
        h2 { color: #4CAF50; }
      </style>
    </head>
    <body>
      <h2>ESP32 Live GPS Tracker 🌍</h2>
      <div id="map"></div>

      <script src="https://unpkg.com/leaflet/dist/leaflet.js"></script>
      <script>
        var map = L.map('map').setView([)rawliteral" + String(lat, 6) + "," + String(lng, 6) + R"rawliteral(], 18);
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
          attribution: 'Map data © OpenStreetMap contributors',
        }).addTo(map);
        var marker = L.marker([)rawliteral" + String(lat, 6) + "," + String(lng, 6) + R"rawliteral(]).addTo(map)
          .bindPopup('Current Location')
          .openPopup();
        setTimeout(() => { location.reload(); }, 5000); // Auto-refresh every 5s
      </script>
    </body>
    </html>
  )rawliteral";
  return html;
}

// ==== Web Server Route ====
void handleRoot() {
  double lat = gps.location.lat();
  double lng = gps.location.lng();

  if (gps.location.isValid()) {
    server.send(200, "text/html", getHtmlPage(lat, lng));
  } else {
    server.send(200, "text/html", "<h2>Waiting for GPS signal...</h2><meta http-equiv='refresh' content='2'>");
  }
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  // GPS Serial setup
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS Serial Started");

  // WiFi Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // Web Server route
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started!");
}

// ==== Loop ====
void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  server.handleClient();
}
