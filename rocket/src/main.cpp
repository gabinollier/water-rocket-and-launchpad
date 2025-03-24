#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* launchpadSSID = "Pas de tir 🚀";
const char* launchpadPassword = "hippocampe";
const unsigned long wifiCheckInterval = 500; // ms 

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

bool setupWifiConnection();
bool attemptWifiConnection();
void changeState(RocketState newState);
String stateToString(RocketState state);
void sendIdentificationRequest();

void setup() 
{
  Serial.begin(115200);
  delay(1000);

  changeState(NOT_CONNECTED);
}

void loop() 
{
  if (currentState == NOT_CONNECTED || currentState == RECONNECTING)

    if (millis() - lastWifiCheckTime > wifiCheckInterval) 
    { 
        lastWifiCheckTime = millis();
        bool success = attemptWifiConnection();
        if (success) 
        {
            if (currentState == NOT_CONNECTED) 
            {
                changeState(CONNECTED_AND_READY);
            } 
            else // if (currentState == RECONNECTING)
            {
                changeState(TRANSMITTING_DATA);
            }
        }
    }

    delay(10);
}

void changeState(RocketState newState) 
{
  RocketState previousState = currentState;
  currentState = newState;

  if (newState == previousState) 
  {
    Serial.println("Pas de changement d'état : " + String(newState));
    return;
  }

  Serial.println("Changement d'état : " + String(previousState) + " -> " + String(newState));

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