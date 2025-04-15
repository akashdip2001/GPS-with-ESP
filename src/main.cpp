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
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32 GPS Tracker</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <meta http-equiv='refresh' content='5'>
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
    <div class="info">
      Latitude: %LAT%<br>
      Longitude: %LNG%<br>
      Satellites: %SAT%<br>
      Speed: %SPD% km/h
    </div>

    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script>
      var map = L.map('map').setView([%LAT%, %LNG%], 16);
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap contributors'
      }).addTo(map);

      var icon = L.divIcon({ className: 'custom-pin' });
      var marker = L.marker([%LAT%, %LNG%], { icon: icon }).addTo(map);
    </script>
  </body>
  </html>
  )rawliteral";

  if (gps.location.isValid()) {
    page.replace("%LAT%", String(gps.location.lat(), 6));
    page.replace("%LNG%", String(gps.location.lng(), 6));
    page.replace("%SAT%", String(gps.satellites.value()));
    page.replace("%SPD%", String(gps.speed.kmph()));
  } else {
    page.replace("%LAT%", "0");
    page.replace("%LNG%", "0");
    page.replace("%SAT%", "0");
    page.replace("%SPD%", "0");
  }

  return page;
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
}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  server.handleClient();
}
