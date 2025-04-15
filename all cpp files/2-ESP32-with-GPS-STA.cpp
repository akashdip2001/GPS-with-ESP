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
  String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<meta http-equiv='refresh' content='5'>";
  page += "<style>body{font-family:Arial; text-align:center;}h2{color:#2F4F4F;}</style></head><body>";
  page += "<h2>ESP32 GPS WebServer</h2>";

  if (gps.location.isValid()) {
    double lat = gps.location.lat();
    double lng = gps.location.lng();

    page += "<p><strong>Latitude:</strong> " + String(lat, 6) + "</p>";
    page += "<p><strong>Longitude:</strong> " + String(lng, 6) + "</p>";
    page += "<p><strong>Altitude:</strong> " + String(gps.altitude.meters()) + " meters</p>";
    page += "<p><strong>Satellites:</strong> " + String(gps.satellites.value()) + "</p>";
    page += "<p><strong>Speed:</strong> " + String(gps.speed.kmph()) + " km/h</p>";
    page += "<p><a href='https://www.google.com/maps?q=" + String(lat, 6) + "," + String(lng, 6) + "' target='_blank'>";
    page += "click to -> Open in Google-Maps</a></p>";
  } else {
    page += "<p><strong>Waiting for valid GPS data...</strong></p>";
  }

  page += "<br><p>Auto-refresh every 5 seconds to get real-time GPS data.</p>";
  page += "</body></html>";
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
