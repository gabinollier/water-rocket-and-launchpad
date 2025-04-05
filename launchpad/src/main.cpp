// LAUNCHPAD CODE

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h> 
#include <DNSServer.h>
#include <WebSocketsServer.h>
#include <vector>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Constants

const char* wifiSSID = "Pas de tir 🚀";
const char* wifiPassword = "hippocampe";
const char* externalWifiSSID = "pc-gabin";
const char* externalWifiPassword = "12345678";
const char* dnsName = "launchpad.local"; // est-ce qu'on peut mettre autre chose que .local ?
const byte DNS_PORT = 53;
const float MAX_PRESSURE = 10.0; // bars
const float LAUNCH_CLEARING_DELAY = 3; // seconds
const int MAX_LOGS = 200;

// Data structures

struct LogEntry 
{
    uint32_t timestamp;
    String message;
};

enum LaunchpadState 
{
    NOT_INITIALIZED,
    IDLE,
    WATER_FILLING,
    PRESSURIZING,
    READY_FOR_LAUNCH,
    LAUNCHING,
    WAITING_FOR_ROCKET,
    VIEWING_DATA
};

// Global variables

WebServer httpServer(80);
WebSocketsServer webSocketServer = WebSocketsServer(81);
DNSServer dnsServer;
LaunchpadState currentLaunchpadState = NOT_INITIALIZED;
LogEntry logs[MAX_LOGS];
int logIndex = 0;
String rocketIP = "";
float currentWaterVolume = 0.0;
float targetWaterVolume = 0.0;
float currentPressure = 0.0;
float targetPressure = 0.0;
unsigned long launchTime = 0;
unsigned long deltaTime = 0;


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
void handleAPIGetFlightData();
void handleAPIStartFilling();
void handleAPILaunch();
void handleAPIReturnToIdle();
void handleAPIAbort();
void handleAPIIdentifyRocket();

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void onWifiEvent(WiFiEvent_t event);

void sendWSNewLog(String timestamp, String message);
void sendWSNewState(String newState);
void sendWSFilling(float waterVolume, float pressure);
void sendWSDataAvailable();

void println(String message);

bool checkRocketConnection();
float getDataTransferPercentage();
float getPressure();
float getFlow();
void enablePump();
void disablePump();
void setPressureDistributorToAtmosphere();
void setPressureDistributorToCompressor();
void setPressureDistributorToLocked();
void closeLockSystem();
void openLockSystem();

float getRocketVolume();
bool isRocketIdling();
bool tellRocketToStartWaitingForLaunch();
bool isRocketWaitingForLaunch();
bool isRocketConnected();

void setup() 
{
    Serial.begin(115200);
    delay(1000);

    println("\nInitialisation...");

    bool setupSuccess = true;
    setupSuccess = setupSuccess && setupFileSystem();
    setupSuccess = setupSuccess && setupWiFiAP();
    setupConnectionToExternalWiFi();
    setupSuccess = setupSuccess && setupLocalDNS();
    setupSuccess = setupSuccess && setupWebServer();
    setupSuccess = setupSuccess && setupWebSocketServer();
    if (setupSuccess) 
    {
        println("Le pas de tir est prêt !");
        changeState(IDLE);
    } 
    else 
    {
        println("Initialisation terminée avec des erreurs. Le pas de tir peut ne pas fonctionner correctement.");
    }
}

void loop() 
{
    static unsigned long previousMillis = millis();
    static unsigned long lastWifiCheck = 0;

    deltaTime = millis() - previousMillis;
    previousMillis = millis();

    dnsServer.processNextRequest();
    httpServer.handleClient();
    webSocketServer.loop();

    currentPressure = getPressure();
    float flow = getFlow();
    currentWaterVolume += flow * (deltaTime / 1000.0);

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
            if (tellRocketToStartWaitingForLaunch())
            {
                changeState(READY_FOR_LAUNCH);
            }
            else 
            {
                changeState(IDLE);
            }
        }
    }  
    else if (currentLaunchpadState == LAUNCHING) 
    {
        if (millis() > launchTime + LAUNCH_CLEARING_DELAY * 1000 ) 
        { 
            changeState(WAITING_FOR_ROCKET);
        }
    }
    else if (currentLaunchpadState == WAITING_FOR_ROCKET)
    {
        if (isRocketConnected()) 
        {
            changeState(VIEWING_DATA);
        }
    }

    delay(10);
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
        if ((newState == NOT_INITIALIZED) 
            || (newState == WATER_FILLING && previousState != IDLE)
            || (newState == PRESSURIZING && previousState != WATER_FILLING)
            || (newState == READY_FOR_LAUNCH && previousState != PRESSURIZING)
            || (newState == LAUNCHING && previousState != READY_FOR_LAUNCH)
            || (newState == WAITING_FOR_ROCKET && previousState != LAUNCHING)
            || (newState == VIEWING_DATA && previousState != WAITING_FOR_ROCKET))
        {
            println("Erreur : impossible de passer à l'état " + stateToString(newState) + " depuis l'état " + stateToString(previousState) + ". Annulation de la séquence de lancement.");
            changeState(IDLE);
            return;
        }
    }

    println("Changement d'état : " + stateToString(previousState) + " -> " + stateToString(newState));
    sendWSNewState(stateToString(newState));

    switch(newState) 
    {
        case IDLE:
            if (previousState != VIEWING_DATA && previousState != NOT_INITIALIZED)
            {
                println("Séquence de lancement annulée au stade : " + stateToString(previousState));
            }
            closeLockSystem();
            disablePump();
            setPressureDistributorToAtmosphere();
            break;

        case WATER_FILLING:
            currentWaterVolume = 0;
            setPressureDistributorToAtmosphere();
            enablePump();
            break;

        case PRESSURIZING:
            disablePump();
            setPressureDistributorToCompressor();
            break;

        case READY_FOR_LAUNCH:
            setPressureDistributorToLocked();
            break;

        case LAUNCHING:
            rocketIP = ""; 
            launchTime = millis();
            openLockSystem();
            break;

        case WAITING_FOR_ROCKET:
            setPressureDistributorToAtmosphere();
            closeLockSystem();
            break;

        case VIEWING_DATA:
            sendWSDataAvailable();
            break;
    }

    
}

String stateToString(LaunchpadState state) 
{
    switch(state) 
    {
        case NOT_INITIALIZED: return "NOT_INITIALIZED";
        case IDLE: return "IDLE";
        case WATER_FILLING: return "WATER_FILLING";
        case PRESSURIZING: return "PRESSURIZING";
        case READY_FOR_LAUNCH: return "READY_FOR_LAUNCH";
        case LAUNCHING: return "LAUNCHING";
        case WAITING_FOR_ROCKET: return "WAITING_FOR_ROCKET";
        case VIEWING_DATA: return "VIEWING_DATA";
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
    WiFi.onEvent(onWifiEvent);

    println("Point d'accès WiFi initialisé. IP locale : " + WiFi.softAPIP().toString());
    return true;
}

bool setupConnectionToExternalWiFi() 
{
    if (externalWifiSSID == NULL || externalWifiPassword == NULL) 
    {
        println("Aucun réseau Wi-Fi externe configuré. Fonctionnement en mode AP uniquement.");
        return false;
    }

    // DEBUG : Scan for available networks :

    Serial.println("Scan des réseaux Wi-Fi disponibles...");
    int n = WiFi.scanNetworks();
    if (n == 0) 
    {
        Serial.println("Aucun réseau Wi-Fi trouvé.");
        return false;
    } 
    else 
    {
        Serial.println("Réseaux Wi-Fi trouvés : " + String(n));
        for (int i = 0; i < n; i++) 
        {
            Serial.println("- " + WiFi.SSID(i) + " (" + (100+WiFi.RSSI(i)) + "%)");
        }
    }

    WiFi.begin(externalWifiSSID, externalWifiPassword);

    int attemptCount = 0;
    while (WiFi.status() != WL_CONNECTED && attemptCount < 20) 
    {
        delay(500);
        attemptCount++;
    }

    if (WiFi.status() == WL_CONNECTED) 
    {
        println("Connecté au réseau Wi-Fi externe. IP locale assignée : " + WiFi.localIP().toString());
        return true;
    } 
    else 
    {
        println("Échec de connexion au réseau Wi-Fi externe. Fonctionnement en mode AP uniquement.");
        return false; 
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
    httpServer.on("/api/get-rocket-volume", HTTP_GET, handleAPIGetTotalVolume);

    httpServer.on("/api/identify-rocket", HTTP_POST, handleAPIIdentifyRocket);
    httpServer.on("/api/start-filling", HTTP_POST, handleAPIStartFilling);
    httpServer.on("/api/launch", HTTP_POST, handleAPILaunch);
    httpServer.on("/api/return-to-idle", HTTP_POST, handleAPIReturnToIdle);
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

/// GET

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
    doc["totalVolume"] = getRocketVolume();
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetFlightData()
{
    // get the flight data using an http request to the rocket
    if (!isRocketConnected()) 
    {
        JsonDocument doc;
        doc["error"] = "Aucune fusée connectée";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/get-flight-data";
    httpClient.begin(serverPath);
    int httpResponseCode = httpClient.GET();

    if (httpResponseCode == 200) 
    {
        String payload = httpClient.getString();
        httpServer.send(200, "application/json", payload);
        httpClient.end();
    } 
    else 
    {
        println("Erreur lors de la récupération des données de vol : " + String(httpResponseCode));
        httpClient.end();
    }
}

/// POST

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
    
    if (!httpServer.hasArg("water-volume") || !httpServer.hasArg("pressure")) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "Les paramètres de volume et de pression sont requis";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    if (!isRocketIdling()) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "La fusée n'est pas connectée ou n'est pas prête";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    targetWaterVolume = httpServer.arg("water-volume").toFloat();
    targetPressure = httpServer.arg("pressure").toFloat();
    float maxVolume = getRocketVolume();

    if (targetWaterVolume <= 0 || targetWaterVolume > maxVolume) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "Volume d'eau invalide (min : 0L | max : " + String(maxVolume) + "L | demandé : " + String(targetWaterVolume) + "L)";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

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

void handleAPILaunch() 
{
    if (currentLaunchpadState != READY_FOR_LAUNCH) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "Le pas de tir n'est pas prête pour le lancement";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    if (!isRocketWaitingForLaunch()) 
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

void handleAPIReturnToIdle()
{
    if (isRocketConnected()) 
    {
        HTTPClient httpClient;
        String serverPath = "http://" + rocketIP + "/api/return-to-idle";
        httpClient.begin(serverPath);
        httpClient.addHeader("Content-Type", "application/json");
        int httpResponseCode = httpClient.POST("{}");
        if (httpResponseCode != 200) 
        {
            println("Erreur lors de la demande de retour à l'état IDLE de la fusée : " + String(httpResponseCode));
        }
        httpClient.end();
    }

    changeState(IDLE);

    JsonDocument doc;
    doc["status"] = "success";
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
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

void handleAPIIdentifyRocket() 
{
    String body = httpServer.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) 
    {
        httpServer.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
        return;
    }

    if (!doc.containsKey("ip")) 
    {
        httpServer.send(400, "application/json", "{\"error\":\"Le champ ip est requis\"}");
        return;
    }

    JsonDocument response;
    response["success"] = false;
    String responseStr;
    serializeJson(response, responseStr);
    httpServer.send(200, "application/json", responseStr);

    rocketIP = doc["ip"].as<String>();
    println("Fusée identifiée - IP: " + rocketIP);
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

void onWifiEvent(WiFiEvent_t event)
{
    switch(event) 
    {
        case SYSTEM_EVENT_AP_STACONNECTED:
            println("Nouvel appareil connecté au Wi-Fi.");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            println("Appareil déconnecté du Wi-Fi.");
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
    JsonDocument doc;
    doc["type"] = "filling";
    doc["water-volume"] = waterVolume;
    doc["pressure"] = pressure;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

void sendWSDataAvailable() 
{
    JsonDocument doc;
    doc["type"] = "data-available";
    
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

// Hardware-related functions implementation - SIMULATED VERSION

bool checkRocketConnection() // TODO : Implement this function
{
    delay(4000);
    return true;
}

float SIMULATION_percentage = 0.0;
float getDataTransferPercentage() // TODO : Implement this function
{ 
    SIMULATION_percentage += 0.3 * (deltaTime / 1000.0);
    if (SIMULATION_percentage >= 1.0) {
        SIMULATION_percentage = 1.0;
    }
    return SIMULATION_percentage;
}

float getPressure()
{
    
    if (currentLaunchpadState == PRESSURIZING) {
        currentPressure += 0.3 * (deltaTime / 1000.0) * targetPressure;
        println("Simulation : Pressurisation en cours : " + String(currentPressure) + " bars");
    }

    return currentPressure;
    
}

float getFlow() // TODO : Implement this function
{
    if (currentLaunchpadState == WATER_FILLING) {
        println("Simulation : Remplissage en cours : " + String(currentWaterVolume) + "L");
        return 0.15; // L/s
    }
    return 0.0;
}

void enablePump()  // TODO : Implement this function
{
    println("Simulation : Pompe activée");
}

void disablePump() // TODO : Implement this function
{
    println("Simulation : Pompe désactivée");
}

void setPressureDistributorToAtmosphere() // TODO : Implement this function
{
    println("Simulation : Distributeur de pression mis à l'atmosphère");
    currentPressure = 1.0;
}

void setPressureDistributorToCompressor() // TODO : Implement this function
{
    println("Simulation : Distributeur de pression mis au compresseur");
}

void setPressureDistributorToLocked() // TODO : Implement this function
{
    println("Simulation : Distributeur de pression verrouillé");
}

void closeLockSystem() // TODO : Implement this function
{
    println("Simulation : Système de verrouillage fermé");
}

void openLockSystem() // TODO : Implement this function
{
    println("Simulation : Système de verrouillage ouvert");
}

float getRocketVolume() 
{
    if (!isRocketConnected()) {
        return 0.0;
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/get-volume";
    httpClient.begin(serverPath);

    int httpResponseCode = httpClient.GET();
    
    if (httpResponseCode == 200) {
        String payload = httpClient.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        float volume = doc["volume"];
        httpClient.end();
        return volume;
    }
    else
    {
        println("Erreur lors de la récupération du volume de la fusée : " + String(httpResponseCode));
        httpClient.end();
        return 0.0;
    }

}

bool isRocketIdling() {
    if (!isRocketConnected()) {
        return false;
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/is-idling";
    httpClient.begin(serverPath);
    
    int httpResponseCode = httpClient.GET();
    
    if (httpResponseCode == 200) {
        String payload = httpClient.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        bool ready = doc["is-idling"];
        httpClient.end();
        return ready;
    }
    
    httpClient.end();
    return false;
}

bool tellRocketToStartWaitingForLaunch()
{
    if (!isRocketConnected()) 
    {
        println("Aucune fusée connectée, annulation...");
        return false;
    }

    // Send a post request to rocketIP/api/get-ready-for-launch
    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/start-waiting-for-launch";
    httpClient.begin(serverPath);

    httpClient.addHeader("Content-Type", "application/json");
    int httpResponseCode = httpClient.POST("{}");

    if (httpResponseCode == 200) {
        String payload = httpClient.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        return true;
    }
    else
    {
        println("Erreur lors de la demande de préparation de la fusée au lancement : " + String(httpResponseCode));
        httpClient.end();
        return false;
    }
}

bool isRocketWaitingForLaunch()
{
    if (!isRocketConnected()) {
        return false;
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/is-waiting-for-launch";
    httpClient.begin(serverPath);

    int httpResponseCode = httpClient.GET();
    
    if (httpResponseCode == 200) {
        String payload = httpClient.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        bool ready = doc["is-waiting"];
        httpClient.end();
        return ready;
    }
    
    httpClient.end();
    return false;
}

bool isRocketConnected()
{
    return rocketIP.length() > 0;
}