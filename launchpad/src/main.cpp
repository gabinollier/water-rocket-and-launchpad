#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h> 
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <vector>
#include <ArduinoJson.h>

// Constants

const char* wifiSSID = "hippocampe2";
const char* wifiPassword = "hippocampe";
const char* externalWifiSSID = "AAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const char* externalWifiPassword = "badadabadaoui";
const char* dnsName = "rocket.local";
const byte DNS_PORT = 53;
const float MAX_PRESSURE = 10.0; // bars
const float LAUNCH_CLEARING_DELAY = 3; // seconds
const int MAX_LOGS = 100;

// Data structures

struct FlightDataEntry 
{
    uint32_t timestamp;
    float altitude;
    float acceleration;
    float speed;
    float pressure;
    float temperature;
};

struct LogEntry 
{
    uint32_t timestamp;
    String message;
};

enum LaunchpadState 
{
    UNINITIALIZED,
    IDLE,
    WATER_FILLING,
    PRESSURIZING,
    READY_FOR_LAUNCH,
    LAUNCHING,
    WAITING_FOR_ROCKET,
    RECEIVING_DATA,
    RECEIVED_DATA
};

// Global variables

WebServer httpServer(80);
WebSocketsServer webSocketServer = WebSocketsServer(81);
DNSServer dnsServer;
LaunchpadState currentLaunchpadState = UNINITIALIZED;
float targetWaterVolume = 0.0;   // in liters
float currentWaterVolume = 0.0;  // in liters
float targetPressure = 0.0;      // in bars
float currentPressure = 0.0;     // in bars
int dataTransferPercentage = 0;
uint32_t launchTime = 0;
LogEntry logs[MAX_LOGS];
int logIndex = 0;
std::vector<FlightDataEntry> flightData;

// Function declarations

void changeState(LaunchpadState newState);
String stateToString(LaunchpadState state);
void updatePressureAndIncrementVolume();

bool setupFileSystem();
bool setupWiFiAP();
bool setupConnectionToExternalWiFi();
bool setupLocalDNS();
bool setupWebServer();
bool setupWebSocketServer();

void setupAPIEndpoints();
void setupFileServingEndpoints();
void setupHttpEndpoints();

void handleAPIGetLogs();
void handleAPIGetTotalVolume();
void handleAPIStartFilling();
void handleAPIAbort();
void handleAPILaunch();
void handleAPIGetRocketReadiness();
void handleAPIGetRocketData();
void handleAPIGetLaunchpadState();

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

void sendWSNewLog(String timestamp, String message);
void sendWSNewState(String newState);
void sendWSFilling(float waterVolume, float pressure);
void sendWSReceivingData(float percentage);

void println(String message);

void setup() 
{

    Serial.begin(115200);
    delay(1000);
    println("\nInitialisation...");

    bool setupSuccess = true;
    
    setupSuccess &= setupFileSystem();
    setupSuccess &= setupWiFiAP();
    
    setupConnectionToExternalWiFi();
    
    setupSuccess &= setupLocalDNS();
    setupSuccess &= setupWebServer();
    setupSuccess &= setupWebSocketServer();
    
    if (setupSuccess) 
    {
        println("Le pas de tir est prêt !");
    } 
    else 
    {
        println("Initialisation terminée avec des erreurs. Le pas de tir peut ne pas fonctionner correctement.");
    }
    
    changeState(IDLE);
}

void loop() 

{
    dnsServer.processNextRequest();
    httpServer.handleClient();
    webSocketServer.loop();

    // Check external Wi-Fi connection every 30 seconds
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 30000) 
    { 
        lastWifiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) 
        {
            println("Reconnexion au réseau Wi-Fi " + String(externalWifiSSID) + "...");
            WiFi.disconnect();
            WiFi.begin(externalWifiSSID, externalWifiPassword);
        }
    }

    // Launch sequence tracking

    updatePressureAndIncrementVolume();

    if (currentLaunchpadState == WATER_FILLING) 
    {
        if (currentWaterVolume < targetWaterVolume) 
        {
            sendWSFilling(currentWaterVolume, currentPressure);
        } 
        else 
        {
            changeState(PRESSURIZING);
        }
    } 
    else if (currentLaunchpadState == PRESSURIZING) 
    {
        if (currentPressure < targetPressure) 
        {
            sendWSFilling(currentWaterVolume, currentPressure);
        } 
        else 
        {
            changeState(READY_FOR_LAUNCH);
        }
    }  
    else if (currentLaunchpadState == LAUNCHING) 
    {
        if (millis() - launchTime > LAUNCH_CLEARING_DELAY * 1000) 
        { 
            changeState(WAITING_FOR_ROCKET);
        }
    }
    else if (currentLaunchpadState == WAITING_FOR_ROCKET) 
    {
        bool isRocketConnected = true; // TODO : Check for connection status
        if (isRocketConnected) 
        {
            changeState(RECEIVING_DATA);
        }
    } 
    else if (currentLaunchpadState == RECEIVING_DATA) 
    {
        float dataTransferPercentage = 1; // TODO : Get real data transfer percentage
        sendWSReceivingData(dataTransferPercentage);
        if (dataTransferPercentage >= 1) 
        {
            changeState(RECEIVED_DATA);
        }
    }

    delay(10);
}

void updatePressureAndIncrementVolume() 
{
    // TODO : Actually request the flowmeter and pressure sensor for current values

    // SIMULATION :
    if (currentLaunchpadState == IDLE) 
    {
        currentPressure = 1.0;
    }
    else if (currentLaunchpadState == WATER_FILLING) 
    {
        currentWaterVolume += 0.001; 
    }
    else if (currentLaunchpadState == PRESSURIZING) 
    {
        currentPressure += 0.001; 
    }
}

void changeState(LaunchpadState newState) 
{
    LaunchpadState previousState = currentLaunchpadState;
    currentLaunchpadState = newState;

    if (newState == previousState) 
    {
        println("Pas de changement d'état : " + stateToString(newState));
        return;
    }

    if (newState != IDLE)
    { 
        // This ensures that the sequence is followed in the correct order.
        if ((newState == UNINITIALIZED) 
            || (newState == WATER_FILLING && previousState != IDLE)
            || (newState == PRESSURIZING && previousState != WATER_FILLING)
            || (newState == READY_FOR_LAUNCH && previousState != PRESSURIZING)
            || (newState == LAUNCHING && previousState != READY_FOR_LAUNCH)
            || (newState == WAITING_FOR_ROCKET && previousState != LAUNCHING)
            || (newState == RECEIVING_DATA && previousState != WAITING_FOR_ROCKET)
            || (newState == RECEIVED_DATA && previousState != RECEIVING_DATA))
        {
            println("Erreur : impossible de passer à l'état " + stateToString(newState) + " depuis l'état " + stateToString(previousState) + ". Annulation de la séquence de lancement.");
            changeState(IDLE);
            return;
        }
    }


    switch(newState) 
    {
        case IDLE:
            if (previousState != RECEIVED_DATA && previousState != UNINITIALIZED)
            {
                println("Séquence de lancement annulée au stade : " + stateToString(previousState));
            }
            // TODO : Stop pump, set pressure distributor to atmosphere, lock the lock system
            break;

        case WATER_FILLING:
            currentWaterVolume = 0;
            // TODO : Set pressure distributor to atmosphere, start pump
            break;

        case PRESSURIZING:
            // TODO : Stop pump, set pressure distributor to compressor
            break;

        case READY_FOR_LAUNCH:
            // TODO : Set pressure distributor to locked
            break;

        case LAUNCHING:
            launchTime = millis();
            // TODO : Transfer start time, pressure and water volume to the rocket
            // TODO : Release rocket
            break;

        case WAITING_FOR_ROCKET:
            // TODO : Set pressure distributor to atmosphere and lock the lock system
            break;

        case RECEIVING_DATA:
            break;

        case RECEIVED_DATA:
            break;
    }

    println("Changement d'état : " + stateToString(previousState) + " -> " + stateToString(newState));
    sendWSNewState(stateToString(newState));
}

String stateToString(LaunchpadState state) 
{
    switch(state) 
    {
        case UNINITIALIZED: return "UNINITIALIZED";
        case IDLE: return "IDLE";
        case WATER_FILLING: return "WATER_FILLING";
        case PRESSURIZING: return "PRESSURIZING";
        case READY_FOR_LAUNCH: return "READY_FOR_LAUNCH";
        case LAUNCHING: return "LAUNCHING";
        case WAITING_FOR_ROCKET: return "WAITING_FOR_ROCKET";
        case RECEIVING_DATA: return "RECEIVING_DATA";
        case RECEIVED_DATA: return "RECEIVED_DATA";
        default: return "UNKNOWN";
    }
}

// Initialization functions implementation

bool setupFileSystem() 
{
    if (!LittleFS.begin(true)) 
    {
        println("L'initialisation et le formatage de LittleFS ont échoué");
        return false;
    }

    println("LittleFS initialisé avec succès.");
    return true;
}

bool setupWiFiAP() 
{
    WiFi.mode(WIFI_AP_STA); 
    WiFi.softAP(wifiSSID, wifiPassword);

    println("Point d'accès WiFi initialisé. IP locale : " + WiFi.softAPIP().toString());
    return true;
}

bool setupConnectionToExternalWiFi() 
{
    WiFi.begin(externalWifiSSID, externalWifiPassword);

    int attemptCount = 0;
    while (WiFi.status() != WL_CONNECTED && attemptCount < 20) 
    {  // Attente max de 10 secondes
        delay(500);
        attemptCount++;
    }

    if (WiFi.status() == WL_CONNECTED) 
    {
        println("Connecté au réseau Wi-Fi externe. IP assignée : " + WiFi.localIP().toString());
        return true;
    } 
    else 
    {
        println("Échec de connexion au réseau Wi-Fi externe. Fonctionnement en mode AP uniquement.");
        return false; // Not critical, so we still proceed
    }
}

bool setupLocalDNS() 
{
    dnsServer.start(DNS_PORT, dnsName, WiFi.softAPIP());
    println("Serveur DNS local initialisé pour http://" + String(dnsName));
    return true;
}

bool setupWebServer() 
{
    setupAPIEndpoints();
    setupHttpEndpoints();
    setupFileServingEndpoints();
    
    httpServer.begin();
    println("Serveur HTTP initialisé.");
    return true;
}

bool setupWebSocketServer() 
{
    webSocketServer.begin();
    webSocketServer.onEvent(onWebSocketEvent);
    println("Serveur WebSocket initialisé.");
    return true;
}

// API and routing functions implementation

void setupAPIEndpoints() 
{
    httpServer.on("/api/get-logs", HTTP_GET, handleAPIGetLogs);
    httpServer.on("/api/get-rocket-readiness", HTTP_GET, handleAPIGetRocketReadiness);
    httpServer.on("/api/get-rocket-volume", HTTP_GET, handleAPIGetTotalVolume);
    httpServer.on("/api/get-rocket-data", HTTP_GET, handleAPIGetRocketData);
    httpServer.on("/api/get-launchpad-state", HTTP_GET, handleAPIGetLaunchpadState);

    httpServer.on("/api/start-filling", HTTP_POST, handleAPIStartFilling);
    httpServer.on("/api/launch", HTTP_POST, handleAPILaunch);
    httpServer.on("/api/abort", HTTP_POST, handleAPIAbort);
}

void setupFileServingEndpoints() 
{
    httpServer.serveStatic("/pages/", LittleFS, "/pages/");
    httpServer.serveStatic("/css/", LittleFS, "/css/");
    httpServer.serveStatic("/js/", LittleFS, "/js/");
}

void setupHttpEndpoints() 
{
    const char* routesToIndex[] = {"", "/", "/launch", "/flight-data", "/debug"}; // URIs that serve index.html

    for (const char* route : routesToIndex) 
    {
        httpServer.serveStatic(route, LittleFS, "/index.html");
    }

    httpServer.onNotFound([]() 
    {
        println("404: Le client a essayé d'accéder à " + httpServer.uri() + " mais cela n'existe pas.");
        File file = LittleFS.open("/index.html", "r");
        httpServer.streamFile(file, "text/html; charset=UTF-8");
        file.close();
    });
}

// API handlers implementation

void handleAPIGetLogs() 
{
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (int i = 0; i < logIndex; i++) 
    {
        JsonObject entry = array.createNestedObject();
        entry["timestamp"] = logs[i].timestamp;
        entry["message"] = logs[i].message;
    }
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetTotalVolume() 
{
    JsonDocument doc;
    doc["totalVolume"] = 1.5;  // TODO : Request the rocket for the real total volume of water
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIStartFilling() 
{
    if (currentLaunchpadState != IDLE) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "La fusée doit être verrouillée et en état IDLE";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }
    
    if (httpServer.hasArg("water-volume") && httpServer.hasArg("pressure")) 
    {
        targetWaterVolume = httpServer.arg("volume").toFloat();
        float maxVolume = 1.5; // TODO : Request the rocket for max volume
        if (targetWaterVolume <= 0 || targetWaterVolume > maxVolume) 
        {
            JsonDocument doc;
            doc["status"] = "error";
            doc["message"] = "Volume d'eau invalide";
            String response;
            serializeJson(doc, response);
            httpServer.send(400, "application/json", response);
            return;
        }

        targetPressure = httpServer.arg("pressure").toFloat();
        if (targetPressure <= 1 || targetPressure > MAX_PRESSURE) 
        { 
            JsonDocument doc;
            doc["status"] = "error";
            doc["message"] = "Valeur de pression invalide";
            String response;
            serializeJson(doc, response);
            httpServer.send(400, "application/json", response);
            return;
        }

        changeState(WATER_FILLING);
        JsonDocument doc;
        doc["status"] = "success";
        String response;
        serializeJson(doc, response);
        httpServer.send(200, "application/json", response);
    } 
    else 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "Les paramètres de volume et de pression sont requis";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
    }
}

void handleAPIAbort() 
{
    changeState(IDLE);
    JsonDocument doc;
    doc["status"] = "success";
    doc["message"] = "Séquence de lancement annulée";
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPILaunch() 
{
    if (currentLaunchpadState != READY_FOR_LAUNCH) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "La fusée n'est pas prête pour le lancement";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    changeState(LAUNCHING);
    JsonDocument doc;
    doc["status"] = "success";
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetRocketReadiness() 
{
    bool isReady = true; // TODO : Request the rocket for readiness 
    JsonDocument doc;
    doc["isReady"] = isReady;
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetRocketData() 
{
    if (flightData.empty()) 
    {
        JsonDocument doc;
        doc["error"] = "Aucune donnée de vol disponible";
        String response;
        serializeJson(doc, response);
        httpServer.send(200, "application/json", response);
        return;
    }

    String csvData = "timestamp,altitude,acceleration,speed,pressure,temperature\n";
    for (const auto& entry : flightData) 
    {
        csvData += String(entry.timestamp) + "," +
                  String(entry.altitude) + "," +
                  String(entry.acceleration) + "," +
                  String(entry.speed) + "," +
                  String(entry.pressure) + "," +
                  String(entry.temperature) + "\n";
    }

    httpServer.send(200, "text/csv", csvData);
}

void handleAPIGetLaunchpadState() 
{
    JsonDocument doc;
    doc["state"] = stateToString(currentLaunchpadState);
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

// WebSocket functions implementation

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) 
{
    switch(type) 
    {
        case WStype_DISCONNECTED:
            println("WS : Client #" + String(num) + " déconnecté du WebSocket");
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocketServer.remoteIP(num);
                println("WS : Client #" + String(num) + " connecté depuis " + ip.toString());
            }
            break;
        case WStype_TEXT:
            println("WS : Message WebSocket reçu du client #" + String(num));
            break;
        default:
            break;
    }
}

void sendWSNewLog(String timestamp, String message) 
{
    JsonDocument doc;
    doc["type"] = "new-log";
    doc["timestamp"] = timestamp;
    doc["message"] = message;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

void sendWSNewState(String newState) 
{
    JsonDocument doc;
    doc["type"] = "new-state";
    doc["state"] = newState;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

void sendWSFilling(float waterVolume, float pressure) 
{
    // debug :
    // println("Envoi de données de remplissage : " + String(waterVolume) + "L, " + String(pressure) + " bars");

    JsonDocument doc;
    doc["type"] = "filling";
    doc["water-volume"] = waterVolume;
    doc["pressure"] = pressure;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

void sendWSReceivingData(float percentage) 
{
    JsonDocument doc;
    doc["type"] = "receiving-data";
    doc["percentage"] = percentage;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

// Logging functions implementation

void println(String message) 
{
    Serial.println(message);

    message.replace("\\", "\\\\"); 
    message.replace("\n", "\\n"); 
    message.replace("\r", "\\r");  
    message.replace("\t", "\\t");  
    message.replace("\"", "\\\""); 

    logs[logIndex] = { millis(), message };
    logIndex = (logIndex + 1) % MAX_LOGS;

    sendWSNewLog(String(millis()), message);
}