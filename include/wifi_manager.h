#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class WiFiManager {
public:
    WiFiManager();
    void begin();                                               // Inicializa el WiFiManager
    bool connect();                                             // Conecta a WiFi con credenciales guardadas
    bool isConnected();                                         // Verifica si WiFi está conectado
    bool reconnect();                                           // Intenta reconectar WiFi
    String getLocalIP();                                        // Obtiene IP local
    int getRSSI();                                              // Obtiene intensidad de señal WiFi
    void disconnect();                                          // Desconecta WiFi
    void loop();                                                // Loop para portal de config
    const String& getConfiguredSSID() const { return currentSSID; }  // Obtiene SSID configurado
    void stopConfigPortal();                                    // Detiene portal de config
    void startConfigPortal();                                   // Inicia portal de config
    bool isPortalActive() const { return portalActive; }        // Verifica si portal está activo
    int getConnectionAttempts() const { return connectionAttempts; }  // Obtiene intentos de conexión
    bool isConfigured();                                        // Verifica si dispositivo está configurado
    void clearAllConfig();                                      // Borra toda la configuración NVS
    String fetchZonesFromGraphQL(int userId);                   // Obtiene zonas desde GraphQL
    String fetchSublocationsFromGraphQL(int zoneId);            // Obtiene sublocalidades por zona
    String saveDeviceLocation(const String& zoneName, const String& subLocation, int zoneId);  // Guarda ubicación
    bool updateDispositivoStatus(int dispositivoId, const String& status, int batteryLevel);  // Actualiza estado del dispositivo

private:
    unsigned long lastConnectionAttempt;         // Último intento de conexión
    bool wasConnected;                           // Estado previo de conexión
    WebServer configServer;                      // Servidor web para portal de config
    DNSServer dnsServer;                         // Servidor DNS para captive portal
    bool portalActive;                           // Portal de config activo
    bool pendingReconnect;                       // Reconexión pendiente
    unsigned long lastPortalAnnounce;            // Último anuncio del portal
    unsigned long reconnectRequestTime;          // Timestamp de solicitud de reconexión
    String currentSSID;                          // SSID actual
    String currentPassword;                      // Password actual
    int connectionAttempts;                      // Intentos de conexión
    int loggedUserId;                            // ID del usuario logueado
    String detectedMasterMac;                    // MAC del maestro detectado desde GraphQL
    bool dnsServerActive;                        // DNS Server activo
    IPAddress cachedBackendIP;                   // IP del backend en cache
    bool backendIPCached;                        // Si el IP del backend esta en cache
    
    void loadStoredCredentials();               // Carga credenciales WiFi desde NVS
    bool resolveBackendDNS(IPAddress& ip);       // Resuelve DNS con retry
    String loginUser(const String& email, const String& password);  // Login de usuario GraphQL
    void saveCredentials(const String& ssid, const String& password);   // Guarda credenciales en NVS
    void loadDeviceConfig();                     // Carga configuración del dispositivo
    void loadDeviceLocation();                   // Carga ubicación del dispositivo
    void saveDeviceConfig(DeviceMode mode, const String& masterMac);    // Guarda config del dispositivo
    void setupPortalRoutes();                    // Configura rutas del portal web
    void sendCORSHeaders();                      // Agrega headers CORS a la respuesta
    String renderPortalPage(const String& statusMessage);   // Renderiza página del portal
    bool attemptConnection(unsigned long timeout);   // Intenta conectar con timeout
    String macToString(const uint8_t* mac);      // Convierte MAC a String
    void stringToMac(const String& macStr, uint8_t* mac);   // Convierte String a MAC
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
