#include "BlinkerESPMQTT.h"
#include "Blinker/BlinkerDebug.h"
#include "modules/ArduinoJson/ArduinoJson.h"

#if defined(ESP8266)
    #include <ESP8266mDNS.h>
    #include <ESP8266WiFi.h>
    #include <ESP8266WebServer.h>
#elif defined(ESP32)
    #include <ESPmDNS.h>
    #include <WiFi.h>
    #include <WebServer.h>
#endif

WiFiServer *_server;
WiFiClient _client;
IPAddress apIP(192, 168, 4, 1);
#if defined(ESP8266)
    IPAddress netMsk(255, 255, 255, 0);
#endif

void BlinkerESPMQTT::commonBegin(const char* _auth,
                                const char* _ssid,
                                const char* _pswd,
                                String & _type)
{
    Base::begin();
    connectWiFi(_ssid, _pswd);
    this->conn.aliType(_type);
    this->conn.begin(_auth);
    Base::loadTimer();

    #if defined(ESP8266)
        BLINKER_LOG(BLINKER_F("ESP8266_MQTT initialized..."));
    #elif defined(ESP32)
        BLINKER_LOG(BLINKER_F("ESP32_MQTT initialized..."));
    #endif
}

void BlinkerESPMQTT::smartconfigBegin(const char* _auth, String & _type)
{
    Base::begin();
    if (!autoInit()) smartconfig();
    this->conn.aliType(_type);
    this->conn.begin(_auth);
    Base::loadTimer();

    #if defined(ESP8266)
        BLINKER_LOG(BLINKER_F("ESP8266_MQTT initialized..."));
    #elif defined(ESP32)
        BLINKER_LOG(BLINKER_F("ESP32_MQTT initialized..."));
    #endif
}

void BlinkerESPMQTT::apconfigBegin(const char* _auth, String & _type)
{
    Base::begin();
    if (!autoInit())
    {
        softAPinit();
        while(WiFi.status() != WL_CONNECTED)
        {
            serverClient();
            ::delay(10);
        }
    }

    this->conn.aliType(_type);
    this->conn.begin(_auth);
    Base::loadTimer();

    #if defined(ESP8266)
        BLINKER_LOG(BLINKER_F("ESP8266_MQTT initialized..."));
    #elif defined(ESP32)
        BLINKER_LOG(BLINKER_F("ESP32_MQTT initialized..."));
    #endif
}

bool BlinkerESPMQTT::autoInit()
{
    WiFi.mode(WIFI_STA);
    String _hostname = BLINKER_F("DiyArduino_");
    _hostname += macDeviceName();

    #if defined(ESP8266)
        WiFi.hostname(_hostname.c_str());
    #elif defined(ESP32)
        WiFi.setHostname(_hostname.c_str());
    #endif

    WiFi.begin();
    ::delay(500);

    BLINKER_LOG(BLINKER_F("Waiting for WiFi "), 
                BLINKER_WIFI_AUTO_INIT_TIMEOUT / 1000,
                BLINKER_F("s, will enter SMARTCONFIG or "),
                BLINKER_F("APCONFIG while WiFi not connect!"));

    uint8_t _times = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ::delay(500);
        if (_times > BLINKER_WIFI_AUTO_INIT_TIMEOUT / 500) break;
        _times++;
    }

    if (WiFi.status() != WL_CONNECTED) return false;
    else {
        BLINKER_LOG(BLINKER_F("WiFi Connected."));
        BLINKER_LOG(BLINKER_F("IP Address: "));
        BLINKER_LOG(WiFi.localIP());

        return true;
    }
}

void BlinkerESPMQTT::smartconfig()
{
    WiFi.mode(WIFI_STA);
    String _hostname = BLINKER_F("DiyArduino_");
    _hostname += macDeviceName();
    
    #if defined(ESP8266)
        WiFi.hostname(_hostname.c_str());
    #elif defined(ESP32)
        WiFi.setHostname(_hostname.c_str());
    #endif

    WiFi.beginSmartConfig();
    
    BLINKER_LOG(BLINKER_F("Waiting for SmartConfig."));
    while (!WiFi.smartConfigDone()) {
        ::delay(500);
    }

    BLINKER_LOG(BLINKER_F("SmartConfig received."));
    
    BLINKER_LOG(BLINKER_F("Waiting for WiFi"));
    while (WiFi.status() != WL_CONNECTED) {
        ::delay(500);
    }

    BLINKER_LOG(BLINKER_F("WiFi Connected."));

    BLINKER_LOG(BLINKER_F("IP Address: "));
    BLINKER_LOG(WiFi.localIP());
}

void BlinkerESPMQTT::softAPinit()
{
    _server = new WiFiServer(80);

    WiFi.mode(WIFI_AP);
    String softAP_ssid = BLINKER_F("DiyArduino_");
    softAP_ssid += macDeviceName();

    #if defined(ESP8266)
        WiFi.hostname(softAP_ssid.c_str());
        WiFi.softAPConfig(apIP, apIP, netMsk);
    #elif defined(ESP32)
        WiFi.setHostname(softAP_ssid.c_str());
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    #endif
    
    WiFi.softAP(softAP_ssid.c_str(), ("12345678"));
    delay(100);

    _server->begin();
    BLINKER_LOG(BLINKER_F("AP IP address: "), WiFi.softAPIP());
    BLINKER_LOG(BLINKER_F("HTTP _server started"));
    BLINKER_LOG(BLINKER_F("URL: http://"), WiFi.softAPIP());
}

void BlinkerESPMQTT::serverClient()
{
    if (!_client)
    {
        _client = _server->available();
    }
    else
    {
        // if (_client.status() == CLOSED)
        if (!_client.connected())
        {
            _client.stop();
            BLINKER_LOG(BLINKER_F("Connection closed on _client"));
        }
        else
        {
            if (_client.available())
            {
                String data = _client.readStringUntil('\r');

                // data = data.substring(4, data.length() - 9);
                _client.flush();

                BLINKER_LOG(BLINKER_F("clientData: "), data);

                if (STRING_contains_string(data, "ssid") && \
                    STRING_contains_string(data, "pswd"))
                {
                    String msg = BLINKER_F("{\"hello\":\"world\"}");
                    
                    String s= BLINKER_F("HTTP/1.1 200 OK\r\n");
                    s += BLINKER_F("Content-Type: application/json;");
                    s += BLINKER_F("charset=utf-8\r\n");
                    s += BLINKER_F("Content-Length: ");
                    s += String(msg.length());
                    s += BLINKER_F("\r\nConnection: Keep Alive\r\n\r\n");
                    s += msg;
                    s += BLINKER_F("\r\n");

                    _client.print(s);
                    
                    _client.stop();

                    parseUrl(data);
                }
            }
        }
    }
}

bool BlinkerESPMQTT::parseUrl(String data)
{
    BLINKER_LOG(BLINKER_F("APCONFIG data: "), data);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& wifi_data = jsonBuffer.parseObject(data);

    if (!wifi_data.success()) {
        return false;
    }
                    
    String _ssid = wifi_data["ssid"];
    String _pswd = wifi_data["pswd"];

    BLINKER_LOG(BLINKER_F("ssid: "), _ssid);
    BLINKER_LOG(BLINKER_F("pswd: "), _pswd);

    free(_server);
    connectWiFi(_ssid, _pswd);
    return true;
}

void BlinkerESPMQTT::connectWiFi(String _ssid, String _pswd)
{
    connectWiFi(_ssid.c_str(), _pswd.c_str());
}

void BlinkerESPMQTT::connectWiFi(const char* _ssid, const char* _pswd)
{
    uint32_t connectTime = millis();

    BLINKER_LOG(BLINKER_F("Connecting to "), _ssid);

    WiFi.mode(WIFI_STA);
    String _hostname = BLINKER_F("DiyArduinoMQTT_");
    _hostname += macDeviceName();
    
    #if defined(ESP8266)
        WiFi.hostname(_hostname.c_str());
    #elif defined(ESP32)
        WiFi.setHostname(_hostname.c_str());
    #endif

    if (_pswd && strlen(_pswd)) {
        WiFi.begin(_ssid, _pswd);
    }
    else {
        WiFi.begin(_ssid);
    }

    while (WiFi.status() != WL_CONNECTED) {
        ::delay(50);

        if (millis() - connectTime > BLINKER_CONNECT_TIMEOUT_MS && WiFi.status() != WL_CONNECTED) {
            connectTime = millis();
            BLINKER_LOG(BLINKER_F("WiFi connect timeout, please check ssid and pswd!"));
            BLINKER_LOG(BLINKER_F("Retring WiFi connect again!"));
        }
    }
    BLINKER_LOG(BLINKER_F("Connected"));

    IPAddress myip = WiFi.localIP();
    BLINKER_LOG(BLINKER_F("Your IP is: "), myip);

    // mDNSInit();
}