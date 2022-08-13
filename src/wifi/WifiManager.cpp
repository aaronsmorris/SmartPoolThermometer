#if !( defined(ESP32) ) && !( defined(ESP8266) )
  #error This code is intended to run on ESP8266 or ESP32 platform!
#endif

#include <Arduino.h>
#include <WiFiClient.h>
#include <ezTime.h>
#include <AsyncElegantOTA.h>

#include "wifi/WifiManager.h"
#include "Configuration.h"

#define MAX_CONNECT_TIMEOUT_MS 15000 // 10 seconds to connect before creating its own AP

const int RSSI_MAX =-50;// define maximum straighten of signal in dBm
const int RSSI_MIN =-100;// define minimum strength of signal in dBm

WiFiClient espClient;

int dBmtoPercentage(int dBm) {
    int quality;
    if(dBm <= RSSI_MIN) {
        quality = 0;
    } else if(dBm >= RSSI_MAX) {  
        quality = 100;
    } else {
        quality = 2 * (dBm + 100);
    }
    return quality;
}

#ifdef ESP8266
bool getLocalTime(struct tm * info, uint32_t ms=5000) {
    uint32_t start = millis();
    time_t now;
    while((millis()-start) <= ms) {
        time(&now);
        localtime_r(&now, info);
        if(info->tm_year > (2016 - 1900)){
            return true;
        }
        delay(10);
    }
    return false;
}
#endif

const String htmlTop = "<html>\
  <head>\
    <title>%s</title>\
    <style>\
      body { background-color: #303030; font-family: 'Anaheim',sans-serif; Color: #d8d8d8; }\
    </style>\
  </head>\
  <body>\
    <h1>%s LED Controller</h1>";

const String htmlBottom = "<br><br><hr>\
  <p>Uptime: %02d:%02d:%02d | Device: %s</p>\
  %s\
  </body>\
</html>";

const String htmlWifiApConnectForm = "<h2>Connect to WiFi Access Point (AP)</h2>\
    <form method='POST' action='/connect' enctype='application/x-www-form-urlencoded'>\
      <label for='ssid'>SSID (AP Name):</label><br>\
      <input type='text' id='ssid' name='ssid'><br><br>\
      <label for='pass'>Password (WPA2):</label><br>\
      <input type='password' id='pass' name='password' minlength='8' autocomplete='off' required><br><br>\
      <input type='submit' value='Connect...'>\
    </form>";

const String htmlDeviceConfigs = "<hr><h2>Configs</h2>\
    <form method='POST' action='/config' enctype='application/x-www-form-urlencoded'>\
      <label for='deviceName'>Device name:</label><br>\
      <input type='text' id='deviceName' name='deviceName' value='%s'><br>\
      <br>\
      <label for='mqttServer'>MQTT server:</label><br>\
      <input type='text' id='mqttServer' name='mqttServer' value='%s'><br>\
      <label for='mqttPort'>MQTT port:</label><br>\
      <input type='text' id='mqttPort' name='mqttPort' value='%u'><br>\
      <label for='mqttTopic'>MQTT topic:</label><br>\
      <input type='text' id='mqttTopic' name='mqttTopic' value='%s'><br>\
      <br>\
      <label for='battVoltsDivider'>Battery volt measurement divider:</label><br>\
      <input type='text' id='battVoltsDivider' name='battVoltsDivider' value='%.2f'><br>\
      <br>\
      <label for='deepSleepDurationSec'>Deep sleep cycle duration:</label><br>\
      <input type='text' id='deepSleepDurationSec' name='deepSleepDurationSec' value='%u'> sec.<br>\
      <br>\
      <input type='submit' value='Set...'>\
    </form>";

CWifiManager::CWifiManager(ISensorProvider *sp): 
apMode(false), rebootNeeded(false), postedSensorUpdate(false), sensorProvider(sp) {    

    // Start capturing voltage
    batteryVoltage = sensorProvider->getBatteryVoltage(NULL);

    strcpy(SSID, configuration.wifiSsid);
    server = new AsyncWebServer(WEB_SERVER_PORT);
    mqtt.setClient(espClient);
    connect();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s'", SSID);
    WiFi.begin(SSID, configuration.wifiPassword);
    apMode = false;
    
  } else {

    // Create AP using fallback and chip ID
    uint32_t chipId = 0;
    #ifdef ESP32
      for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
    #elif ESP8266
      chipId = ESP.getChipId();
    #endif
  
    Log.infoln("Chip ID: '%i'", chipId);
    sprintf_P(softAP_SSID, "%s_%i", WIFI_FALLBACK_SSID, chipId);
    Log.infoln("Creating WiFi: '%s' / '%s'", softAP_SSID, WIFI_FALLBACK_PASS);
    
    if (WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS)) {
      apMode = true;
      Log.infoln("Wifi AP '%s' created, listening on '%s'", softAP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
      Log.errorln("Wifi AP faliled");
    };

  }
  
}

void CWifiManager::listen() {

    status = WF_LISTENING;

    // Web
    server->on("/", std::bind(&CWifiManager::handleRoot, this, std::placeholders::_1));
    server->on("/connect", HTTP_POST, std::bind(&CWifiManager::handleConnect, this, std::placeholders::_1));
    server->on("/config", HTTP_POST, std::bind(&CWifiManager::handleConfig, this, std::placeholders::_1));
    server->begin();
    Log.infoln("Web server listening on %s port %i", WiFi.localIP().toString().c_str(), WEB_SERVER_PORT);

    // NTP
    Log.infoln("Configuring time from %s at %i (%i)", configuration.ntpServer, configuration.gmtOffset_sec, configuration.daylightOffset_sec);
    configTime(configuration.gmtOffset_sec, configuration.daylightOffset_sec, configuration.ntpServer);
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
        Log.noticeln("The time is %i:%i", timeinfo.tm_hour,timeinfo.tm_min);
    }

    // OTA
    AsyncElegantOTA.begin(server);

    // MQTT
    mqtt.setServer(configuration.mqttServer, configuration.mqttPort);
    mqtt.setKeepAlive(60);
    if (strlen(configuration.mqttServer) && strlen(configuration.mqttTopic) && !mqtt.connected()) {
        Log.noticeln("Attempting MQTT connection to '%s:%i' ...", configuration.mqttServer, configuration.mqttPort);
        if (mqtt.connect("arduinoClient")) {
            Log.noticeln("MQTT connected");
            postSensorUpdate();
        } else {
            Log.warningln("MQTT connect failed, rc=%i", mqtt.state());
        }
    }
}

void CWifiManager::loop() {

    batteryVoltage = (float)(batteryVoltage + sensorProvider->getBatteryVoltage(NULL)) / 2.0;

  if (rebootNeeded && millis() - tMillis > 200) {
    Log.noticeln("Rebooting...");
#ifdef ESP32
    ESP.restart();
#elif ESP8266
    ESP.reset();
#endif
    return;
  }

  if (WiFi.status() == WL_CONNECTED || apMode ) {
    // WiFi is connected

    if (status != WF_LISTENING) {  
      // Start listening for requests
      listen();
      return;
    }

    if (millis() - tMillis > 30000) {
        tMillis = millis();
        postSensorUpdate();
    }

  } else {
    // WiFi is down

    switch (status) {
      case WF_LISTENING: {
        Log.infoln("Disconnecting %i", status);
        server->end();
        status = WF_CONNECTING;
        connect();
      } break;
      case WF_CONNECTING: {
        if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
          Log.warning("Connecting failed (wifi status %i) after %l ms, create an AP instead", (millis() - tMillis), WiFi.status());
          tMillis = millis();
          strcpy(SSID, "");
          connect();
        }
      } break;

    }

  }
  
}

void CWifiManager::handleRoot(AsyncWebServerRequest *request) {

    Log.infoln("handleRoot");

    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->printf(htmlTop.c_str(), configuration.name, configuration.name);

    if (apMode) {
        response->printf(htmlWifiApConnectForm.c_str());
    } else {
        response->printf("<p>Connected to '%s'</p>", SSID);
    }

    response->printf(htmlDeviceConfigs.c_str(), configuration.name, configuration.mqttServer, 
    configuration.mqttPort, configuration.mqttTopic, 
    configuration.battVoltsDivider, configuration.deepSleepDurationSec);

    bool c; float t = sensorProvider->getTemperature(&c);
    char sensorStr[100];
    sprintf(sensorStr, "Temp: %.2fF %s", (t*1.8+32), c ? "" : "(stale)");
    response->printf(htmlBottom.c_str(), hr, min % 60, sec % 60, String(DEVICE_NAME), sensorStr);
    request->send(response);
}

void CWifiManager::handleConnect(AsyncWebServerRequest *request) {

  Log.infoln("handleConnect");

  String ssid = request->arg("ssid");
  String password = request->arg("password");
  
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->printf(htmlTop.c_str(), configuration.name, configuration.name);
  response->printf("<p>Connecting to '%s' ... see you on the other side!</p>", ssid.c_str());
  response->printf(htmlBottom.c_str(), hr, min % 60, sec % 60, String(DEVICE_NAME), String(""));
  request->send(response);

  ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
  password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));

  Log.noticeln("Saved config SSID: '%s'", configuration.wifiSsid);

  EEPROM_saveConfig();

  strcpy(SSID, configuration.wifiSsid);
  connect();
}

void CWifiManager::handleConfig(AsyncWebServerRequest *request) {

    Log.infoln("handleConfig");

    String deviceName = request->arg("deviceName");
    deviceName.toCharArray(configuration.name, sizeof(configuration.name));
    Log.infoln("Device req name: %s", deviceName);
    Log.infoln("Device size %i name: %s", sizeof(configuration.name), configuration.name);

    String mqttServer = request->arg("mqttServer");
    mqttServer.toCharArray(configuration.mqttServer, sizeof(configuration.mqttServer));
    Log.infoln("MQTT Server: %s", mqttServer);

    uint16_t mqttPort = atoi(request->arg("mqttPort").c_str());
    configuration.mqttPort = mqttPort;
    Log.infoln("MQTT Port: %u", mqttPort);

    String mqttTopic = request->arg("mqttTopic");
    mqttTopic.toCharArray(configuration.mqttTopic, sizeof(configuration.mqttTopic));
    Log.infoln("MQTT Topic: %s", mqttTopic);

    uint16_t battVoltsDivider = atoi(request->arg("battVoltsDivider").c_str());
    configuration.battVoltsDivider = battVoltsDivider;
    Log.infoln("battVoltsDivider: %u", battVoltsDivider);

    float deepSleepDurationSec = atof(request->arg("deepSleepDurationSec").c_str());
    configuration.deepSleepDurationSec = deepSleepDurationSec;
    Log.infoln("deepSleepDurationSec: %.2f", deepSleepDurationSec);

    EEPROM_saveConfig();

    // TODO check if MQTT reconnect is needed
    if (strlen(configuration.mqttServer) && strlen(configuration.mqttTopic)) {
        rebootNeeded = true;
    }

    request->redirect("/");
}

void CWifiManager::postSensorUpdate() {
#ifdef TEMP_SENSOR
    if (!mqtt.connected()) {
        if (mqtt.state() < MQTT_CONNECTED 
            && strlen(configuration.mqttServer) && strlen(configuration.mqttTopic)) { // Reconnectable
            Log.noticeln("Attempting to reconnect from MQTT state %i at '%s:%i' ...", mqtt.state(), configuration.mqttServer, configuration.mqttPort);
            if (mqtt.connect("arduinoClient")) {
                Log.noticeln("MQTT reconnected");
            } else {
                Log.warningln("MQTT reconnect failed, rc=%i", mqtt.state());
            }
        }
        if (!mqtt.connected()) {
            Log.noticeln("MQTT not connected %i", mqtt.state());
            return;
        }
    }

    if (!strlen(configuration.mqttTopic)) {
        Log.warningln("Blank MQTT topic");
        return;
    }

    char topic[255];
    bool current;
    float v;
    
    v = sensorProvider->getTemperature(&current);
    if (current) {
        sprintf_P(topic, "%s/sensor/temperature", configuration.mqttTopic);
        mqtt.publish(topic,String(v, 2).c_str());
        Log.noticeln("Sent '%FC' temp to MQTT topic '%s'", v, topic);
    }

    v = sensorProvider->getHumidity(&current);
    if (current) {
        sprintf_P(topic, "%s/sensor/humidity", configuration.mqttTopic);
        mqtt.publish(topic,String(v, 2).c_str());
        Log.noticeln("Sent '%F%' humidity to MQTT topic '%s'", v, topic);
    }

    #ifdef BATTERY_SENSOR
    v = (float)(batteryVoltage + sensorProvider->getBatteryVoltage(NULL)) / 2.0;
    sprintf_P(topic, "%s/sensor/battery", configuration.mqttTopic);
    mqtt.publish(topic,String(v, 2).c_str());
    Log.noticeln("Sent '%Fv' battery voltage to MQTT topic '%s'", v, topic);
    #endif

    time_t now; 
    time(&now);
    sprintf_P(topic, "%s/sensor/timestamp", configuration.mqttTopic);
    mqtt.publish(topic,String(now).c_str());
    Log.noticeln("Sent '%u' timestamp to MQTT topic '%s'", now, topic);

    postedSensorUpdate = true;
#endif
}