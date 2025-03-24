// ROCKET CODE

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Adafruit_DPS310.h>
#include <LSM6DS3.h>
#include <Wire.h>

const char* launchpadSSID = "Pas de tir 🚀";
const char* launchpadPassword = "hippocampe";
const unsigned long wifiCheckInterval = 500; // ms 
const float seaLevelPressure = 1013.25; // hPa
const float g = 9.81; // m/s²

enum RocketState 
{
  NOT_CONNECTED,
  CONNECTED_AND_READY,
  ASCENDING,
  DESCENDING,
  RECONNECTING,
  TRANSMITTING_DATA
};

String launchpadIP; 
RocketState currentState = NOT_CONNECTED;
unsigned long lastWifiCheckTime = 0;
Adafruit_DPS310 baralt;
LSM6DS3 accgyr(I2C_MODE, 0x6A);
WebServer httpServer(80);

bool setupWifiConnection();
bool attemptWifiConnection();
void changeState(RocketState newState);
String stateToString(RocketState state);
void sendIdentificationRequest();
void setupAPIEndpoints();
void setupBarometer();
float getTemperature();
float getPressure();
float getAltitude();
float calculateAltitude(float pressure);
void setupAccGyr();
void scanI2CDevices();
void getAcceleration(float &accelX, float &accelY, float &accelZ);
void getGyroscope(float &gyroX, float &gyroY, float &gyroZ);


void setup() 
{
  Serial.begin(115200);

  if (!Wire.begin()) 
  {
    Serial.println("Échec de l'initialisation I2C");
    return;
  }

  Serial.println("Scanning I2C devices...");

  for (uint8_t address = 1; address < 127; address++) 
  {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(address, HEX);
    }
  }

  Serial.println("Initialisation I2C réussie");

  delay(1000);

  setupAccGyr();
  setupBarometer();

  changeState(NOT_CONNECTED);
  //setupAPIEndpoints();

  Serial.println("Initialisation terminée");
}

void scanI2CDevices() {
  Serial.println("Scan des périphériques I2C...");
  byte error, address;
  int nDevices = 0;
 
  for(address = 1; address < 127; address++) 
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Périphérique I2C trouvé à l'adresse 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  
  if (nDevices == 0) 
  {
    Serial.println("Aucun périphérique I2C trouvé\n");
  } else 
  {
    Serial.println("Scan terminé\n");
  }
}

void setupAccGyr()
{
  if (accgyr.begin() != 0) 
  {
    Serial.print("Échec de l'initialisation du LSM6DS.");
    while (1);
  }

  Serial.println("LSM6DS initialisé avec succès");

}


void setupBarometer()
{
  if (!baralt.begin_I2C())
  {
    Serial.println("Échec de l'initialisation du capteur de pression DPS310");
  }
  else
  {
    Serial.println("Capteur de pression DPS310 initialisé");
  }
  baralt.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
}

void loop() 
{
  // if (currentState == NOT_CONNECTED || currentState == RECONNECTING)
  // {
  //   if (millis() - lastWifiCheckTime > wifiCheckInterval) 
  //   { 
  //     lastWifiCheckTime = millis();
  //     bool success = attemptWifiConnection();
  //     if (success) 
  //     {
  //       if (currentState == NOT_CONNECTED) 
  //       {
  //         changeState(CONNECTED_AND_READY);
  //       } 
  //       else // if (currentState == RECONNECTING)
  //       {
  //         changeState(TRANSMITTING_DATA);
  //       }
  //     }
  //   }
  // }

  httpServer.handleClient();

  float accelX = 0, accelY = 0, accelZ = 0, gyroX = 0, gyroY = 0, gyroZ = 0;

  float altitude = getAltitude();
  float pressure = getPressure();
  float temperature = getTemperature();
  getAcceleration(accelX, accelY, accelZ);
  getGyroscope(gyroX, gyroY, gyroZ);

  Serial.println("Altitude actuelle : " + String(altitude) + "m");
  Serial.println("Pression actuelle : " + String(pressure) + " bars");
  Serial.println("Température actuelle : " + String(temperature) + "°C");
  Serial.println("Accélération X : " + String(accelX) + " m/s²");
  Serial.println("Accélération Y : " + String(accelY) + " m/s²");
  Serial.println("Accélération Z : " + String(accelZ) + " m/s²");
  Serial.println("Vitesse angulaire X : " + String(gyroX) + " rad/s");
  Serial.println("Vitesse angulaire Y : " + String(gyroY) + " rad/s");
  Serial.println("Vitesse angulaire Z : " + String(gyroZ) + " rad/s");

  delay(10);
}

void changeState(RocketState newState) 
{
  RocketState previousState = currentState;
  currentState = newState;

  if (newState == previousState) 
  {
    Serial.println("Pas de changement d'état : " + stateToString(newState));
    return;
  }

  Serial.println("Changement d'état : " + stateToString(previousState) + " -> " + stateToString(newState));

  if ((newState == CONNECTED_AND_READY && previousState != TRANSMITTING_DATA && previousState != NOT_CONNECTED)
    || (newState == ASCENDING && previousState != CONNECTED_AND_READY)
    || (newState == DESCENDING && previousState != ASCENDING)
    || (newState == RECONNECTING && previousState != DESCENDING)
    || (newState == TRANSMITTING_DATA && previousState != RECONNECTING))
  { 
    Serial.println("Erreur : impossible de passer à l'état " + stateToString(newState) + " depuis l'état " + stateToString(previousState) + ". Annulation de la séquence de lancement.");
    changeState(CONNECTED_AND_READY);
    return;
  }

  switch(newState) 
  {
    case NOT_CONNECTED:
      // Handled in loop()
      break;

    case CONNECTED_AND_READY:
      Serial.println("Connecté au réseau Wi-Fi du pas de tir. IP locale assignée : " + WiFi.localIP().toString());
      launchpadIP = WiFi.gatewayIP().toString();
      sendIdentificationRequest();
      break;

    case ASCENDING:
      break;

    case DESCENDING:
      break;

    case RECONNECTING:
      // Handled in loop()
      break;

    case TRANSMITTING_DATA:
      Serial.println("Connecté au réseau Wi-Fi du pas de tir. IP locale assignée : " + WiFi.localIP().toString());
      launchpadIP = WiFi.gatewayIP().toString();
      sendIdentificationRequest();
      break;
  }
}

String stateToString(RocketState state) 
{
  switch(state) 
  {
    case NOT_CONNECTED: return "NON CONNECTÉ";
    case CONNECTED_AND_READY: return "PRÊT";
    case ASCENDING: return "ASCENSION";
    case DESCENDING: return "DESCENTE";
    case RECONNECTING: return "RECONNEXION";
    case TRANSMITTING_DATA: return "TRANSMISSION DE DONNÉES";
    default: return "INCONNU";
  }
}

bool attemptWifiConnection()
{
  if (WiFi.status() == WL_CONNECTED) 
  {
    return true;
  } 
  else 
  {
    Serial.println("Reconnexion au réseau Wi-Fi du pas de tir...");
    WiFi.disconnect();
    WiFi.begin(launchpadSSID, launchpadPassword);
    return WiFi.status() == WL_CONNECTED;
  }
}

void sendIdentificationRequest()
{
  if (launchpadIP.length() == 0) {
    Serial.println("Erreur : IP du pas de tir non disponible");
    return;
  }

  HTTPClient http;
  String serverPath = "http://" + launchpadIP + "/api/identify-rocket";
  
  JsonDocument doc;
  doc["mac"] = WiFi.macAddress();
  doc["ip"] = WiFi.localIP().toString();
  
  String payload;
  serializeJson(doc, payload);
  
  http.begin(serverPath);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) 
  {
    Serial.print("Requête d'identification envoyée. Code de réponse : " + httpResponseCode);
  } else {
    Serial.println("Erreur lors de l'envoi de la requête d'identification. Erreur : " + http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void setupAPIEndpoints() {
    httpServer.on("/api/get-readiness", HTTP_GET, []() {
        JsonDocument doc;
        doc["ready"] = (currentState == CONNECTED_AND_READY);
        String response;
        serializeJson(doc, response);
        httpServer.send(200, "application/json", response);
    });
    httpServer.begin();
    Serial.println("Serveur HTTP initialisé");
}

float getPressure()
{
  sensors_event_t temp_event, pressure_event;
  baralt.getEvents(&temp_event, &pressure_event);
  return pressure_event.pressure / 1000.0; // Convert hPa to bars
}

float getTemperature()
{
  sensors_event_t temp_event, pressure_event;
  baralt.getEvents(&temp_event, &pressure_event);
  return temp_event.temperature;
}

float getAltitude() 
{
  return baralt.readAltitude(seaLevelPressure);
}

void getAcceleration(float& accelX, float& accelY, float& accelZ)
{
  accelX = accgyr.readFloatAccelX() * g;
  accelY = accgyr.readFloatAccelY() * g;
  accelZ = accgyr.readFloatAccelZ() * g;
}

void getGyroscope(float& gyroX, float& gyroY, float& gyroZ)
{
  gyroX = accgyr.readFloatGyroX();
  gyroY = accgyr.readFloatGyroY();
  gyroZ = accgyr.readFloatGyroZ();
  
}