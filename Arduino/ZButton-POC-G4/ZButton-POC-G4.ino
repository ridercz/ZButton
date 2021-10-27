#include <Pushbutton.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

#define USER_AGENT      "Z-Button/PoC"
#define CONFIG_TIMEOUT  1000
#define CONFIG_FILE     "/config.txt"
#define CONFIG_BUFFER   512

// Configuration

struct Config {
  char WifiSsid[64];
  char WifiPass[64];
  char ServerName[64];
  char ServerPath[64];
  char Secret[64];
  unsigned int ServerPort;
  unsigned long Sequence;
};

// Declarations

Config config;
bool configMode = false;
byte mac[6];
Pushbutton buttonStop(D5, PULL_UP_ENABLED, DEFAULT_STATE_LOW);
BearSSL::WiFiClientSecure client;
StaticJsonDocument<CONFIG_BUFFER> cfgJson;

// Main

void setup() {
  // Initialize serial port
  Serial.begin(9600);
  delay(200);

  // Print welcome banner
  WiFi.macAddress(mac);
  Serial.printf("OK %s %02X%02X%02X%02X%02X%02X\n", USER_AGENT, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Initialize SPIFFS
  Serial.print("Initializing SPIFFS...");
  if (SPIFFS.begin()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    Serial.println("System halted.");
    while (true);
  }

  // Load configuration
  loadConfigFromFile();

  // Start config mode timeout if not already in it
  if (!configMode) {
    Serial.printf("Send 0x10 in next %i ms to enter configuration mode...\n", CONFIG_TIMEOUT);
    unsigned long configModeTimeout = millis() + CONFIG_TIMEOUT;
    while (millis() < configModeTimeout) {
      if (Serial.available()) {
        configMode = true;
        break;
      }
    }
  }

  // Read configuration from serial
  if (configMode) readConfigFromSerial();

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.WifiSsid, config.WifiPass);

  // Disable certificate verification
  client.setInsecure();
  if (!configMode) Serial.println("Initialization done.");
}

void loop() {
  if (configMode) {
    // Configuration mode
  } else {
    // Keep connected to WiFi
    ensureWiFiConnected();

    // Check button state
    if (buttonStop.getSingleDebouncedPress()) notifyEvent("BUTTON_STOP", "PRESS");
    if (buttonStop.getSingleDebouncedRelease()) notifyEvent("BUTTON_STOP", "RELEASE");
  }
}

// Functional methods

void readConfigFromSerial() {
  Serial.println("Configuration mode:");

  // Read configuration from serial
  char lineBuffer[64];

  Serial.println("WifiSsid=?");
  readLine(lineBuffer);
  cfgJson["wifiSsid"] = lineBuffer;
  Serial.println(lineBuffer);

  Serial.println("WifiPass=?");
  readLine(lineBuffer);
  cfgJson["wifiPass"] = lineBuffer;
  Serial.println(lineBuffer);

  Serial.println("ServerName=?");
  readLine(lineBuffer);
  cfgJson["serverName"] = lineBuffer;
  Serial.println(lineBuffer);

  Serial.println("ServerPort=?");
  readLine(lineBuffer);
  cfgJson["serverPort"] = String(lineBuffer).toInt();
  Serial.println(lineBuffer);

  Serial.println("ServerPath=?");
  readLine(lineBuffer);
  cfgJson["serverPath"] = lineBuffer;
  Serial.println(lineBuffer);

  Serial.println("Secret=?");
  readLine(lineBuffer);
  cfgJson["secret"] = lineBuffer;
  Serial.println(lineBuffer);

  Serial.println("Sequence=?");
  readLine(lineBuffer);
  cfgJson["sequence"] = String(lineBuffer).toInt();
  Serial.println(lineBuffer);

  // Save to file
  Serial.print("Saving configuration...");
  if (saveConfigToFile()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    Serial.println("System halted.");
    while (true);
  }

  Serial.println("Rebooting system...");
  ESP.restart();
}

bool saveConfigToFile() {
  File cfgFile = SPIFFS.open(CONFIG_FILE, "w");
  bool result = serializeJson(cfgJson, cfgFile);
  if (result) cfgFile.close();
  return result;
}

void loadConfigFromFile() {
  // Load configuration from file
  Serial.print("Loading configuration...");
  File cfgFile = SPIFFS.open(CONFIG_FILE, "r");
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    if (SPIFFS.exists(CONFIG_FILE)) {
      Serial.println("Failed, can't parse!");
    } else {
      Serial.println("Failed, not found!");
    }
    Serial.println("Entering configuration mode...");
    configMode = true;
  } else {
    strlcpy(config.Secret, cfgJson["secret"], sizeof(config.Secret));
    strlcpy(config.WifiSsid, cfgJson["wifiSsid"], sizeof(config.WifiSsid));
    strlcpy(config.WifiPass, cfgJson["wifiPass"], sizeof(config.WifiPass));
    strlcpy(config.ServerName, cfgJson["serverName"], sizeof(config.ServerName));
    strlcpy(config.ServerPath, cfgJson["serverPath"], sizeof(config.ServerPath));
    config.ServerPort = cfgJson["serverPort"] | 443;
    config.Sequence = cfgJson["sequence"] | 0;
    Serial.println("OK");
  }
}

bool notifyEvent(char* eventSource, char* eventName) {
  // Increment sequence number
  config.Sequence++;
  cfgJson["sequence"] = config.Sequence;
  saveConfigToFile();
  
  // Construct query string to sign
  char qsBytes[128];
  sprintf(qsBytes, "mac=%02X%02X%02X%02X%02X%02X&seq=%i&src=%s&evt=%s", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], config.Sequence, eventSource, eventName);

  // Get signature
  byte sigBytes[32];
  sign(qsBytes, config.Secret, sigBytes);

  // Convert signature to Base16
  byte encodedSigBytes[64];
  toHexString(sigBytes, 32, encodedSigBytes);

  // Connect to server
  Serial.printf("Connecting to %s:%i...", config.ServerName, config.ServerPort);
  bool connectResult = client.connect(config.ServerName, config.ServerPort);
  if (!connectResult) {
    Serial.println("Can't connect!");
    return false;
  }
  Serial.println("OK");

  // Send GET request
  Serial.printf("GET %s?%s&sig=%s\n", config.ServerPath, qsBytes, encodedSigBytes);
  client.printf("GET %s?%s&sig=%s HTTP/1.1\r\n", config.ServerPath, qsBytes, encodedSigBytes);
  client.printf("Host: %s\r\n", config.ServerName);
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

  Serial.printf("Connecting to WiFi %s...", config.WifiSsid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  notifyEvent("SYSTEM", "CONNECTED");
}

// Helper methods

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

void readLine(char* buffer) {
  unsigned int idx = 0;
  char c;
  do {
    while (Serial.available() == 0);
    c = Serial.read();
    buffer[idx] = c;
    idx++;
  }
  while (c != '\n' && c != '\r');
  if (idx > 0)idx--;
  buffer[idx] = 0;
}
