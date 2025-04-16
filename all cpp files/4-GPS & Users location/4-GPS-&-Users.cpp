#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>    // For JSON parsing/serialization
#include <map>

// ==== WiFi Credentials ====
const char* ssid = "spa";
const char* password = "12345678";

// ==== Create AsyncWebServer on port 80 and WebSocket at "/ws" ====
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==== GPS Setup ====
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Using UART2 on pins 16 (RX) and 17 (TX)
const int RXD2 = 16;
const int TXD2 = 17;

// ==== Global Map to store client ID mapping ====
// This maps the ESPAsyncWebSocketClient's numeric ID to the client’s custom ID (e.g., username or client id)
std::map<uint32_t, String> clientIDs;

// ==== HTML Page Content ====
// This page sets up a Leaflet map, prompts the user for a name, 
// opens a WebSocket connection to send both the user’s geolocation (using the Geolocation API)
// and display both the ESP32 module’s position and every connected user’s position.
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
  </style>
</head>
<body>
  <h2>Real-Time Location Sharing</h2>
  <div id="map"></div>
  <div class="info" id="info">Connecting to server...</div>
  
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
    // Prompt for user name (you may expand this later to a nicer login)
    let userName = prompt("Please enter your name:") || "Anonymous";

    // Generate a random client ID string
    const clientId = "client_" + Math.random().toString(36).substr(2, 9);

    // Initialize the map centered at [0,0]
    let map = L.map('map').setView([0,0], 2);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; OpenStreetMap contributors'
    }).addTo(map);
    
    // Maintain a dictionary of markers indexed by an identifier.
    let markers = {};

    // Establish a WebSocket connection to the ESP32
    const ws = new WebSocket('ws://' + window.location.hostname + '/ws');
    
    ws.onopen = function() {
      document.getElementById('info').innerHTML = 'Connected to server.';
      // Send an initial message with your info (even before geolocation data is available)
      sendClientLocation({coords: {latitude: 0, longitude: 0}});
      // If supported, watch the geolocation to send updates continuously
      if (navigator.geolocation) {
        navigator.geolocation.watchPosition(sendClientLocation, function(error) {
          console.error("Geolocation error:", error);
        }, { enableHighAccuracy: true, maximumAge: 3000, timeout: 5000 });
      } else {
        console.error("Geolocation not supported by this browser.");
      }
    };

    ws.onmessage = function(event) {
      // Parse the incoming JSON message
      let data = JSON.parse(event.data);
      let id = data.id;
      let lat = data.lat;
      let lng = data.lng;
      let type = data.type; // "module" or "client"
      
      if (type === "module" || type === "client") {
        // Create or update a marker for this id.
        // When available, bind a popup showing the name.
        let iconClass = (type === "module") ? 'custom-pin' : 'client-pin';
        if (markers[id]) {
          markers[id].setLatLng([lat, lng]);
        } else {
          let customIcon = L.divIcon({ className: iconClass });
          markers[id] = L.marker([lat, lng], {icon: customIcon}).addTo(map);
          if (data.name) {
            markers[id].bindPopup(data.name);
          } else if(type === "module") {
            markers[id].bindPopup("GPS Module");
          }
        }
        // Update popup text if needed
        if(data.name && markers[id].getPopup()) {
          markers[id].getPopup().setContent(data.name);
        }
      } else if (type === "remove") {
        // Remove marker if a client disconnects.
        if (markers[id]) {
          map.removeLayer(markers[id]);
          delete markers[id];
        }
      }
    };

    ws.onclose = function() {
      document.getElementById('info').innerHTML = 'Disconnected from server.';
    };

    // Function to send the client's geolocation along with the user name
    function sendClientLocation(position) {
      let message = {
        type: "client",
        id: clientId,
        name: userName,
        lat: position.coords.latitude,
        lng: position.coords.longitude
      };
      ws.send(JSON.stringify(message));
    }
  </script>
</body>
</html>
)rawliteral";

// ==== WebSocket Event Handler ====
// This function processes connection events, data received, and disconnect events.
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void * arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    // If we have stored an ID from this client, broadcast a removal message.
    if (clientIDs.find(client->id()) != clientIDs.end()) {
      String removeMsg;
      // Build JSON removal message.
      DynamicJsonDocument doc(128);
      doc["type"] = "remove";
      doc["id"] = clientIDs[client->id()];
      serializeJson(doc, removeMsg);
      ws.textAll(removeMsg);
      // Remove from our client mapping.
      clientIDs.erase(client->id());
    }
  }
  else if (type == WS_EVT_DATA) {
    // When data is received from a client, decode the JSON.
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, data, len);
    if (!error) {
      String msgType = doc["type"];
      if (msgType == "client") {
        // Save the client’s provided ID so we can refer to it on disconnect.
        String customId = doc["id"];
        clientIDs[client->id()] = customId;
      }
      // Broadcast the message to every connected client.
      ws.textAll((const char*)data, len);
      Serial.printf("Broadcasted message: %s\n", data);
    } else {
      Serial.println("Failed to parse JSON from client.");
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

  // ==== Serve the HTML page ====
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", htmlPage);
  });

  // ==== WebSocket Setup ====
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ==== Start the server ====
  server.begin();
  Serial.println("HTTP & WebSocket server started");
}

// ==== Broadcast the module’s GPS data every 5 seconds ===
unsigned long lastModuleBroadcast = 0;
void loop() {
  // Process incoming GPS data from the module.
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  
  if (millis() - lastModuleBroadcast > 5000) {
    lastModuleBroadcast = millis();
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
  }
}
