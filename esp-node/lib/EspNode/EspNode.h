/**
 * EspNode.h
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

#ifndef EspNode_h
#define EspNode_h

#include <Arduino.h>

#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <MQTTClient.h>

#ifdef ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#elif ESP32
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#else
#error "Wrong board - ESP8266 or ESP32 must be used."
#endif

const unsigned long CONNECT_TO = 300;         // Timeout for WiFi and MQTT connection attempts in seconds
const unsigned long RECONNECT_TO = 15;        // Timeout for WiFi reconnection attempts in seconds
const int CONFIG_SIZE = 10240;                // Configuration size
const char MASKED_PASSWORD[] = "********";    // Masked password constant^
const uint16_t MQTT_BUFFER = 4096;            // Size of buffer for incoming MQTT message
const unsigned long MQTT_RETRY_DELAY = 10000; // Delay for reconnect
const static int CALLBACK_CNT = 5;            // Max number of callbacks
const static int BUTTON_CNT = 5;              // Max number of buttons

//***** HTML Text - Root *****//
const char HTML_BUTTON[] PROGMEM = "<a href='{uri}'><button>{name}</button></a><hr>";
const char HTML_ROOT_SETTINGS[] PROGMEM = "<a href='/settings'><button>Settings</button></a>";
const char HTML_ROOT_STATUS[] PROGMEM = "<hr><a href='/status'><button>Status</button></a>";

//***** HTML Text - Setting *****//
const char HTML_SETTINGS_FORM_START[] PROGMEM = "<form method='POST' action='saveSettings'>";
const char HTML_SETTINGS_NODE_NAME[] PROGMEM = "<b>Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small></i><input id='nodeName' required name='nodeName' maxlength=31 placeholder='Node Name' pattern='[a-z0-9_]*' value='{nodeName}'>";
const char HTML_SETTINGS_WIFI_SSID[] PROGMEM = "<br/><br/><b>WiFi SSID</b> <i><small>(required)</small></i><input id='wifiSsid' required name='wifiSsid' maxlength=32 placeholder='WiFi SSID' value='{wifiSsid}'>";
const char HTML_SETTINGS_WIFI_PASSWD[] PROGMEM = "<br/><b>WiFi Password</b> <i><small>(optional)</small></i><input id='wifiPass' name='wifiPass' type='password' maxlength=64 placeholder='WiFi Password' value='{wifiPass}'>";
const char HTML_SETTINGS_ADMIN_USER[] PROGMEM = "<br/><br/><b>Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='{configUser}'>";
const char HTML_SETTINGS_ADMIN_PASSWD[] PROGMEM = "<br/><b>Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='{configPassword}'>";
const char HTML_SETTINGS_MQTT_SERVER[] PROGMEM = "<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='{mqttServer}'>";
const char HTML_SETTINGS_MQTT_PORT[] PROGMEM = "<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='1883' value='{mqttPort}'>";
const char HTML_SETTINGS_MQTT_USER[] PROGMEM = "<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='{mqttUser}'>";
const char HTML_SETTINGS_MQTT_PASSWD[] PROGMEM = "<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword' value='{mqttPassword}'>";
const char HTML_SETTINGS_MQTT_TOPIC[] PROGMEM = "<br/><b>MQTT Topic</b> <i><small>(optional)</small></i><input id='mqttTopic' name='mqttTopic' maxlength=127 value='{mqttTopic}'>";
const char HTML_SETTINGS_MQTT_STATUS[] PROGMEM = "<br/><b>MQTT Status</b><input id='mqttSatus' readonly name='mqttSatus' placeholder='mqttStatus' value='{mqttStatus}'>";
const char HTML_SETTINGS_DEBUG_SERIAL[] PROGMEM = "<br/><br/><b>Debug Serial Enabled</b> <i><small>(0/1)</small></i><input id='debugSerialEnabled' name='debugSerialEnabled' type='number' min='0' max='1' value='{debugSerialEnabled}'>";
const char HTML_SETTINGS_DEBUG_REMOTE[] PROGMEM = "<br/><b>Debug Remote Enabled</b><i> <small>(0/1)</small></i><input id='debugRemoteEnabled' name='debugRemoteEnabled' type='number' min='0' max='1' value='{debugRemoteEnabled}'>";
const char HTML_SETTINGS_BTN_SAVE_FORM_END[] PROGMEM = "<br/><br/><button type='submit'>Save</button></form>";
const char HTML_SETTINGS_BTN_BACK[] PROGMEM = "<hr><a href='/'><button>Back</button></a>";

//***** HTML Text - SaveSetting *****//
const char HTML_SAVESETTINGS_START_REDIR_15SEC[] PROGMEM = "<meta http-equiv='refresh' content='15;url={redirectUrl}' />";
const char HTML_SAVESETTINGS_START_REDIR_3SEC[] PROGMEM = "<meta http-equiv='refresh' content='3;url={redirectUrl}' />";
const char HTML_SAVESETTINGS_SAVE_RESTART[] PROGMEM = "<br/>Saving updated configuration values and restarting device ... <a href='{redirectUrl}'>redirect</a>";
const char HTML_SAVESETTINGS_SAVE_NORESTART[] PROGMEM = "<br/>Saving updated configuration values and updating device ... <a href='{redirectUrl}'>redirect</a>";
const char HTML_SAVESETTINGS_NOCHANGE[] PROGMEM = "<br/>No changes found ... redirecting to <a href='{redirectUrl}'>redirecting</a>";
const char HTML_REPLACE_REDIRURL[] PROGMEM = "{redirectUrl}";

//***** HTML Text - Status *****//
const char HTML_STATUS_FW_NAME[] PROGMEM = "<b>FW Name: </b> {firmwareName}";
const char HTML_STATUS_FW_VERSION[] PROGMEM = "<br/><b>FW Name: </b> {firmwareVersion}";
const char HTML_STATUS_FW_FORM[] PROGMEM = "<form method='POST' action='/updateFw' enctype='multipart/form-data'> <hr> <b>FW Upload: </b> <input type='file' accept='.bin,.bin.gz' name='firmware'> <button>Update Firmware</button> </form> <hr>";
const char HTML_STATUS_CPU[] PROGMEM = "<br/><b>CPU Frequency: </b> {cpuFreq} MHz";
const char HTML_STATUS_SKETCH_SIZE[] PROGMEM = "<br/><b>Sketch Size: </b> {sketchSize} bytes";
const char HTML_STATUS_SKETCH_FREESIZE[] PROGMEM = "<br/><b>Free Sketch Space: </b> {freeSketchSize} bytes";
const char HTML_STATUS_HEAP[] PROGMEM = "<br/><b>Heap Free: </b> {freeHeap}";
const char HTML_STATUS_IPADDR[] PROGMEM = "<br/><b>IP Address: </b> {ipAddr}";
const char HTML_STATUS_SIGSTRENGTH[] PROGMEM = "<br/><b>Signal Strength: </b> {sigStrength}";
const char HTML_STATUS_UPTIME[] PROGMEM = "<br/><b>Uptime: </b> {uptime} sec";
const char HTML_STATUS_BTN_BACK[] PROGMEM = "<hr><a href='/'><button>Back</button></a>";

typedef void (*ConfigSaveCallback)();
typedef void (*MQTTAvailableCallback)();

class EspNode
{
public:
  EspNode(char *nodeName, char *fwName, char *fwVersion);
  ~EspNode();

  void setup();
  void loop();

  void debugPrintln(String debugText);

  File configOpenFile(const char *path, const char *mode);
  void configSaveAddCallback(ConfigSaveCallback callback);

  void webStartHttpMsg(String type, int code);
  void webStartHttpMsg(String type, String meta, int code);
  void webStartHttpMsg(String type, String meta, int code, String redirectUrl);
  void webSendHttpContent(String content, String find, String replace);
  void webSendHttpContent(String content);
  void webEndHttpMsg();
  String webGetArg(const String &name);
  void webAddButtonHandler(const String, const String buttonName);
  void webRegisterHandler(const Uri &uri, std::function<void(void)> handler);

  String mqttGetDefaultTopic();
  String mqttGetNodeTopic(String subTopic);
  String mqttGetNodeCmdTopic(String subTopic);
  String mqttGetCommonDefaultTopic();
  String mqttGetCommonNodesCmdTopic(String subTopic);
  String mqttGetOnOffPayload(bool on);
  bool mqttSendAvailable(bool reset);
  bool mqttSend(String topic, String cmd);
  void mqttAvailableAddCallback(MQTTAvailableCallback callback);
  void mqttRcvAddCallback(MQTTClientCallbackSimple callback);

private:
  char _fwName[16] = "esp_node";                                                                         // Name of the firmware
  char _fwVersion[8] = "0.0";                                                                            // Version of the firmware
  char _nodeName[32] = "esp_node";                                                                       // Nodes name - default value, may be overridden
  byte _espMac[6];                                                                                       // Byte array to store our MAC address
  char _uniqueNodeName[128] = "";                                                                        // Unique node name, generated by combining node name and parts of the mac address
  char _configUser[32] = "admin";                                                                        // User name for web access - default value, may be overridden
  char _configPassword[32] = "";                                                                         // Password for web access - default value, may be overridden
  ConfigSaveCallback _configSaveCallbacks[CALLBACK_CNT] = {nullptr, nullptr, nullptr, nullptr, nullptr}; // Save callback array to dispatch save calls

  void _nodeSetup();
  void _nodeReset();

  bool _debugSerialEnabled = true;  // Enable serial debug - default value, may be overridden
  bool _debugRemoteEnabled = false; // Enable remote debug - default value, may be overridden

  void _debugSetup();
  void _debugSetupFinalize();
  void _debugLoop();

  void _configRead();
  void _configSave();
  void _configClear(bool all);

  void _wifiResetSettings();
  void _wifiConfig(String wifiSsid, String wifiPass);
  void _wifiSetup();
  void _wifiLoop();

#ifdef ESP8266
  ESP8266WebServer *_webServer;
  ESP8266HTTPUpdateServer *_webUpdateServer;
#elif ESP32
  WebServer *_webServer;
  HTTPUpdateServer *_webUpdateServer;
#else
#error "Wrong board - ESP8266 or ESP32 must be used."
#endif
  String _webButtons[BUTTON_CNT] = {"", "", "", "", ""};

  void _webSetup();
  void _webCheckAuth();
  static void _webHandleRootCallback(void *ptr);
  void _webHandleRoot();
  void _webHandleSettings();
  void _webHandleSaveSettings();
  void _webHandleStatus();
  void _webHandleNotFound();
  void _webLoop();

  WiFiClient *_mqttWifiClient;
  MQTTClient *_mqttClient;
  unsigned long _mqttRetryMillis = 0; // Timestamp used to measure delay for retry
  char _mqttServer[64] = "";          // MQTT Server IP/URL - Default value, maybe overridden
  int _mqttPort = 1883;               // MQTT Server Port - Default value, maybe overridden
  char _mqttUser[32] = "";            // MQTT User name - Default value, maybe overridden
  char _mqttPassword[32] = "";        // MQTT Password - Default value, maybe overridden
  char _mqttTopic[128] = "";          // MQTT Topic - Default value, maybe overridden

  const char _mqttDefaultTopicBase[10] = "espnodes/";             // MQTT Base for default topic
  const char _mqttAvailableSubTopic[10] = "available";            // MQTT available sub topic topic
  const char _mqttRebootSubTopic[7] = "reboot";                  // MQTT reboot subtopic
  const char _mqttEnableSendSubTopic[10] = "mqtt/send";           // MQTT enable send subtopic
  const char _mqttDebugSubTopic[10] = "debug/log";                // MQTT debug remote subtopic for printing logs
  const char _mqttEnableDebugSubTopic[13] = "debug/serial";       // MQTT enable debug serial subtopic
  const char _mqttEnableRemoteDebugSubTopic[13] = "debug/remote"; // MQTT enable debug remote subtopic
  const char _mqttOnPayload[3] = "on";
  const char _mqttOffPayload[4] = "off";
  const char _mqttSavePayload[5] = "save";
  const char _mqttAnnouncePayload[9] = "announce";

  boolean _mqttSendEnabled = true;                                                                          // MQTT flad indicating, if node specific payloads will be send
  boolean _mqttAvailableMsgPending = false;                                                                 // MQTT flag indicating if availability status is pending
  MQTTAvailableCallback _mqttAvailableCallbacks[CALLBACK_CNT] = {nullptr, nullptr, nullptr, nullptr, nullptr}; // MQTT available callback array to dispatch available behaviour
  MQTTClientCallbackSimple _mqttRcvCallbacks[CALLBACK_CNT] = {nullptr, nullptr, nullptr, nullptr, nullptr}; // MQTT callback array to dispatch received messages

  void _mqttSetup();
  void _mqttConnect();
  bool _mqttSend(String topic, String cmd);
  void _mqttSendAvailableResend();
  void _mqttRcvCallback(String &topic, String &payload);
  void _mqttLoop();
};

#endif