#include <Arduino.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>

// Set up Wi-Fi credentials
const char *ssid = "ESP32-GPS-AP"; // Wi-Fi Name (Access Point)
const char *password = "12345678"; // Wi-Fi Password (min 8 chars)

// Set up GPS serial communication
HardwareSerial mySerial(1);  // Using UART1 (RX = GPIO 16, TX = GPIO 17)
TinyGPSPlus gps;  // GPS object

// IP address for AP
IPAddress local_IP(192, 168, 4, 1);   // Static IP
IPAddress gateway(192, 168, 4, 1);    // Gateway IP (same as AP)
IPAddress subnet(255, 255, 255, 0);   // Subnet mask

// Create web server on port 80
WiFiServer server(80);

void setup() {
  // Start Serial Monitor for debugging
  Serial.begin(115200);
  
  // Set up GPS serial port
  mySerial.begin(9600, SERIAL_8N1, 16, 17);  // Baud rate = 9600
  
  // Set up Wi-Fi AP mode
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started!");
  Serial.print("Connect to: "); Serial.println(ssid);
  Serial.print("IP address: "); Serial.println(WiFi.softAPIP());

  // Start web server
  server.begin();
}

void loop() {
  // Wait for a client to connect
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends data
  Serial.println("Client connected!");
  while (!client.available()) {
    delay(1);
  }

  // Read HTTP request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();

  // Read GPS data
  while (mySerial.available() > 0) {
    gps.encode(mySerial.read());
  }

  // GPS location logic
  float latitude = gps.location.lat();
  float longitude = gps.location.lng();

  // HTML content
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:sans-serif;text-align:center;}h2{color:green;}</style></head>";
  html += "<body><h2>GPS Location</h2>";
  
  if (gps.location.isUpdated()) {
    html += "<p>Latitude: " + String(latitude, 6) + "</p>";
    html += "<p>Longitude: " + String(longitude, 6) + "</p>";
  } else {
    html += "<p>Waiting for GPS fix...</p>";
  }

  html += "</body></html>";

  // Send response
  client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  client.print(html);

  delay(1);
  Serial.println("Client disconnected");
}
