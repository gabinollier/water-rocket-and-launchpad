#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h> // File System
//#include <FS.h>   
#include <DNSServer.h>
#include <WebSocketsServer.h>

const char* wifiSSID = "hippocampe2";
const char* wifiPassword = "hippocampe";
const char* externalWifiSSID = "AAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const char* externalWifiPassword = "badadabadaoui";

const char* dnsName = "rocket.local";
const byte DNS_PORT = 53;
WebServer httpServer(80);
WebSocketsServer webSocketServer = WebSocketsServer(81);
DNSServer dnsServer;
#define MAX_LOGS 100
int logIndex = 0;

void println(String message);
void handleLEDEndpoint();
void createAPIEndpoints();
void createRooting();
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
String getJSONFormattedLogs();

struct LogEntry {
    uint32_t timestamp;
    String message;
};

LogEntry logs[MAX_LOGS];

void setup() {
    Serial.begin(115200);

    delay(2000);

    // BULIT-IN LED

    // #define LED_BUILTIN 2
    // pinMode(LED_BUILTIN, OUTPUT);
    // digitalWrite(LED_BUILTIN, HIGH);

    // LittleFS

    println("\nInitialisation...");

    if (!LittleFS.begin(true)) {
        println("LittleFS initialization and formatting both failed");
        return;
      }

    println("LittleFS initialisé avec succès.");

    // WIFI AP

    WiFi.mode(WIFI_AP_STA); 
    WiFi.softAP(wifiSSID, wifiPassword);
    println("Point d'accès WiFi initialisé. IP locale : " + WiFi.softAPIP().toString());

    // WIFI CONNECTION 

    WiFi.begin(externalWifiSSID, externalWifiPassword);

    int attemptCount = 0;
    while (WiFi.status() != WL_CONNECTED && attemptCount < 20) {  // Attente max de 10 secondes
        delay(500);
        attemptCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        println("Connecté au réseau Wi-Fi externe. IP assignée : " + WiFi.localIP().toString());
    } else {
        println("Échec de connexion au réseau Wi-Fi externe. Fonctionnement en mode AP uniquement.");
    }

    // mDNS

    dnsServer.start(DNS_PORT, dnsName, WiFi.softAPIP());
    println("Serveur DNS local initialisé pour http://" + String(dnsName));

    // SERVEUR WEB

    createAPIEndpoints();
    createRooting();
    serveFiles();

    httpServer.begin();

    println("Serveur HTTP initialisé.");

    // SERVEUR WEBSOCKET

    webSocketServer.begin();
    webSocketServer.onEvent(onWebSocketEvent);

    println("Serveur WebSocket initialisé.");

    println("Le pas de tir est prêt !");
}



void loop() {
    dnsServer.processNextRequest();
    httpServer.handleClient();
    webSocketServer.loop();

    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 30000) {  // Vérification toutes les 30 secondes
        lastWifiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) {
            println("Reconnexion au réseau Wi-Fi " + String(externalWifiSSID) + "...");
            WiFi.disconnect();
            WiFi.begin(externalWifiSSID, externalWifiPassword);
        }
    }

    delay(10);
}

void createAPIEndpoints() {
    httpServer.on("/api/led", handleLEDEndpoint);
    httpServer.on("/api/getlogs", []() { httpServer.send(200, "application/json", getJSONFormattedLogs()); });
}

void serveFiles()
{
    httpServer.serveStatic("/pages/", LittleFS, "/pages/");
    httpServer.serveStatic("/css/", LittleFS, "/css/");
    httpServer.serveStatic("/js/", LittleFS, "/js/");
}

void createRooting()
{
    const char* routesToIndex[] = {"", "/", "/launch", "/flight-data", "/debug"}; // URIs that serve index.html

    for (const char* route : routesToIndex)
    {
        httpServer.serveStatic(route, LittleFS, "/index.html");
    }

    httpServer.onNotFound([]() {
        println("404: Client tried to access " + httpServer.uri() + " but it does not exists.");
        File file = LittleFS.open("/index.html", "r");
        httpServer.streamFile(file, "text/html; charset=UTF-8");
        file.close(); });
}



String getJSONFormattedLogs()
{
    String json = "[";
    for (int i = 0; i < logIndex; i++) {
        json += "{\"timestamp\":" + String(logs[i].timestamp) + ",\"message\":\"" + logs[i].message + "\"}";
        if (i < logIndex - 1) json += ",";
    }
    json += "]";
    return json;
}

void handleLEDEndpoint() {
    if (httpServer.hasArg("state")) {

        String state = httpServer.arg("state");

        if (state == "on") {
            digitalWrite(LED_BUILTIN, LOW); // LED is active LOW
            httpServer.send(200, "text/plain", "LED turned ON");
        } else if (state == "off") {
            digitalWrite(LED_BUILTIN, HIGH);
            httpServer.send(200, "text/plain", "LED turned OFF");
        } else {
            httpServer.send(400, "text/plain", "Invalid state");
        }
    } else {
        httpServer.send(400, "text/plain", "Missing state parameter");
    }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
    // switch (type) {
    //     case WStype_TEXT:
    //       Serial.printf("Message du client %u: %s\n", num, payload);
    //       // Ici, vous pouvez traiter les messages entrants si nécessaire
    //       break;
    //     default:
    //       break;
    //   }
}

void println(String message) {
    Serial.println(message);

    message.replace("\\", "\\\\"); 
    message.replace("\n", "\\n"); 
    message.replace("\r", "\\r");  
    message.replace("\t", "\\t");  
    message.replace("\"", "\\\""); 

    logs[logIndex] = { millis(), message };
    logIndex = (logIndex + 1) % MAX_LOGS;

    // Send log update via WebSocket
    String wsMessage = "{\"type\":\"log\",\"timestamp\":" + String(millis()) + ",\"message\":\"" + message + "\"}";
    webSocketServer.broadcastTXT(wsMessage);

}