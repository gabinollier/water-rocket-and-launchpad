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
const char* externalWifiSSID = "AAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const char* externalWifiPassword = "";
const char* dnsName = "launchpad.local"; // on peut mettre autre chose que .local si on veut
const byte DNS_PORT = 53;
const float MAX_PRESSURE = 10.0; // bars
const float LAUNCH_CLEARING_DELAY = 2; // seconds
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
    IDLING,
    WATER_FILLING,
    PRESSURIZING,
    READY_FOR_LAUNCH,
    LAUNCHING,
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
String launchpadStateToString(LaunchpadState state);
void updatePressureAndIncrementVolume();

// Setup functions
bool setupFileSystem();
bool setupWiFiAP();
bool setupConnectionToExternalWiFi();
bool setupLocalDNS();
bool setupHttpServer();
bool setupWebSocketServer();
void setupAPIEndpoints();
void setupFileEndpoints();

// API endpoint handlers
void handleAPIGetRocketState();
void handleAPIGetLogs();
void handleAPIGetRocketVolume();
void handleAPIGetAllFlightData();
void handleAPIStartFilling();
void handleAPILaunch();
void handleAPIAbort();
void handleAPINewRocketState();
void handleAPIUploadFlightData();
void handleAPIGetLaunchpadState();

// WebSocket senders
void sendWSNewLog(String timestamp, String message);
void sendWSNewLaunchpadState(String newLaunchpadState);
void sendWSNewRocketState(String newRocketState);
void sendWSFilling(float waterVolume, float pressure);

// Hardware sensors
float getPressure();
float getFlow();

// Hardware control functions
void openValve();
void closeValve();
void setPressureDistributorToAtmosphere();
void setPressureDistributorToCompressor();
void setPressureDistributorToLocked();
void closeLockSystem();
void openLockSystem();

// Communication with the rocket
bool isRocketConnected();
float getRocketVolume();
void tellRocketToStartWaitingForLaunch();
String getRocketState();

// Debug functions
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void println(String message);

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
    setupSuccess = setupSuccess && setupHttpServer();
    setupSuccess = setupSuccess && setupWebSocketServer();

    if (setupSuccess) 
    {
        println("Le pas de tir est prêt !");
        changeState(IDLING);
    } 
    else 
    {
        println("Initialisation terminée avec des erreurs. Le pas de tir peut ne pas fonctionner correctement.");
    }
}

void loop() 
{
    static unsigned long previousMillis = millis();

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
            changeState(READY_FOR_LAUNCH);
        }
    }  
    else if (currentLaunchpadState == LAUNCHING) 
    {
        if (millis() > launchTime + LAUNCH_CLEARING_DELAY * 1000) 
        { 
            changeState(IDLING);
        }
    }

    delay(20);
}

void changeState(LaunchpadState newState) 
{
    LaunchpadState previousState = currentLaunchpadState;
    currentLaunchpadState = newState;

    if (newState == previousState) 
    {
        println("Pas de changement d'état : " + launchpadStateToString(newState));
        return;
    }

    if (newState != IDLING)
    { 
        // This ensures that the sequence is followed in the correct order.
        if ((newState == NOT_INITIALIZED) 
            || (newState == WATER_FILLING && previousState != IDLING)
            || (newState == PRESSURIZING && previousState != WATER_FILLING)
            || (newState == READY_FOR_LAUNCH && previousState != PRESSURIZING)
            || (newState == LAUNCHING && previousState != READY_FOR_LAUNCH))
        {
            println("Erreur : impossible de passer à l'état " + launchpadStateToString(newState) + " depuis l'état " + launchpadStateToString(previousState) + ". Annulation de la séquence de lancement.");
            changeState(IDLING);
            return;
        }
    }

    println("Changement d'état : " + launchpadStateToString(previousState) + " -> " + launchpadStateToString(newState));
    sendWSNewLaunchpadState(launchpadStateToString(newState));

    switch(newState) 
    {
        case IDLING:
            closeLockSystem();
            closeValve();
            setPressureDistributorToAtmosphere();
            break;

        case WATER_FILLING:
            currentWaterVolume = 0;
            setPressureDistributorToAtmosphere();
            openValve();
            break;

        case PRESSURIZING:
            closeValve();
            setPressureDistributorToCompressor();
            break;

        case READY_FOR_LAUNCH:
            tellRocketToStartWaitingForLaunch();
            setPressureDistributorToLocked();
            break;

        case LAUNCHING:
            rocketIP = "";
            launchTime = millis();
            openLockSystem();
            break;
    }
}

String launchpadStateToString(LaunchpadState state) 
{
    switch(state) 
    {
        case NOT_INITIALIZED: return "NOT_INITIALIZED";
        case IDLING: return "IDLING";
        case WATER_FILLING: return "WATER_FILLING";
        case PRESSURIZING: return "PRESSURIZING";
        case READY_FOR_LAUNCH: return "READY_FOR_LAUNCH";
        case LAUNCHING: return "LAUNCHING";
        default: return "UNKNOWN";
    }
}

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

bool setupHttpServer() 
{
    setupAPIEndpoints();
    setupFileEndpoints();
    
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

void setupAPIEndpoints() 
{
    // Endpoint for the frontend
    httpServer.on("/api/get-logs", HTTP_GET, handleAPIGetLogs);
    httpServer.on("/api/get-rocket-volume", HTTP_GET, handleAPIGetRocketVolume);
    httpServer.on("/api/get-rocket-state", HTTP_GET, handleAPIGetRocketState);
    httpServer.on("/api/get-launchpad-state", HTTP_POST, handleAPIGetLaunchpadState);
    httpServer.on("/api/start-filling", HTTP_POST, handleAPIStartFilling);
    httpServer.on("/api/launch", HTTP_POST, handleAPILaunch);
    httpServer.on("/api/abort", HTTP_POST, handleAPIAbort);

    // Endpoints for the rocket
    httpServer.on("/api/new-rocket-state", HTTP_POST, handleAPINewRocketState);
}

void handleAPIGetRocketState()
{
    JsonDocument doc;
    doc["rocket-state"] = getRocketState();
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetLaunchpadState() 
{
    JsonDocument doc;
    doc["launchpad-state"] = launchpadStateToString(currentLaunchpadState);
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void setupFileEndpoints() 
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

    httpServer.serveStatic("/pages/", LittleFS, "/pages/");
    httpServer.serveStatic("/css/", LittleFS, "/css/");
    httpServer.serveStatic("/js/", LittleFS, "/js/");
}

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

void handleAPIGetAllFlightData()
{
    // TODO : Read all flight data from the SD card and send it to the client
}

void handleAPIGetRocketVolume() 
{
    JsonDocument doc;
    doc["totalVolume"] = getRocketVolume();
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIStartFilling() 
{
    if (currentLaunchpadState != IDLING) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "La fusée doit être verrouillée et en état IDLING";
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
        doc["message"] = "Valeur de pression invalide (min : 1 bar | max : " + String(MAX_PRESSURE) + " bars | demandé : " + String(targetPressure) + " bars)";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    changeState(WATER_FILLING);
    httpServer.send(200, "application/json");
}

void handleAPILaunch() 
{
    if (currentLaunchpadState != READY_FOR_LAUNCH) 
    {
        JsonDocument doc;
        doc["status"] = "error";
        doc["message"] = "Le pas de tir n'est pas prêt pour le lancement";
        String response;
        serializeJson(doc, response);
        httpServer.send(400, "application/json", response);
        return;
    }

    changeState(LAUNCHING);
    httpServer.send(200, "application/json");
}

void handleAPIAbort() 
{
    changeState(IDLING);
    httpServer.send(200, "application/json");
}

void handleAPINewRocketState() 
{
    String body = httpServer.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) 
    {
        httpServer.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
        return;
    }

    if (!doc.containsKey("rocket-state")) 
    {
        httpServer.send(400, "application/json", "{\"error\":\"Le champ rocket-state est requis\"}");
        return;
    }

    String newRocketState = doc["rocket-state"].as<String>();
    rocketIP = httpServer.client().remoteIP().toString();

    httpServer.send(200, "application/json");

    sendWSNewRocketState(newRocketState);
}

void handleAPIUploadFlightData() 
{
    // TODO : Write the flight data to the SD card and send ok to the client if successful
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) // Just for debugging
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

void sendWSNewLaunchpadState(String newLaunchpadState) 
{
    JsonDocument doc;
    doc["type"] = "new-launchpad-state";
    doc["launchpad-state"] = newLaunchpadState;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocketServer.broadcastTXT(jsonString);
}

void sendWSNewRocketState(String newRocketState) 
{
    JsonDocument doc;
    doc["type"] = "new-rocket-state";
    doc["rocket-state"] = newRocketState;
    
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

float getPressure() // TODO : Implement this function
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

void openValve()  // TODO : Implement this function
{
    println("Simulation : Pompe activée");
}

void closeValve() // TODO : Implement this function
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

bool isRocketConnected() 
{
    return rocketIP != "";
}

float getRocketVolume() 
{
    if (!isRocketConnected()) 
    {
        println("Aucune fusée connectée, annulation...");
        return 0.0;
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/get-rocket-volume";
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

void tellRocketToStartWaitingForLaunch()
{
    if (!isRocketConnected()) 
    {
        println("Erreur lors de laD demande de préparation de la fusée au lancement : aucune fusée connectée");
        return;
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
    }
    else
    {
        println("Erreur lors de laD demande de préparation de la fusée au lancement : " + String(httpResponseCode));
        httpClient.end();
    }
}

String getRocketState()
{
    if (!isRocketConnected()) 
    {
        return "DISCONNECTED";
    }

    HTTPClient httpClient;
    String serverPath = "http://" + rocketIP + "/api/get-rocket-state";
    httpClient.begin(serverPath);

    int httpResponseCode = httpClient.GET();
    
    if (httpResponseCode == 200) 
    {
        String payload = httpClient.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        String rocketState = doc["rocket-state"];
        httpClient.end();
        return rocketState;
    }
    
    httpClient.end();
    return "UNKNOWN";
}