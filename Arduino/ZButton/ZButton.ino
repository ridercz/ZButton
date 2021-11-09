#include <Pushbutton.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

#define USER_AGENT      "Z-Button/1.1.0"
#define CONFIG_TIMEOUT  3000
#define CONFIG_FILE     "/cfg-v001.txt"
#define CONFIG_BUFFER   512

// Configuration

struct Config {
  char WifiSsid[65];
  char WifiPass[65];
  char ServerName[65];
  char ServerPath[65];
  char Secret[65];
  char ClientId[65];
  unsigned int ServerPort;
  unsigned long Sequence;
};

// Declarations

Config config;
byte mac[6];
unsigned long hwid = ESP.getChipId();
Pushbutton buttonStop(D5, PULL_UP_ENABLED, DEFAULT_STATE_LOW);
Pushbutton buttonYellow(D6, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton buttonGreen(D7, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
BearSSL::WiFiClientSecure client;
StaticJsonDocument<CONFIG_BUFFER> cfgJson;

// Main

void setup() {
  // Initialize serial port
  Serial.begin(9600);
  delay(200);

  // Print welcome banner
  WiFi.macAddress(mac);
  Serial.printf("[=] Version=%s\n[=] MAC=%02X:%02X:%02X:%02X:%02X:%02X\n[=] HWID=%i\n",
                USER_AGENT,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                hwid);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("[x] Error initializing SPIFFS. System halted.");
    while (true);
  }

  // Load configuration
  loadConfigFromFile();
  Serial.printf("[=] ClientId=%s\n", config.ClientId);

  // Start config mode timeout
  Serial.println("[i] You can enter configuration mode now.");
  unsigned long configModeTimeout = millis() + CONFIG_TIMEOUT;
  bool configMode = false;
  while (millis() < configModeTimeout) {
    if (Serial.available()) {
      char c = Serial.read();
      configMode = true;
      break;
    }
  }

  // Read configuration from serial
  if (configMode) readConfigFromSerial();

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.WifiSsid, config.WifiPass);

  // Disable certificate verification
  client.setInsecure();
}

void loop() {
  // Keep connected to WiFi
  ensureWiFiConnected();

  // Check button state
  if (buttonStop.getSingleDebouncedPress()) notifyEvent("BUTTON_STOP", "PRESS");
  if (buttonYellow.getSingleDebouncedPress()) notifyEvent("BUTTON_YELLOW", "PRESS");
  if (buttonGreen.getSingleDebouncedPress()) notifyEvent("BUTTON_GREEN", "PRESS");
}

// Functional methods

void readConfigFromSerial() {
  Serial.println("[i] Entering configuration mode");

  // Read configuration from serial
  char lineBuffer[65];

  Serial.printf("[?] WifiSsid (%s):\n", config.WifiSsid);
  readLine(lineBuffer);
  cfgJson["wifiSsid"] = strlen(lineBuffer) ? lineBuffer : config.WifiSsid;

  Serial.println("[?] WifiPass (********):");
  readLine(lineBuffer);
  cfgJson["wifiPass"] = strlen(lineBuffer) ? lineBuffer : config.WifiPass;

  Serial.printf("[?] ServerName (%s):\n", config.ServerName);
  readLine(lineBuffer);
  cfgJson["serverName"] = strlen(lineBuffer) ? lineBuffer : config.ServerName;

  Serial.printf("[?] ServerPort (%i):\n", config.ServerPort);
  readLine(lineBuffer);
  cfgJson["serverPort"] = strlen(lineBuffer) ? String(lineBuffer).toInt() : config.ServerPort;

  Serial.printf("[?] ServerPath (%s):\n", config.ServerPath);
  readLine(lineBuffer);
  cfgJson["serverPath"] = strlen(lineBuffer) ? lineBuffer : config.ServerPath;

  Serial.printf("[?] ClientId (%s):\n", config.ClientId);
  readLine(lineBuffer);
  cfgJson["clientId"] = strlen(lineBuffer) ? lineBuffer : config.ClientId;

  Serial.println("[?] Secret (********):");
  readLine(lineBuffer);
  cfgJson["secret"] = strlen(lineBuffer) ? lineBuffer : config.Secret;

  Serial.printf("[?] Sequence (%i):\n", config.Sequence);
  readLine(lineBuffer);
  cfgJson["sequence"] = strlen(lineBuffer) ? String(lineBuffer).toInt() : config.Sequence;

  // Save to file
  if (!saveConfigToFile()) {
    Serial.println("[x] Error saving configuration file. System halted.");
    while (true);
  }
  Serial.println("[i] Rebooting system.");
  ESP.restart();
}

bool saveConfigToFile() {
  File cfgFile = SPIFFS.open(CONFIG_FILE, "w");
  bool result = serializeJson(cfgJson, cfgFile);
  if (result) cfgFile.close();
  return result;
}

void loadConfigFromFile() {
  // Create default configuration
  strlcpy(config.Secret, "0000000000000000000000000000000000000000000000000000000000000000", sizeof(config.Secret));
  strlcpy(config.WifiSsid, "Z-Button", sizeof(config.WifiSsid));
  strlcpy(config.WifiPass, "", sizeof(config.WifiPass));
  strlcpy(config.ServerName, "zbutton.local", sizeof(config.ServerName));
  strlcpy(config.ServerPath, "/", sizeof(config.ServerPath));
  strlcpy(config.ClientId, "0000000000000000000000000000000000000000000000000000000000000000", sizeof(config.ClientId));
  config.ServerPort = 443;
  config.Sequence = 0;

  // Try to load configuration from file
  File cfgFile = SPIFFS.open(CONFIG_FILE, "r");
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    if (SPIFFS.exists(CONFIG_FILE)) {
      Serial.println("[!] Cannot parse configuration file, using (probably incorrect) default values.");
    } else {
      Serial.println("[!] Configuration file not found, using (probably incorrect) default values.");
    }
    return;
  }

  // Read values
  strlcpy(config.Secret, cfgJson["secret"] | config.Secret, sizeof(config.Secret));
  strlcpy(config.WifiSsid, cfgJson["wifiSsid"] | config.WifiSsid, sizeof(config.WifiSsid));
  strlcpy(config.WifiPass, cfgJson["wifiPass"] | config.WifiPass, sizeof(config.WifiPass));
  strlcpy(config.ServerName, cfgJson["serverName"] | config.ServerName, sizeof(config.ServerName));
  strlcpy(config.ServerPath, cfgJson["serverPath"] | config.ServerPath, sizeof(config.ServerPath));
  strlcpy(config.ClientId, cfgJson["clientId"] | config.ClientId, sizeof(config.ClientId));
  config.ServerPort = cfgJson["serverPort"] | config.ServerPort;
  config.Sequence = cfgJson["sequence"] | config.Sequence;
}

bool notifyEvent(char* eventSource, char* eventName) {
  // Increment sequence number
  config.Sequence++;
  cfgJson["sequence"] = config.Sequence;
  saveConfigToFile();

  // Show event info
  Serial.printf("[i] Event: %s.%s (seq=%i)\n", eventSource, eventName, config.Sequence);

  // Construct query string to sign
  char qsBytes[1024];
  sprintf(qsBytes, "evt=%s.%s&seq=%i&clientid=%s", eventSource, eventName, config.Sequence, config.ClientId);

  // Get signature
  byte sigBytes[32];
  sign(qsBytes, config.Secret, sigBytes);

  // Convert signature to Base16
  byte encodedSigBytes[64];
  toHexString(sigBytes, 32, encodedSigBytes);

  // Connect to server
  Serial.printf("[i] Connecting to %s:%i...\n", config.ServerName, config.ServerPort);
  bool connectResult = client.connect(config.ServerName, config.ServerPort);
  if (!connectResult) {
    Serial.println("[!] Can't connect");
    return false;
  }

  // Send GET request
  Serial.printf("[i] GET %s?%s&sig=%s\n", config.ServerPath, qsBytes, encodedSigBytes);
  client.printf("GET %s?%s&sig=%s HTTP/1.1\r\n", config.ServerPath, qsBytes, encodedSigBytes);
  client.printf("Host: %s\r\n", config.ServerName);
  client.printf("User-Agent: %s\r\n", USER_AGENT);
  client.printf("X-MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  client.printf("X-HWID: %i\n", hwid);
  client.print("Connection: close\r\n\r\n");

  // Read response
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
    }
  }
  Serial.println("[i] Client disconnected.");
  return true;
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[i] Connecting to WiFi \"%s\"...\n", config.WifiSsid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("[i] IP=");
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
