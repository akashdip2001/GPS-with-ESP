#include <WiFi.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WebServer.h>

// ==== Replace with your network settings ====
const char* ssid = "spa";
const char* password = "12345678";

// ==== Static IP Configuration ====
IPAddress staticIP(192, 168, 1, 100);  // Set your desired static IP
IPAddress gateway(192, 168, 1, 1);     // Router's IP
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);            // Google DNS

// ==== Web server on port 80 ====
WebServer server(80);

// ==== GPS Setup ====
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Use UART2
const int RXD2 = 16;
const int TXD2 = 17;

// ==== HTML Page with Leaflet.js Map ====
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
  if (!gps.location.isValid() || gps.location.age() > 5000) {
    server.send(200, "text/html", "<h2>GPS signal lost!</h2><meta http-equiv='refresh' content='2'>");
    return;
  }
  server.send(200, "text/html", getHtmlPage(gps.location.lat(), gps.location.lng()));
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  // GPS Serial setup
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS Serial Started");

  // WiFi Static IP Setup
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. Static IP: " + WiFi.localIP().toString());

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