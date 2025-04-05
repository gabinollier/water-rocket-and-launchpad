// ROCKET CODE

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Adafruit_DPS310.h>
#include <LSM6DS3.h>
#include <Wire.h>
#include <ESP32Servo.h>

// Constants

const char* LAUNCHPAD_SSID = "Pas de tir 🚀";
const char* LAUNCHPAD_PASSWORD = "hippocampe";
const char* DNS_NAME = "rocket.local";
const byte DNS_PORT = 53;
const unsigned long WIFI_CHECK_INVERVAL = 500; // ms 
const float LAUNCH_DETECTION_THRESHOLD = 1.0; // meters
const float g = 9.80665; // m/s²
const int SAMPLE_RATE = 60; // Hz
const int PRELAUNCH_BUFFER_DURATION = 2; // seconds
const int TOTAL_BUFFER_DURATION = 60; // seconds
const unsigned long LANDING_DETECTION_ALTITUDE_THRESHOLD = 2; // meters
const unsigned long LANDING_DETECTION_DURATION = 5; // seconds
const float VOLUME = 3.0; // liters
const int PARACHUTE_SERVO_PIN = 10; 

const int PRELAUNCH_BUFFER_SIZE = SAMPLE_RATE * PRELAUNCH_BUFFER_DURATION;
const int TOTAL_BUFFER_SIZE = SAMPLE_RATE * TOTAL_BUFFER_DURATION;
const unsigned long MIN_LOOP_TIME = 1000 / SAMPLE_RATE; 

// Data structures

enum RocketState 
{
    ERROR,
    NOT_INITIALIZED,
    IDLING,
    WAITING_FOR_LAUNCH,
    ASCENDING,
    DESCENDING,
    RECONNECTING,
    SERVING_DATA
};

struct FlightData 
{
    unsigned long timestamp;
    float temperature;
    float pressure;
    float altitude;
    float accelX;
    float accelY;
    float accelZ;
    float gyroX;
    float gyroY;
    float gyroZ;
};

// Global variables

String launchpadIP; 
RocketState currentState = NOT_INITIALIZED;
Adafruit_DPS310 pressureSensor;
LSM6DS3 accelerationSensor(I2C_MODE, 0x6A);
WebServer httpServer(80);
float launchpadAltitude = 0.0; // meters
FlightData prelaunchBuffer[PRELAUNCH_BUFFER_SIZE];
int prelaunchBufferIndex = 0;
FlightData* flightBuffer = NULL; 
int flightBufferIndex = 0;
Servo parachuteServo;

// Function prototypes

bool setupWifiConnectionAndIdentifyRocket();
bool identifyRocket();
void setupAPIEndpoints();
bool setupPressureSensor();
bool setupAccelerationSensor();
bool setupI2C();
void scanI2CDevices();

void changeState(RocketState newState);
String stateToString(RocketState state);

bool detectLaunch(const FlightData &data);
bool detectLanding(const FlightData &data);
bool detectApogee(const FlightData &data);

FlightData getCurrentFlightData();
void getPressureAndTemperature(float &pressure, float &temperature);
void getAcceleration(float &accelX, float &accelY, float &accelZ);
void getGyroscope(float &gyroX, float &gyroY, float &gyroZ);
float calculateAltitude(float pressure);
float calculateMagnitude(float x, float y, float z);

void recordDataInPrelaunchBuffer(const FlightData &data);
void recordData(const FlightData &data);
void copyPrelaunchBufferToFlightBuffer();
bool resetDataBuffers();

void handleAPIIsIdling();
void handleAPIGetVolume();
void handleAPIStartWaitingForLaunch();
void handleAPIIsWaitingForLaunch();
void handleAPIGetFlightData();
void handleAPIReturnToIdle();



void setup() 
{
    Serial.begin(115200);
    delay(1000);


    parachuteServo.attach(PARACHUTE_SERVO_PIN);

    parachuteServo.write(0); 
    delay(1000);
    parachuteServo.write(90);
    delay(1000);
    parachuteServo.write(180);
    delay(1000);


    Serial.println("\nInitialisation...");

    bool initializationSuccess = true;
    initializationSuccess = initializationSuccess && setupI2C();
    initializationSuccess = initializationSuccess && setupAccelerationSensor();
    initializationSuccess = initializationSuccess && setupPressureSensor();
    initializationSuccess = initializationSuccess && resetDataBuffers();
    initializationSuccess = initializationSuccess && setupWifiConnectionAndIdentifyRocket();
    if (!initializationSuccess) 
    {
        Serial.println("Échec de l'initialisation du système. Mise en erreur.");
        changeState(ERROR);
        return;
    }

    setupAPIEndpoints();

    Serial.println("Initialisation terminée.\n");
    changeState(IDLING);
}

void loop() 
{
    static unsigned long previousLoopTime = millis(); //ms
    static unsigned long deltaTime = millis() - previousLoopTime; //ms

    deltaTime = millis() - previousLoopTime;
    if (deltaTime < MIN_LOOP_TIME)
    {
        delay(MIN_LOOP_TIME - deltaTime); 
    }
    deltaTime = millis() - previousLoopTime;
    previousLoopTime = millis();

    httpServer.handleClient();

    if (currentState == ERROR) 
    {
        return;
    }

    else if (currentState == WAITING_FOR_LAUNCH)
    {
        const FlightData data = getCurrentFlightData();
        recordDataInPrelaunchBuffer(data);

        if (detectLaunch(data))
        {
            changeState(ASCENDING);
        }
    }
    else if (currentState == ASCENDING)
    {
        const FlightData data = getCurrentFlightData();
        recordData(data);

        if (detectApogee(data)) 
        {
            changeState(DESCENDING);
        }
    }
    else if (currentState == DESCENDING)
    {
        const FlightData data = getCurrentFlightData();
        recordData(data);

        if (detectLanding(data))
        {
            changeState(RECONNECTING);
        }
    }
    else if (currentState == RECONNECTING)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            setupWifiConnectionAndIdentifyRocket();
        }
        else
        {
            changeState(SERVING_DATA);
        }
    }
    else if (currentState == SERVING_DATA)
    {
        // TODO : Send data to launchpad and change state when finished

        if (false) // todo : when finished
        {
            if (resetDataBuffers())
            {
                changeState(IDLING);
            }
            else
            {
                changeState(ERROR);
            }
        }
    }
}



bool detectLaunch(const FlightData &data)
{
    return data.altitude - launchpadAltitude > LAUNCH_DETECTION_THRESHOLD;
}



bool setupWifiConnectionAndIdentifyRocket()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

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

    for (int i = 0; i < 15; i++) 
    {
        Serial.println("Tentative de connexion au réseau Wi-Fi du pas de tir...");
        WiFi.begin(LAUNCHPAD_SSID, LAUNCHPAD_PASSWORD);

        delay(2000);

        if (WiFi.status() == WL_CONNECTED) 
        {
            launchpadIP = WiFi.gatewayIP().toString();

            Serial.println("Connecté au réseau Wi-Fi du pas de tir. IP locale : " + WiFi.localIP().toString() + ", IP du pas de tir : " + launchpadIP);
            return identifyRocket();
        }
    }

    String reason = "";
    switch (WiFi.status()) 
    {
        case WL_NO_SSID_AVAIL:
            reason = "SSID non disponible.";
            break;
        case WL_CONNECT_FAILED:
            reason = "Échec de la connexion.";
            break;
        case WL_CONNECTION_LOST:
            reason = "Connexion perdue.";
            break;
        case WL_DISCONNECTED:
            reason = "Déconnecté.";
            break;
        case WL_IDLE_STATUS:
            reason = "État d'idle.";
            break;
        case WL_NO_SHIELD:
            reason = "Pas de shield Wi-Fi.";
            break;
        case WL_SCAN_COMPLETED:
            reason = "Scan terminé.";
            break;
        case WL_CONNECTED:
            reason = "Déjà connecté.";
            break;
        default:
            reason = "Raison inconnue : " + String(WiFi.status());
            break;
    }

    Serial.println("Échec de la connexion au réseau Wi-Fi du pas de tir : " + reason);
    WiFi.disconnect();
    return false;
}

bool setupI2C()
{
    if (!Wire.begin())
    {
        Serial.println("Échec de l'initialisation I2C");
        return false;
    }

    Serial.println("Initialisation I2C réussie");
    return true;
}

bool resetDataBuffers()
{
    for (int i = 0; i < PRELAUNCH_BUFFER_SIZE; i++)
    {
        prelaunchBuffer[i] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }
    prelaunchBufferIndex = 0;

    if (flightBuffer != NULL) 
    {
        free(flightBuffer);
        flightBuffer = NULL;
    }
    flightBufferIndex = 0;
    
    flightBuffer = (FlightData*)malloc(sizeof(FlightData) * TOTAL_BUFFER_SIZE);
    if (!flightBuffer) 
    {
        Serial.println("Erreur d'allocation de mémoire pour le buffer de vol.");
        return false;
    }

    Serial.println("Buffer de vol réinitialisé avec succès.");
    return true;
}

void recordDataInPrelaunchBuffer(const FlightData &data)
{
    prelaunchBuffer[prelaunchBufferIndex] = data;
    prelaunchBufferIndex = (prelaunchBufferIndex + 1) % PRELAUNCH_BUFFER_SIZE; // Circular buffer
}

void recordData(const FlightData &data)
{
    if (flightBufferIndex < TOTAL_BUFFER_SIZE)
    {
        flightBuffer[flightBufferIndex] = data;
        flightBufferIndex++;
    }
}

FlightData getCurrentFlightData()
{
    float accelX = 0, accelY = 0, accelZ = 0, gyroX = 0, gyroY = 0, gyroZ = 0, pressure = 0, temperature = 0;
    getAcceleration(accelX, accelY, accelZ);
    getGyroscope(gyroX, gyroY, gyroZ);
    getPressureAndTemperature(pressure, temperature);
    float altitude = pressureSensor.readAltitude();

    FlightData data = {millis(), temperature, pressure, altitude, accelX, accelY, accelZ, gyroX, gyroY, gyroZ};
    return data;
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

bool setupAccelerationSensor()
{
    if (accelerationSensor.begin() != 0) 
    {
        Serial.println("Échec de l'initialisation de l'accéléromètre LSM6DS.");
        return false;
    }

    Serial.println("Accéléromètre LSM6DS initialisé avec succès");
    return true;
}


bool setupPressureSensor()
{
    if (!pressureSensor.begin_I2C())
    {
        Serial.println("Échec de l'initialisation du capteur de pression DPS310");
        return false;
    }
    else
    {
        Serial.println("Capteur de pression DPS310 initialisé");
    }

    pressureSensor.setMode(DPS310_CONT_PRESTEMP);
    pressureSensor.configurePressure(DPS310_128HZ, DPS310_128SAMPLES);
    pressureSensor.configureTemperature(DPS310_128HZ, DPS310_128SAMPLES);

    return true;

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

    if ((newState == NOT_INITIALIZED)
        || (newState == IDLING && (previousState != SERVING_DATA && previousState != NOT_INITIALIZED))
        || (newState == WAITING_FOR_LAUNCH && previousState != IDLING)
        || (newState == ASCENDING && previousState != WAITING_FOR_LAUNCH)
        || (newState == DESCENDING && previousState != ASCENDING)
        || (newState == RECONNECTING && previousState != DESCENDING)
        || (newState == SERVING_DATA && previousState != RECONNECTING))
    { 
        Serial.println("Erreur : impossible de passer à l'état " + stateToString(newState) + " depuis l'état " + stateToString(previousState) + ". Mise en erreur.");
        changeState(ERROR);
        return;
    }

    switch(newState) 
    {
        case IDLING:
            launchpadAltitude = getCurrentFlightData().altitude;

            if (previousState == SERVING_DATA)
            {
                bool success = resetDataBuffers();
                if (!success)
                {
                    changeState(ERROR);
                    return;
                }
            }

            break;

        case WAITING_FOR_LAUNCH:
            break;

        case ASCENDING:
            copyPrelaunchBufferToFlightBuffer();
            WiFi.disconnect();
            break;

        case DESCENDING:
            break;

        case RECONNECTING:
            break;

        case SERVING_DATA:
            break;
    }
}

void copyPrelaunchBufferToFlightBuffer()
{
    flightBufferIndex = 0;
    for (int i = prelaunchBufferIndex; i < prelaunchBufferIndex + PRELAUNCH_BUFFER_SIZE; i++)
    {
        FlightData data = prelaunchBuffer[i % PRELAUNCH_BUFFER_SIZE];
        flightBuffer[flightBufferIndex] = data;
        flightBufferIndex++;
    }
}

String stateToString(RocketState state) 
{
    switch(state) 
    {
        case ERROR: return "ERROR";
        case NOT_INITIALIZED: return "NOT_INITIALIZED";
        case IDLING: return "IDLING";
        case WAITING_FOR_LAUNCH: return "WAITING_FOR_LAUNCH";
        case ASCENDING: return "ASCENDING";
        case DESCENDING: return "DESCENDING";
        case RECONNECTING: return "RECONNECTING";
        case SERVING_DATA: return "SERVING_DATA";
        default: return "UNKNOWN";
    }
}

bool detectLanding(const FlightData &data)
{
    static unsigned long entryTime = 0; // milliseconds

    if (data.altitude < launchpadAltitude + LANDING_DETECTION_ALTITUDE_THRESHOLD)
    {
        if (entryTime == 0) // just entered the zone
        {
            entryTime = millis();
        }

        if (millis() >= entryTime + LANDING_DETECTION_DURATION * 1000)
        {
            return true;
        }

        return false;
    }
    else
    {
        entryTime = 0;
        return false;
    }
}

bool identifyRocket()
{
    if (launchpadIP == "") 
    {
        Serial.println("Erreur : l'adresse IP du pas de tir n'est pas définie.");
        return false;
    }

    for (int i = 0; i < 5; i++)
    {
        HTTPClient httpClient;
        String serverPath = "http://" + launchpadIP + "/api/identify-rocket";
        
        JsonDocument doc;
        doc["ip"] = WiFi.localIP().toString();
        String payload;
        serializeJson(doc, payload);
        
        httpClient.begin(serverPath);
        httpClient.addHeader("Content-Type", "application/json");
        
        Serial.println("Envoi de la requête d'identification au pas de tir...");
    
        int httpResponseCode = httpClient.POST(payload);
        String responsePayload = httpClient.getString();

        httpClient.end();
    
        if (httpResponseCode == 200) 
        {
            Serial.println("Fusée bien identifiée par le pas de tir.");
            return true;
        } else 
        {
            Serial.println("Erreur lors de l'envoi de la requête d'identification : " + String(httpResponseCode) + " - " + responsePayload);
        }

        delay(1000);
    }
}

void setupAPIEndpoints() 
{
    httpServer.begin();
    
    httpServer.on("/api/get-flight-data", HTTP_GET, handleAPIGetFlightData);
    httpServer.on("/api/get-volume", HTTP_GET, handleAPIGetVolume);
    httpServer.on("/api/is-idling", HTTP_GET, handleAPIIsIdling);
    httpServer.on("/api/is-waiting-for-launch", HTTP_GET, handleAPIIsWaitingForLaunch);
    httpServer.on("/api/start-waiting-for-launch", HTTP_POST, handleAPIStartWaitingForLaunch);
    httpServer.on("/api/return-to-idle", HTTP_POST, handleAPIReturnToIdle);

    httpServer.onNotFound([]() {
        httpServer.send(404, "application/json", "{\"error\":\"Endpoint non trouvé\"}");
    });

    Serial.println("Serveur HTTP et endpoints API initialisés");
}

void getPressureAndTemperature(float& pressure, float& temperature)
{
    sensors_event_t temp_event, pressure_event;
    pressureSensor.getEvents(&temp_event, &pressure_event);
    pressure = pressure_event.pressure / 1000.0; // Convert hPa to bars
    temperature = temp_event.temperature;
}

float calculateAltitude(float pressure)
{
    return 44330.0 * (1 - pow(pressure / 1013.25, 0.1903));
}

void getAcceleration(float& accelX, float& accelY, float& accelZ)
{
    accelX = accelerationSensor.readFloatAccelX() * g;
    accelY = accelerationSensor.readFloatAccelY() * g;
    accelZ = accelerationSensor.readFloatAccelZ() * g;
}

void getGyroscope(float& gyroX, float& gyroY, float& gyroZ)
{
    gyroX = accelerationSensor.readFloatGyroX();
    gyroY = accelerationSensor.readFloatGyroY();
    gyroZ = accelerationSensor.readFloatGyroZ();
}

float calculateMagnitude(float x, float y, float z)
{
    return sqrt(x * x + y * y + z * z);
}

bool detectApogee(const FlightData &data)
{
    // TODO : implement apogee detection
    return false;
}

void handleAPIIsIdling()
{
    JsonDocument doc;
    doc["is-idling"] = (currentState == IDLING);
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetVolume()
{
    JsonDocument doc;
    doc["volume"] = VOLUME;
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIStartWaitingForLaunch()
{   
    if (currentState != IDLING) 
    {
        httpServer.send(400, "application/json", "{\"error\":\"La fusée n'est pas prête\"}");
        return;
    }

    changeState(WAITING_FOR_LAUNCH);

    JsonDocument doc;
    doc["status"] = "success";
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);

}

void handleAPIIsWaitingForLaunch()
{
    JsonDocument doc;
    doc["is-waiting-for-launch"] = (currentState == WAITING_FOR_LAUNCH);
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIGetFlightData()
{
    if (flightBuffer == NULL || flightBufferIndex == 0) 
    {
        httpServer.send(404, "application/json", "{\"error\":\"Aucune donnée de vol disponible\"}");
        return;
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (int i = 0; i < flightBufferIndex; i++) 
    {
        JsonObject entry = array.createNestedObject();
        entry["timestamp"] = flightBuffer[i].timestamp;
        entry["temperature"] = flightBuffer[i].temperature;
        entry["pressure"] = flightBuffer[i].pressure;
        entry["altitude"] = flightBuffer[i].altitude;
        entry["accelX"] = flightBuffer[i].accelX;
        entry["accelY"] = flightBuffer[i].accelY;
        entry["accelZ"] = flightBuffer[i].accelZ;
        entry["gyroX"] = flightBuffer[i].gyroX;
        entry["gyroY"] = flightBuffer[i].gyroY;
        entry["gyroZ"] = flightBuffer[i].gyroZ;
    }
    String response;
    serializeJson(doc, response);
    httpServer.send(200, "application/json", response);
}

void handleAPIReturnToIdle()
{

}