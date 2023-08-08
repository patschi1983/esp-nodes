/**
 * EspNode.cpp
 *
 * Basic library for build node device on esp32 or esp8266.
 * <p>
 * WiFi, mQTT, debug, config Support out of the box.
 *
 * @author Creator patba
 * @author patbah
 * @version 1.0.0
 * @license Apache License 2.0
 */

#include "EspNode.h"

// constructors
EspNode::EspNode(char *nodeName, char *fwName, char *fwVersion)
{
  strcpy(_nodeName, nodeName);
  strcpy(_fwName, fwName);
  strcpy(_fwVersion, fwVersion);

#ifdef ESP8266
  _webServer = new ESP8266WebServer(80);
  _webUpdateServer = new ESP8266HTTPUpdateServer();
#elif ESP32
  _webServer = new WebServer(80);
  _webUpdateServer = new HTTPUpdateServer();
#else
#error "Wrong board - ESP8266 or ESP32 must be used."
#endif

  _mqttWifiClient = new WiFiClient();
  _mqttClient = new MQTTClient(MQTT_BUFFER);
}

// destructor
EspNode::~EspNode()
{
  // currently nothing in here
}

// setup method
void EspNode::setup()
{
  _debugSetup();
  _configRead();
  _nodeSetup();
  _wifiSetup();
  _mqttSetup();
  _webSetup();
  _debugSetupFinalize();
}

// loop method
void EspNode::loop()
{
  _debugLoop();
  _wifiLoop();
  _mqttLoop();
  _webLoop();
}

// debug print line
void EspNode::debugPrintln(String debugText)
{
  if (!_debugSerialEnabled && !_debugRemoteEnabled)
  {
    // No debug enabled so do nothing
    return;
  }

  if (_debugSerialEnabled || _debugRemoteEnabled)
  {
    String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;

    if (_debugSerialEnabled)
    {
      Serial.println(debugTimeText);
      Serial.flush();
    }

    if (_debugRemoteEnabled)
    {
      _mqttSend(mqttGetNodeTopic(_mqttDebugSubTopic), debugTimeText);
    }
  }
}

File EspNode::configOpenFile(const char *path, const char *mode)
{
  if (SPIFFS.exists(path))
  { // File exists, reading and loading
    debugPrintln(F("SPIFFS: opening file '") + String(path) + (F("' mode='")) + String(mode) + (F("'.")));

    return SPIFFS.open(path, mode);
  }
  else
  {
    debugPrintln(F("SPIFFS: file '") + String(path) + (F("' mode='")) + String(mode) + (F("' not found.")));
    return File();
  }
}

void EspNode::configSaveAddCallback(ConfigSaveCallback callback)
{
  for (int i = 0; i < CALLBACK_CNT; i++)
  {
    if (_configSaveCallbacks[i] == nullptr)
    {
      _configSaveCallbacks[i] = callback;
      return;
    }
  }

  debugPrintln("SPIFFS: All save callbacks already used - restarting.");
  _nodeReset();
}

void EspNode::webStartHttpMsg(String type, int code)
{
  webStartHttpMsg(type, String(F("")), code, String(F("/")));
}

void EspNode::webStartHttpMsg(String type, String meta, int code)
{
  webStartHttpMsg(type, meta, code, String(F("/")));
}

void EspNode::webStartHttpMsg(String type, String meta, int code, String redirectUrl)
{
  // check if auth is needed
  _webCheckAuth();

  // Prepare for multipart and send first part of html header
  String httpMessage = FPSTR(HTTP_HEAD_START);

  httpMessage.replace(String(F("{v}")), String(_uniqueNodeName));

  _webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  _webServer->send(code, String(F("text/html")), httpMessage);

  // Send script, style and meta
  webSendHttpContent(FPSTR(HTTP_SCRIPT));
  webSendHttpContent(FPSTR(HTTP_STYLE));

  if (meta.length() > 0)
  {
    meta.replace(HTML_REPLACE_REDIRURL, redirectUrl);
    webSendHttpContent(meta);
  }

  // Send end of html header and start of html body
  webSendHttpContent(FPSTR(HTTP_HEAD_END));

  // Send common content header
  webSendHttpContent(String(F("<h1>")));
  webSendHttpContent(String(_uniqueNodeName));
  webSendHttpContent(String(F("</h1>")));

  if (type.length() > 0)
  {
    webSendHttpContent(String(F("<h2>")));
    webSendHttpContent(String(type));
    webSendHttpContent(String(F("</h2>")));
  }
}

void EspNode::webSendHttpContent(String content, String find, String replace)
{
  content.replace(find, replace);
  webSendHttpContent(content);
}

void EspNode::webSendHttpContent(String content)
{
  _webServer->sendContent(content);
}

void EspNode::webEndHttpMsg()
{
  // Send end of html body
  _webServer->sendContent(FPSTR(HTTP_END));

  _webServer->sendContent("");
  _webServer->setContentLength(CONTENT_LENGTH_NOT_SET);
}

String EspNode::webGetArg(const String &name)
{
  return _webServer->arg(name);
}

void EspNode::webAddButtonHandler(const String uri, const String buttonName)
{
  for (int i = 0; i < BUTTON_CNT; i++)
  {
    if (_webButtons[i].isEmpty())
    {
      String htmlButton = String(HTML_BUTTON);
      htmlButton.replace("{uri}", uri);
      htmlButton.replace("{name}", buttonName);
      _webButtons[i] = htmlButton;

      debugPrintln(String(F("HTTP: Added button[")) + String(i) + String(F("] - ")) + _webButtons[i]);

      return;
    }
  }

  debugPrintln(String(F("HTTP: All button handlers already used - restarting.")));
  _nodeReset();
}

void EspNode::webRegisterHandler(const Uri &uri, std::function<void(void)> handler)
{
  _webServer->on(uri, handler);
}

String EspNode::mqttGetDefaultTopic()
{
  String topic = _mqttDefaultTopicBase + String(_uniqueNodeName);
  return topic;
}

String EspNode::mqttGetNodeTopic(String subTopic)
{
  String topic = _mqttTopic;

  if (topic.isEmpty())
  {
    topic = mqttGetDefaultTopic();
  }

  if (!subTopic.isEmpty())
  {
    if (!subTopic.startsWith(F("/")))
    {
      topic += F("/");
    }

    topic += subTopic;
  }

  return topic;
}

String EspNode::mqttGetNodeCmdTopic(String subTopic)
{
  String topic = _mqttTopic;

  if (topic.isEmpty())
  {
    topic = mqttGetDefaultTopic();
  }

  topic += F("/cmd");

  if (!subTopic.isEmpty())
  {
    if (!subTopic.startsWith(F("/")))
    {
      topic += F("/");
    }

    topic += subTopic;
  }

  return topic;
}

String EspNode::mqttGetCommonDefaultTopic()
{
  return _mqttDefaultTopicBase;
}

String EspNode::mqttGetCommonNodesCmdTopic(String subTopic)
{
  String topic = mqttGetCommonDefaultTopic();

  topic += F("cmd");

  if (!subTopic.isEmpty())
  {
    if (!subTopic.startsWith(F("/")))
    {
      topic += F("/");
    }

    topic += subTopic;
  }

  return topic;
}

String EspNode::mqttGetOnOffPayload(bool on)
{
  return (on ? _mqttOnPayload : _mqttOffPayload);
}

bool EspNode::mqttSendAvailable(bool reset)
{
  if (reset)
  {
    debugPrintln(String(F("MQTT: Preparing reset, sending available --> false.")) + String(_mqttServer));

    return _mqttSend(mqttGetNodeTopic(_mqttAvailableSubTopic), String(F("false")));
  }

  if (_mqttAvailableMsgPending)
  {
    debugPrintln(String(F("MQTT: Sending pending available state --> true.")));

    _mqttAvailableMsgPending = !_mqttSend(mqttGetNodeTopic(_mqttAvailableSubTopic), String(F("true")));

    if (!_mqttAvailableMsgPending)
    {
      // send mqtt send enable status, if available paylod has been sent
      _mqttSend(mqttGetNodeTopic(_mqttEnableSendSubTopic), mqttGetOnOffPayload(_mqttEnableSendSubTopic));
      _mqttSend(mqttGetNodeTopic(_mqttEnableDebugSubTopic), mqttGetOnOffPayload(_debugSerialEnabled));
      _mqttSend(mqttGetNodeTopic(_mqttEnableRemoteDebugSubTopic), mqttGetOnOffPayload(_debugRemoteEnabled));

      // Call mqtt available callbacks
      for (int i = 0; i < CALLBACK_CNT; i++)
      {
        if (_mqttAvailableCallbacks[i] != nullptr)
        {
          _mqttAvailableCallbacks[i]();
        }
      }
    }

    return true;
  }
  else
  {
    return false;
  }

  return true;
}
bool EspNode::mqttSend(String topic, String cmd)
{
  if (_mqttSendEnabled)
  {
    return _mqttClient->publish(topic, cmd);
  }
  else
  {
    return false;
  }
}

void EspNode::mqttAvailableAddCallback(MQTTAvailableCallback callback)
{
  for (int i = 0; i < CALLBACK_CNT; i++)
  {
    if (_mqttAvailableCallbacks[i] == nullptr)
    {
      _mqttAvailableCallbacks[i] = callback;
      return;
    }
  }

  debugPrintln("MQTT: All receive callbacks already used - restarting.");
  _nodeReset();
}

void EspNode::mqttRcvAddCallback(MQTTClientCallbackSimple callback)
{
  for (int i = 0; i < CALLBACK_CNT; i++)
  {
    if (_mqttRcvCallbacks[i] == nullptr)
    {
      _mqttRcvCallbacks[i] = callback;
      return;
    }
  }

  debugPrintln("MQTT: All receive callbacks already used - restarting.");
  _nodeReset();
}

void EspNode::_nodeSetup()
{
  WiFi.macAddress(_espMac); // Read our MAC address and save it to espMac
  String uniqueName = String(_nodeName) + "_" + String(_espMac[0], HEX) + String(_espMac[1], HEX) + String(_espMac[2], HEX) + String(_espMac[3], HEX) + String(_espMac[4], HEX) + String(_espMac[5], HEX);
  strcpy(_uniqueNodeName, uniqueName.c_str());
}

void EspNode::_nodeReset()
{
  debugPrintln(F("RESET: reset"));

  mqttSendAvailable(true);
  delay(500);
  ESP.restart();
  delay(5000);
}

void EspNode::_debugSetup()
{
  // Setup serial for debug output
  Serial.begin(115200);
  Serial.println();

  debugPrintln(String(F("********************************************************************************")));
  debugPrintln(String(F("SYSTEM: Starting ")) + String(_fwName) + String(F(" v")) + String(_fwVersion));
}

void EspNode::_debugSetupFinalize()
{
  if (!_debugSerialEnabled)
  {
    debugPrintln(String(F("SYSTEM: Stopping serial debug due to configuration")));

    Serial.flush();
    Serial.end();
  }
}

void EspNode::_debugLoop()
{
  // currently nothing in here
}

void EspNode::_configRead()
{
  // Read saved config.json from SPIFFS
  debugPrintln(F("SPIFFS: mounting SPIFFS"));

#ifdef ESP8266
  if (SPIFFS.begin())
#endif
#ifdef ESP32
    if (SPIFFS.begin(true))
#endif
    {
      if (SPIFFS.exists("/config.json"))
      { // File exists, reading and loading
        debugPrintln(F("SPIFFS: reading /config.json"));

        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile)
        {
          size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[configFileSize]);
          configFile.readBytes(buf.get(), configFileSize);

          DynamicJsonDocument configJson(CONFIG_SIZE);
          DeserializationError jsonError = deserializeJson(configJson, buf.get());

          if (jsonError)
          { // Couldn't parse the saved config
            bool removedJson = SPIFFS.remove("/config.json");

            debugPrintln(String(F("SPIFFS: [ERROR] Failed to parse /config.json: ")) + String(jsonError.c_str()));

            if (removedJson)
            {
              debugPrintln(String(F("SPIFFS: Removed corrupt file /config.json")));
            }
            else
            {
              debugPrintln(String(F("SPIFFS: [ERROR] Corrupt file /config.json could not be removed")));
            }
          }
          else
          {
            // Read node configuration
            if (!configJson["nodeName"].isNull())
            {
              strcpy(_nodeName, configJson["nodeName"]);
            }
            if (!configJson["configUser"].isNull())
            {
              strcpy(_configUser, configJson["configUser"]);
            }
            if (!configJson["configPassword"].isNull())
            {
              strcpy(_configPassword, configJson["configPassword"]);
            }

            // Read MQTT configuration
            if (!configJson["mqttServer"].isNull())
            {
              strcpy(_mqttServer, configJson["mqttServer"]);
            }
            if (!configJson["mqttPort"].isNull())
            {
              _mqttPort = configJson["mqttPort"];
            }
            if (!configJson["mqttUser"].isNull())
            {
              strcpy(_mqttUser, configJson["mqttUser"]);
            }
            if (!configJson["mqttPassword"].isNull())
            {
              strcpy(_mqttPassword, configJson["mqttPassword"]);
            }
            if (!configJson["mqttTopic"].isNull())
            {
              strcpy(_mqttTopic, configJson["mqttTopic"]);
            }

            // Read Debug configuration
            if (!configJson["debugSerialEnabled"].isNull())
            {
              _debugSerialEnabled = configJson["debugSerialEnabled"];
            }
            if (!configJson["debugRemoteEnabled"].isNull())
            {
              _debugRemoteEnabled = configJson["debugRemoteEnabled"];
            }

            // Print read JSON configuration
            String configJsonStr;
            serializeJson(configJson, configJsonStr);

            debugPrintln(String(F("SPIFFS: parsed json:")) + configJsonStr);
          }
        }
        else
        {
          debugPrintln(F("SPIFFS: [ERROR] File not found /config.json"));
        }
      }
      else
      {
        debugPrintln(F("SPIFFS: [WARNING] /config.json not found, will be created on first config save"));
      }
    }
    else
    {
      debugPrintln(F("SPIFFS: [ERROR] Failed to mount FS"));
    }
}

void EspNode::_configSave()
{
  // Save the parameters to config.json
  debugPrintln(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(CONFIG_SIZE);

  // Save node configuration
  jsonConfigValues["nodeName"] = _nodeName;
  jsonConfigValues["configUser"] = _configUser;
  jsonConfigValues["configPassword"] = _configPassword;

  // Save MQTT configuration
  jsonConfigValues["mqttServer"] = _mqttServer;
  jsonConfigValues["mqttPort"] = _mqttPort;
  jsonConfigValues["mqttUser"] = _mqttUser;
  jsonConfigValues["mqttPassword"] = _mqttPassword;
  jsonConfigValues["mqttTopic"] = _mqttTopic;

  // Save Debug configuration
  jsonConfigValues["debugSerialEnabled"] = _debugSerialEnabled;
  jsonConfigValues["debugRemoteEnabled"] = _debugRemoteEnabled;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debugPrintln(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();

    // Print saved JSON configuration
    String configJsonStr;
    serializeJson(jsonConfigValues, configJsonStr);

    debugPrintln(String(F("SPIFFS: saved json:")) + configJsonStr);
  }

  // Call save callbacks
  for (int i = 0; i < CALLBACK_CNT; i++)
  {
    if (_configSaveCallbacks[i] != nullptr)
    {
      _configSaveCallbacks[i]();
    }
  }

  delay(500);
}

void EspNode::_configClear(bool all)
{ // Clear out all local storage
  debugPrintln(F("RESET: Formatting SPIFFS"));

  SPIFFS.format();

  _wifiResetSettings();

  EEPROM.begin(512);
  debugPrintln(F("Clearing EEPROM..."));

  for (uint16_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }

  debugPrintln(F("RESET: Rebooting device"));
  _nodeReset();
}

void EspNode::_wifiResetSettings()
{
  debugPrintln(F("WIFI: Clearing WiFi settings..."));

  WiFiManager wifiManager;
  wifiManager.resetSettings();
}

void EspNode::_wifiConfig(String wifiSsid, String wifiPass)
{
  debugPrintln(String(F("WIFI: Changing to WiFi network")) + wifiSsid + String(F("|")) + wifiPass + String(F("...")));

  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  delay(1000);
}

void EspNode::_wifiSetup()
{
  WiFiManager wifiManager;
  wifiManager.autoConnect(_uniqueNodeName);
}

void EspNode::_wifiLoop()
{
  while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
  {
    // Check WiFi is connected and that we have a valid IP, retry until we do.

    if (WiFi.status() == WL_CONNECTED)
    {
      // If we're currently connected, disconnect so we can try again
      WiFi.disconnect();
    }

    _wifiSetup();
  }
}

void EspNode::_webSetup()
{
  // Connect with http update server
  if ((_configPassword[0] != '\0') && (_configUser[0] != '\0'))
  {
    _webUpdateServer->setup(_webServer, String(F("/updateFw")), String(_configUser), String(_configPassword));
  }
  else
  { // or without a password if not
    _webUpdateServer->setup(_webServer, String(F("/updateFw")));
  }

  // Setup webserver

  _webServer->on("/", [this]()
                 { this->_webHandleRoot(); });
  _webServer->on("/settings", [this]()
                 { this->_webHandleSettings(); });
  _webServer->on("/saveSettings", [this]()
                 { this->_webHandleSaveSettings(); });
  _webServer->on("/status", [this]()
                 { this->_webHandleStatus(); });

  _webServer->onNotFound([this]()
                         { this->_webHandleNotFound(); });
  _webServer->begin();

  debugPrintln(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());
}

void EspNode::_webCheckAuth()
{
  if (_configPassword[0] != '\0')
  {
    // Request HTTP auth if configPassword is set

    if (!_webServer->authenticate(_configUser, _configPassword))
    {
      return _webServer->requestAuthentication();
    }
  }
}

void EspNode::_webHandleRootCallback(void *ptr)
{
}

void EspNode::_webHandleRoot()
{
  debugPrintln(String(F("HTTP: WebHandleRoot called from client: ")) + _webServer->client().remoteIP().toString());

  webStartHttpMsg(String(F("Navigation")), 200);

  for (int i = 0; i < BUTTON_CNT; i++)
  {
    if (!_webButtons[i].isEmpty())
    {
      webSendHttpContent(_webButtons[i]);
      debugPrintln(String(F("HTTP: Send http content for button[")) + String(i) + String(F("] - ")) + _webButtons[i]);
    }
  }

  webSendHttpContent(HTML_ROOT_SETTINGS);

  webSendHttpContent(HTML_ROOT_STATUS);

  webEndHttpMsg();

  debugPrintln(String(F("HTTP: WebHandleRoot page sent.")));
}

void EspNode::_webHandleSettings()
{
  debugPrintln(String(F("HTTP: WebHandleSettings called from client: ")) + _webServer->client().remoteIP().toString());

  webStartHttpMsg(String(F("Settings")), 200);

  String httpMessage = "";
  webSendHttpContent(HTML_SETTINGS_FORM_START);

  webSendHttpContent(HTML_SETTINGS_NODE_NAME, String(F("{nodeName}")), _nodeName);

  webSendHttpContent(HTML_SETTINGS_WIFI_SSID, String(F("{wifiSsid}")), String(WiFi.SSID()));
  webSendHttpContent(HTML_SETTINGS_WIFI_PASSWD, String(F("{wifiSsid}")), MASKED_PASSWORD);

  webSendHttpContent(HTML_SETTINGS_ADMIN_USER, String(F("{configUser}")), String(_configUser));
  webSendHttpContent(HTML_SETTINGS_ADMIN_PASSWD, String(F("{configPassword}")), (strlen(_configPassword) != 0) ? MASKED_PASSWORD : String(F("")));

  webSendHttpContent(HTML_SETTINGS_MQTT_SERVER, String(F("{mqttServer}")), String(_mqttServer));
  webSendHttpContent(HTML_SETTINGS_MQTT_PORT, String(F("{mqttPort}")), String(_mqttPort));
  webSendHttpContent(HTML_SETTINGS_MQTT_USER, String(F("{mqttUser}")), String(_mqttUser));
  webSendHttpContent(HTML_SETTINGS_MQTT_PASSWD, String(F("{mqttPassword}")), (strlen(_mqttPassword) != 0) ? MASKED_PASSWORD : String(F("")));
  webSendHttpContent(HTML_SETTINGS_MQTT_TOPIC, String(F("{mqttTopic}")), (strlen(_mqttTopic) != 0) ? String(_mqttTopic) : mqttGetDefaultTopic());
  webSendHttpContent(HTML_SETTINGS_MQTT_STATUS, String(F("{mqttStatus}")), (_mqttClient->connected()) ? String(F("connected")) : String(F("diconnected")));

  webSendHttpContent(HTML_SETTINGS_DEBUG_SERIAL, String(F("{debugSerialEnabled}")), (_debugSerialEnabled ? String(F("1")) : String(F("0"))));
  webSendHttpContent(HTML_SETTINGS_DEBUG_REMOTE, String(F("{debugRemoteEnabled}")), (_debugRemoteEnabled ? String(F("1")) : String(F("0"))));

  webSendHttpContent(HTML_SETTINGS_BTN_SAVE_FORM_END);
  webSendHttpContent(HTML_SETTINGS_BTN_BACK);

  webEndHttpMsg();

  debugPrintln(String(F("HTTP: WebHandleSettings page sent.")));
}

void EspNode::_webHandleSaveSettings()
{
  debugPrintln(String(F("HTTP: WebHandleSaveSettings called from client: ")) + _webServer->client().remoteIP().toString());
  debugPrintln(String(F("HTTP: Checking for changed settings...")));

  bool configShouldSave = false;
  // check if node settings have changed
  if (_webServer->arg(String(F("nodeName"))) != String(_nodeName))
  {
    configShouldSave = true;

    String lowerNodeName = _webServer->arg(String(F("nodeName")));
    lowerNodeName.toLowerCase();
    lowerNodeName.toCharArray(_nodeName, 32);
  }
  if (_webServer->arg(String(F("configUser"))) != String(_configUser))
  {
    configShouldSave = true;

    _webServer->arg(String(F("configUser"))).toCharArray(_configUser, 32);
  }
  if (_webServer->arg(String(F("configPassword"))) != String(MASKED_PASSWORD) && _webServer->arg(String(F("configPassword"))) != String(_configPassword))
  {
    configShouldSave = true;

    _webServer->arg(String(F("configPassword"))).toCharArray(_configPassword, 32);
  }

  // check if wifi settings have changed
  bool shouldSaveWifi = false;
  char wifiSsid[32] = "";
  char wifiPass[64] = "";
  if (_webServer->arg(String(F("wifiSsid"))) != String(WiFi.SSID()))
  {
    shouldSaveWifi = true;

    _webServer->arg(String(F("wifiSsid"))).toCharArray(wifiSsid, 32);

    if (_webServer->arg(String(F("wifiPass"))) != String(MASKED_PASSWORD) && _webServer->arg(String(F("mqttPassword"))) != String(wifiPass))
    {
      _webServer->arg(String(F("wifiPass"))).toCharArray(wifiPass, 64);
    }
  }

  // check if mqtt settings have changed
  if (_webServer->arg(String(F("mqttServer"))) != String(_mqttServer))
  {
    configShouldSave = true;

    _webServer->arg(String(F("mqttServer"))).toCharArray(_mqttServer, 64);
  }
  if (_webServer->arg(String(F("mqttPort"))) != String(_mqttPort))
  {
    configShouldSave = true;

    _mqttPort = atoi(_webServer->arg(String(F("mqttPort"))).c_str());
  }
  if (_webServer->arg(String(F("mqttUser"))) != String(_mqttUser))
  {
    configShouldSave = true;

    _webServer->arg(String(F("mqttUser"))).toCharArray(_mqttUser, 32);
  }
  if (_webServer->arg(String(F("mqttPassword"))) != String(MASKED_PASSWORD) && _webServer->arg(String(F("mqttPassword"))) != String(_mqttPassword))
  {
    configShouldSave = true;

    _webServer->arg(String(F("mqttPassword"))).toCharArray(_mqttPassword, 32);
  }
  if (_webServer->arg(String(F("mqttTopic"))) != String(_mqttTopic))
  {
    configShouldSave = true;

    _webServer->arg(String(F("mqttTopic"))).toCharArray(_mqttTopic, 128);
  }

  // check if debug settings have changed
  if (_webServer->arg(String(F("debugSerialEnabled"))) != String(_debugSerialEnabled))
  {
    configShouldSave = true;

    _debugSerialEnabled = (_webServer->arg(String(F("debugSerialEnabled"))).toInt() > 0);
  }

  if (_webServer->arg(String(F("debugRemoteEnabled"))) != String(_debugRemoteEnabled))
  {
    configShouldSave = true;

    _debugRemoteEnabled = (_webServer->arg(String(F("debugRemoteEnabled"))).toInt() > 0);
  }

  // Process config or wifi changes
  if (configShouldSave || shouldSaveWifi)
  {
    // Config updated, notify user and trigger write of configurations or wifi settings7
    debugPrintln(String(F("HTTP: Sending /saveSettings page to client connected from: ")) + _webServer->client().remoteIP().toString());

    webStartHttpMsg(String(F("")), HTML_SAVESETTINGS_START_REDIR_15SEC, 200, String(F("/")));
    webSendHttpContent(HTML_SAVESETTINGS_SAVE_RESTART, HTML_REPLACE_REDIRURL, String(F("/")));
    webEndHttpMsg();

    if (configShouldSave)
    {
      _configSave();
    }

    if (shouldSaveWifi)
    {
      _wifiConfig(wifiSsid, wifiPass);
    }

    _nodeReset();
  }
  else
  {
    // No change found, notify user and link back to config page
    debugPrintln(String(F("HTTP: Sending /saveSettings page to client connected from: ")) + _webServer->client().remoteIP().toString());

    webStartHttpMsg(String(F("")), HTML_SAVESETTINGS_START_REDIR_3SEC, 200, String(F("/settings")));
    webSendHttpContent(HTML_SAVESETTINGS_NOCHANGE, HTML_REPLACE_REDIRURL, String(F("/settings")));
    webEndHttpMsg();
  }

  debugPrintln(String(F("HTTP: WebHandleSaveSettings page sent.")));
}

void EspNode::_webHandleStatus()
{
  debugPrintln(String(F("HTTP: WebHandleStatus called from client: ")) + _webServer->client().remoteIP().toString());

  webStartHttpMsg(String(F("Status")), 200);

  webSendHttpContent(HTML_STATUS_FW_NAME, String(F("{firmwareName}")), String(_fwName));
  webSendHttpContent(HTML_STATUS_FW_VERSION, String(F("{firmwareVersion}")), String(_fwVersion));
  webSendHttpContent(HTML_STATUS_FW_FORM);
  webSendHttpContent(HTML_STATUS_CPU, String(F("{cpuFreq}")), String(ESP.getCpuFreqMHz()));
  webSendHttpContent(HTML_STATUS_SKETCH_SIZE, String(F("{sketchSize}")), String(ESP.getSketchSize()));
  webSendHttpContent(HTML_STATUS_SKETCH_FREESIZE, String(F("{freeSketchSize}")), String(ESP.getFreeSketchSpace()));
  webSendHttpContent(HTML_STATUS_HEAP, String(F("{freeHeap}")), String(ESP.getFreeHeap()));
  webSendHttpContent(HTML_STATUS_IPADDR, String(F("{ipAddr}")), String(WiFi.localIP().toString()));
  webSendHttpContent(HTML_STATUS_SIGSTRENGTH, String(F("{sigStrength}")), String(WiFi.RSSI()));
  unsigned long uptime = (millis() / 1000);
  webSendHttpContent(HTML_STATUS_UPTIME, String(F("{uptime}")), String(uptime));
  webSendHttpContent(HTML_STATUS_BTN_BACK);

  webEndHttpMsg();

  debugPrintln(String(F("HTTP: WebHandleStatus page sent.")));
}

void EspNode::_webHandleNotFound()
{
  debugPrintln(String(F("HTTP: WebHandleNotFound called from client: ")) + _webServer->client().remoteIP().toString());

  webStartHttpMsg(String(F("File Not Found\n\n")), 404);
  webSendHttpContent(String(F("URI: ")));
  webSendHttpContent(_webServer->uri());
  webSendHttpContent(String(F("\nMethod: ")));
  webSendHttpContent((_webServer->method() == HTTP_GET) ? String(F("GET")) : String(F("POST")));
  webSendHttpContent(String(F("\nArguments: ")));
  webSendHttpContent(String(_webServer->args()));
  webSendHttpContent(String(F("\n")));

  for (uint8_t i = 0; i < _webServer->args(); i++)
  {
    webSendHttpContent(" " + _webServer->argName(i) + ": " + _webServer->arg(i) + "\n");
  }

  webEndHttpMsg();

  debugPrintln(String(F("HTTP: WebHandleNotFound page sent.")));
}

void EspNode::_webLoop()
{
  _webServer->handleClient();
}

void EspNode::_mqttSetup()
{
  _mqttClient->begin(_mqttServer, _mqttPort, *_mqttWifiClient);

  _mqttClient->onMessage([this](String &topic, String &payload)
                         { this->_mqttRcvCallback(topic, payload); });
  _mqttConnect();
}

void EspNode::_mqttConnect()
{
  // Connect initially or reconnect if connection was lost
  if (!_mqttClient->connected())
  {
    bool retry = false;

    // check for retry delay
    if (_mqttRetryMillis > 0)
    {
      unsigned long millisPassed = millis() - _mqttRetryMillis;
      retry = (millisPassed >= MQTT_RETRY_DELAY);
    }
    else
    {
      retry = true;
    }

    if (retry)
    {
      // Set keepAlive, cleanSession, timeout
      _mqttClient->setOptions(30, true, 1000);

      _mqttClient->connect(_uniqueNodeName, _mqttUser, _mqttPassword);
      if (_mqttClient->connected())
      {
        _mqttRetryMillis = 0;
        _mqttAvailableMsgPending = true;

        debugPrintln(String(F("MQTT: Connection established to ")) + String(_mqttServer));

        _mqttClient->subscribe(mqttGetNodeCmdTopic(F("#")));
        _mqttClient->subscribe(mqttGetCommonNodesCmdTopic(F("#")));
      }
      else
      {
        _mqttRetryMillis = millis();

        debugPrintln(String(F("MQTT: Connection could not be established - failed with rc ")) + String(_mqttClient->returnCode()));
      }
    }
  }
}

bool EspNode::_mqttSend(String topic, String cmd)
{
  return _mqttClient->publish(topic, cmd);
}

void EspNode::_mqttSendAvailableResend()
{
  _mqttAvailableMsgPending = true;
}

void EspNode::_mqttRcvCallback(String &topic, String &payload)
{
  debugPrintln(String(F("MQTT: Message arrived on topic: '")) + topic + String(F("' with payload: '")) + payload + String(F("'.")));

  if (topic.equals(mqttGetNodeCmdTopic(_mqttRebootSubTopic)))
  {
    // standard command reset
    if (payload.equals(_mqttSavePayload))
    {
      _configSave();
    }

    _nodeReset();
  }
  else if (topic.equals(mqttGetNodeCmdTopic(_mqttEnableSendSubTopic)))
  {
    // standard command to enable/disable the sending of node specific payloads
    // debug and availability will always be send
    if (payload.equals(_mqttOnPayload))
    {
      _mqttSendEnabled = true;
      _mqttSend(mqttGetNodeTopic(_mqttEnableSendSubTopic), payload);
    }
    else if (payload.equals(_mqttOffPayload))
    {
      _mqttSendEnabled = false;
      _mqttSend(mqttGetNodeTopic(_mqttEnableSendSubTopic), payload);
    }
    else
    {
      debugPrintln(String(F("MQTT: Unknown payload in topic - ")) + topic + String(F("#")) + payload + String(F("'.")));
    }
  }
  else if (topic.equals(mqttGetNodeCmdTopic(_mqttEnableDebugSubTopic)))
  {
    // standard command to enable/disable the serial debug mode
    if (payload.equals(_mqttOnPayload))
    {
      _debugSerialEnabled = true;
      _mqttSend(mqttGetNodeTopic(_mqttEnableDebugSubTopic), payload);
    }
    else if (payload.equals(_mqttOffPayload))
    {
      _debugSerialEnabled = false;
      _mqttSend(mqttGetNodeTopic(_mqttEnableDebugSubTopic), payload);
    }
    else
    {
      debugPrintln(String(F("MQTT: Unknown payload in topic - ")) + topic + String(F("#")) + payload + String(F("'.")));
    }
  }
  else if (topic.equals(mqttGetNodeCmdTopic(_mqttEnableRemoteDebugSubTopic)))
  {
    // standard command to enable/disable the remote debug mode
    if (payload.equals(_mqttOnPayload))
    {
      _debugRemoteEnabled = true;
      _mqttSend(mqttGetNodeTopic(_mqttEnableRemoteDebugSubTopic), payload);
    }
    else if (payload.equals(_mqttOffPayload))
    {
      _debugRemoteEnabled = false;
      _mqttSend(mqttGetNodeTopic(_mqttEnableRemoteDebugSubTopic), payload);
    }
    else
    {
      debugPrintln(String(F("MQTT: Unknown payload in topic - ")) + topic + String(F("#")) + payload + String(F("'.")));
    }
  }
  else if (topic.equals(mqttGetCommonNodesCmdTopic("")))
  {
    // announce command to distribute the availabe messages again via mqtt (common topic)
    if (payload.equals(_mqttAnnouncePayload))
    {
      _mqttSendAvailableResend();
      mqttSendAvailable(false);
    }
    else
    {
      debugPrintln(String(F("MQTT: Unknown payload in topic - ")) + topic + String(F("#")) + payload + String(F("'.")));
    }
  }
  else
  {
    // delegate to handler if no standard command was triggered
    for (int i = 0; i < CALLBACK_CNT; i++)
    {
      if (_mqttRcvCallbacks[i] != nullptr)
      {
        _mqttRcvCallbacks[i](topic, payload);
      }
    }
  }
}

void EspNode::_mqttLoop()
{
  _mqttConnect();
  mqttSendAvailable(false);

  _mqttClient->loop();
}