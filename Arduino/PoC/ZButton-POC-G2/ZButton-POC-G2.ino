#include <Pushbutton.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define WIFI_SSID     "public.altair.is"
#define WIFI_PASS     "Altairis_OPEN"
#define SERVER_NAME   "altairtrackdot.azurewebsites.net"
#define SERVER_PATH   "/L-ZBUTTON"
#define SERVER_PORT   443
#define USER_AGENT    "Z-Button/PoC"

BearSSL::WiFiClientSecure client;
byte mac[6];
Pushbutton buttonStop(D5, PULL_UP_ENABLED, DEFAULT_STATE_LOW);

void setup() {
  // Initialize serial port
  Serial.begin(9600);
  delay(200);
  Serial.println();

  // Print MAC Address
  WiFi.macAddress(mac);
  Serial.printf("OK %s %02X%02X%02X%02X%02X%02X\n", USER_AGENT, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Disable certificate verification
  client.setInsecure();
}

bool notifyEvent(char* eventSource, char* eventName) {
  // Connect to server
  Serial.printf("Connecting to %s:%i...", SERVER_NAME, SERVER_PORT);
  bool connectResult = client.connect(SERVER_NAME, SERVER_PORT);
  if (!connectResult) {
    Serial.println("Can't connect!");
    return false;
  }
  Serial.println("OK");

  // Send GET request
  Serial.printf("GET %s?mac=%02X%02X%02X%02X%02X%02X&src=%s&evt=%s HTTP/1.1\n", SERVER_PATH, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], eventSource, eventName);
  client.printf("GET %s?mac=%02X%02X%02X%02X%02X%02X&src=%s&evt=%s HTTP/1.1\r\n", SERVER_PATH, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], eventSource, eventName);
  client.printf("Host: %s\r\n", SERVER_NAME);
  client.printf("User-Agent: %s\r\n", USER_AGENT);
  client.print("Connection: close\r\n\r\n");
  Serial.println("OK, waiting for response");

  // Read response
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }
  Serial.println("Client disconnected.");
  return true;
}

void ensureWiFiConnected() {
  if(WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(WiFi.localIP());
  notifyEvent("SYSTEM", "CONNECTED");
}

void loop() {
  // Keep connected to WiFi
  ensureWiFiConnected();
  
  // Check button state
  if(buttonStop.getSingleDebouncedPress()) notifyEvent("BUTTON_STOP", "PRESS");
  if(buttonStop.getSingleDebouncedRelease()) notifyEvent("BUTTON_STOP", "RELEASE");
}
