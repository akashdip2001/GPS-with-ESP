#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPSPlus.h>
#include <ArduinoJson.h>
#include <map>

// ==== WiFi Credentials ====
const char* ssid = "spa";
const char* password = "12345678";

// ==== Create Web Server and WebSocket ====
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==== GPS Setup ====
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // UART2
const int RXD2 = 16;
const int TXD2 = 17;

// ==== Track clients ====
std::map<uint32_t, String> clientIDs;

// ==== HTML Page ====
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Real-Time Location Sharing</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <style>
    body { margin:0; font-family: Arial, sans-serif; text-align: center; background: #f4f4f4; }
    h2 { color: #2c3e50; margin: 10px 0; }
    #map { height: 80vh; width: 100%; margin: 10px 0; border: 2px solid #2c3e50; border-radius: 10px; }
    .info { font-size: 18px; padding: 10px; }
    .custom-pin { background-color: red; border-radius: 50%; width: 14px; height: 14px; display: block; border: 2px solid white; }
    .client-pin { background-color: blue; border-radius: 50%; width: 14px; height: 14px; display: block; border: 2px solid white; }
    .self-pin { background-color: green; border-radius: 50%; width: 14px; height: 14px; display: block; border: 2px solid white; }
  </style>
</head>
<body>
  <h2>Real-Time Location Sharing</h2>
  <div id="map"></div>
  <div class="info" id="info">Connecting to server...</div>
  
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
  // Prompt for user name
  let userName = prompt("Please enter your name:") || "Anonymous";

  // Generate a random client ID string
  const clientId = "client_" + Math.random().toString(36).substr(2, 9);

  // Initialize the map centered at [0,0]
  let map = L.map('map').setView([0, 0], 2);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; OpenStreetMap contributors'
  }).addTo(map);

  // Marker store
  let markers = {};

  // WebSocket connection
  const ws = new WebSocket('ws://' + window.location.hostname + '/ws');

  ws.onopen = function () {
    document.getElementById('info').innerHTML = 'Connected to server.';
    console.log("WebSocket connected. Trying to access geolocation...");

    // Start watching location if allowed
    if (navigator.geolocation) {
      navigator.geolocation.watchPosition(sendClientLocation, function (error) {
        console.error("Geolocation error:", error);
        alert("Location access failed: " + error.message);
      }, {
        enableHighAccuracy: true,
        maximumAge: 3000,
        timeout: 5000
      });
    } else {
      console.error("Geolocation not supported by this browser.");
      alert("Geolocation not supported by this browser.");
    }
  };

  ws.onmessage = function (event) {
    let data = JSON.parse(event.data);
    let id = data.id;
    let lat = data.lat;
    let lng = data.lng;
    let type = data.type;
    let name = data.name || (type === "module" ? "GPS Module" : "Client");

    let iconClass = (type === "module") ? 'custom-pin' : 'client-pin';

    if (markers[id]) {
      markers[id].setLatLng([lat, lng]);
    } else {
      let customIcon = L.divIcon({ className: iconClass });
      markers[id] = L.marker([lat, lng], { icon: customIcon }).addTo(map).bindPopup(name).openPopup();

      // Zoom to first-time user location (self only)
      if (id === clientId) {
        map.setView([lat, lng], 16, { animate: true, duration: 2 });
      }
    }

    // Update name in popup
    if (markers[id].getPopup()) {
      markers[id].getPopup().setContent(name);
    }
  };

  ws.onclose = function () {
    document.getElementById('info').innerHTML = 'Disconnected from server.';
  };

  function sendClientLocation(position) {
    let message = {
      type: "client",
      id: clientId,
      name: userName,
      lat: position.coords.latitude,
      lng: position.coords.longitude
    };
    ws.send(JSON.stringify(message));
    console.log("Location sent:", message);
  }
</script>
</body>
</html>
)rawliteral";

// ==== WebSocket Event Handler ====
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    if (clientIDs.count(client->id())) {
      DynamicJsonDocument doc(128);
      doc["type"] = "remove";
      doc["id"] = clientIDs[client->id()];
      String msg;
      serializeJson(doc, msg);
      ws.textAll(msg);
      clientIDs.erase(client->id());
    }
  }
  else if (type == WS_EVT_DATA) {
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, data, len);
    if (!err) {
      String msgType = doc["type"];
      if (msgType == "client") {
        String customId = doc["id"];
        clientIDs[client->id()] = customId;
      }
      ws.textAll((const char*)data, len);
      Serial.printf("Broadcasted message: %s\n", data);
    } else {
      Serial.println("JSON parse error from client.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // ==== Connect to WiFi ====
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ==== Serve HTML Page ====
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", htmlPage);
  });

  // ==== Setup WebSocket ====
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ==== Start Server ====
  server.begin();
  Serial.println("HTTP & WebSocket server started");
}

// ==== Periodic GPS Broadcasting ====
unsigned long lastBroadcast = 0;

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (millis() - lastBroadcast > 5000) {
    lastBroadcast = millis();
    DynamicJsonDocument doc(256);
    doc["type"] = "module";
    doc["id"] = "module";
    if (gps.location.isValid()) {
      doc["lat"] = gps.location.lat();
      doc["lng"] = gps.location.lng();
    } else {
      doc["lat"] = 0;
      doc["lng"] = 0;
    }
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
    Serial.println("Sent GPS location: " + json);
  }
}
