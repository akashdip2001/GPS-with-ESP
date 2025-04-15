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

// ==== HTML Page ====
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

      
    .floating-buttons {
    position: absolute;
    top: 90px;
    right: 20px;
    z-index: 999;
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  
  .floating-buttons button {
    padding: 10px 14px;
    border: none;
    border-radius: 8px;
    color: white;
    font-size: 14px;
    cursor: pointer;
    box-shadow: 0 2px 6px rgba(0,0,0,0.2);
    display: flex;
    align-items: center;
    gap: 8px;
    transition: background-color 0.3s;
  }

  .floating-buttons .center-btn { background-color: #3498db; }
  .floating-buttons .center-btn:hover { background-color: #2980b9; }

  .floating-buttons .yt-btn { background-color: crimson; }
  .floating-buttons .yt-btn:hover { background-color: #b30000; }

  .floating-buttons .gh-btn { background-color: black; }
  .floating-buttons .gh-btn:hover { background-color: #333; }

  .floating-buttons button svg {
    width: 16px;
    height: 16px;
  }

    </style>
  </head>
  <body>
    <h2>ESP32 GPS Tracker</h2>
    <div id="map"></div>
    <div class="floating-buttons">
  <button class="center-btn" onclick="centerToLocation()">Where am I ?</button>
  <button class="yt-btn" onclick="window.open('https://www.youtube.com/playlist?list=PL_RecMEcs_p-5UwLqFBFtat90L8IOc1bZ', '_blank')">
    <svg fill="white" viewBox="0 0 24 24"><path d="M23.5 6.2s-.2-1.6-.9-2.3c-.9-.9-1.9-.9-2.4-1-3.3-.2-8.3-.2-8.3-.2h-.1s-5 .1-8.3.2c-.5.1-1.5.1-2.4 1C.7 4.6.5 6.2.5 6.2S.3 7.9.3 9.6v1.8c0 1.7.2 3.4.2 3.4s.2 1.6.9 2.3c.9.9 2.2.9 2.8 1 2 .2 8.3.2 8.3.2s5 0 8.3-.2c.5-.1 1.5-.1 2.4-1 .7-.7.9-2.3.9-2.3s.2-1.7.2-3.4v-1.8c0-1.7-.2-3.4-.2-3.4zM9.8 14.6V8.4l6.1 3.1-6.1 3.1z"/></svg>
    YouTube
  </button>
  <button class="gh-btn" onclick="window.open('https://github.com/akashdip2001/GPS-with-ESP/blob/main/README.md', '_blank')">
    <svg fill="white" viewBox="0 0 24 24"><path d="M12 .5C5.6.5.5 5.6.5 12c0 5.1 3.3 9.4 7.9 10.9.6.1.8-.3.8-.6v-2.2c-3.2.7-3.8-1.4-3.8-1.4-.5-1.3-1.1-1.6-1.1-1.6-.9-.6.1-.6.1-.6 1 .1 1.6 1 1.6 1 .9 1.6 2.4 1.1 3 .9.1-.7.4-1.1.7-1.4-2.5-.3-5.1-1.3-5.1-5.6 0-1.2.4-2.1 1-2.9-.1-.3-.4-1.4.1-2.8 0 0 .8-.3 2.9 1 .8-.2 1.6-.3 2.4-.3s1.6.1 2.4.3c2.1-1.3 2.9-1 2.9-1 .5 1.4.2 2.5.1 2.8.6.8 1 1.7 1 2.9 0 4.3-2.6 5.2-5.1 5.6.4.3.7 1 .7 2v3c0 .3.2.7.8.6C20.2 21.4 24 17.2 24 12c0-6.4-5.1-11.5-12-11.5z"/></svg>
    GitHub
  </button>
</div>

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

  let firstLoad = true;

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

      if (firstLoad && lat !== 0 && lng !== 0) {
        map.flyTo([lat, lng], 16, { animate: true, duration: 1.5 });
        firstLoad = false;
      }

      // Save location globally for the center button
      window.lastGPS = [lat, lng];
    } catch (err) {
      console.error("Failed to update location", err);
    }
  }

  // First update
  updateLocation();
  setInterval(updateLocation, 5000);

  // Center button function
  function centerToLocation() {
    if (window.lastGPS) {
      map.flyTo(window.lastGPS, 16, { animate: true, duration: 1.2 });
    }
  }
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
