#include <Pushbutton.h>
#include <ESP8266WiFi.h>

#define WIFI_SSID     "public.altair.is"
#define WIFI_PASS     "Altairis_OPEN"
#define SERVER_NAME   "altairtrackdot.azurewebsites.net"
#define SERVER_PATH   "/L-ZBUTTON"
#define SERVER_PORT   443
#define USER_AGENT    "Z-Button/PoC"
#define SECRET        "ThisIsSecret"

byte mac[6];
Pushbutton buttonStop(D5, PULL_UP_ENABLED, DEFAULT_STATE_LOW);
BearSSL::WiFiClientSecure client;

void setup() {
  // Initialize serial port
  Serial.begin(9600);
  delay(200);

  // Print welcome banner
  WiFi.macAddress(mac);
  Serial.printf("OK %s %02X%02X%02X%02X%02X%02X\n", USER_AGENT, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Disable certificate verification
  client.setInsecure();
}

void loop() {
  // Keep connected to WiFi
  ensureWiFiConnected();

  // Check button state
  if (buttonStop.getSingleDebouncedPress()) notifyEvent("BUTTON_STOP", "PRESS");
  if (buttonStop.getSingleDebouncedRelease()) notifyEvent("BUTTON_STOP", "RELEASE");
}

bool notifyEvent(char* eventSource, char* eventName) {
  // Construct query string to sign
  char qsBytes[128];
  sprintf(qsBytes, "mac=%02X%02X%02X%02X%02X%02X&src=%s&evt=%s", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], eventSource, eventName);

  // Get signature
  byte sigBytes[32];
  sign(qsBytes, SECRET, sigBytes);

  // Convert signature to Base16
  byte encodedSigBytes[64];
  toHexString(sigBytes, 32, encodedSigBytes);

  // Connect to server
  Serial.printf("Connecting to %s:%i...", SERVER_NAME, SERVER_PORT);
  bool connectResult = client.connect(SERVER_NAME, SERVER_PORT);
  if (!connectResult) {
    Serial.println("Can't connect!");
    return false;
  }
  Serial.println("OK");

  // Send GET request
  Serial.printf("GET %s?%s&sig=%s\n", SERVER_PATH, qsBytes, encodedSigBytes);
  client.printf("GET %s?%s&sig=%s HTTP/1.1\r\n", SERVER_PATH, qsBytes, encodedSigBytes);
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
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(WiFi.localIP());
  notifyEvent("SYSTEM", "CONNECTED");
}

void sign(String payload, String secret, byte buffer[32]) {
  br_hmac_key_context keyCtx;
  br_hmac_key_init(&keyCtx, &br_sha256_vtable, secret.c_str(), secret.length());
  br_hmac_context hmacCtx;
  br_hmac_init(&hmacCtx, &keyCtx, 0);
  br_hmac_update(&hmacCtx, payload.c_str(), payload.length());
  br_hmac_out(&hmacCtx, buffer);
}

void toHexString(byte array[], unsigned int len, byte buffer[]) {
  for (unsigned int i = 0; i < len; i++)  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}
