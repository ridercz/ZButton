#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define LED_PIN       D4
#define WIFI_SSID     "public.altair.is"
#define WIFI_PASS     "Altairis_OPEN"
#define SERVER_NAME   "altairtrackdot.azurewebsites.net"
#define SERVER_PORT   443
#define GREEN_DELAY   0
#define GREEN_PATH    "/L-GREEN"
#define YELLOW_DELAY  1000
#define YELLOW_PATH   "/L-YELLOW"
#define RED_DELAY     1000
#define RED_PATH      "/L-RED"
#define USER_AGENT    "Z-Button/PoC"

Adafruit_NeoPixel pixels(1, LED_PIN, NEO_RGB + NEO_KHZ800);
BearSSL::WiFiClientSecure client;
byte mac[6];

void setup() {
  // Initialize serial port
  Serial.begin(9600);
  delay(200);
  Serial.println();

  // Light up LED blue
  Serial.print("Initializing LED...");
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0x00, 0x00, 0xFF));
  pixels.show();
  Serial.println("OK");

  // Print MAC Address
  WiFi.macAddress(mac);
  Serial.printf("BSSID: %02X%02X%02X%02X%02X%02X\n", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(WiFi.localIP());

  // Disable certificate verification
  client.setInsecure();

  // Perform HTTP requests
  delay(GREEN_DELAY);
  while (!requestUrl(GREEN_PATH, 0x000080, 0x00FF00));
  delay(YELLOW_DELAY);
  while (!requestUrl(YELLOW_PATH, 0x008000, 0xFFFF00));
  delay(RED_DELAY);
  while (!requestUrl(RED_PATH, 0x808000, 0xFF0000));
}

bool requestUrl(char* path, unsigned long color1, unsigned long color2) {
  // Set first color
  pixels.setPixelColor(0, color1);
  pixels.show();

  // Connect to server
  Serial.printf("Requesting %s... ", path);
  bool connectResult = client.connect(SERVER_NAME, SERVER_PORT);
  if (!connectResult) {
    Serial.println("Can't connect!");
    return false;
  }

  // Send GET request
  client.printf("GET %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", SERVER_NAME);
  client.printf("User-Agent: %s\r\n", USER_AGENT);
  client.printf("X-ZButton-BSSID: %02X%02X%02X%02X%02X%02X\n", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  client.print("Connection: close\r\n\r\n");
  Serial.println("OK");

  // Read response
  while (client.connected()) {
    bool responseStarted = false;
    if (client.available()) {
      if (!responseStarted) {
        // Set second color
        pixels.setPixelColor(0, color2);
        pixels.show();
        responseStarted = true;
      }
      char c = client.read();
      Serial.print(c);
    }
  }
  return true;
}

void loop() {
  // Do nothing in loop
}
