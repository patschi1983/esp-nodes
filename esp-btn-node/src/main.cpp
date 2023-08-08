#include <Arduino.h>
#include <EspNode.h>
#include <OneButton.h>

//***** ESP Node *****//
char nodeName[32] = "btn";      // Nodes name - default value, may be overridden
char fwName[16] = "esp-btn"; // Name of the firmware
char fwVersion[8] = "1.0";           // Version of the firmware

EspNode *espNode;

#define BTN_TYPE_SINGLE 1
#define BTN_TYPE_DOUBLE 2
#define BTN_TYPE_MULTI 3
#define BTN_TYPE_LONG 4
#define BTN_CMD_1X "Cmd1x"
#define BTN_CMD_2X "Cmd2x"
#define BTN_CMD_MU "CmdMu"
#define BTN_CMD_LO "CmdLo"

const char HTML_BUTTONS_FORM_START[] PROGMEM = "<form method='POST' action='saveButtons'>";
const char HTML_BUTTONS_SECTION_START[] PROGMEM = "<h3>{buttonName}</h3>";
const char HTML_BUTTONS_CMD_1X[] PROGMEM = "<b>Command Single</b> <i><small>(disabled, if empty)</small></i>";
const char HTML_BUTTONS_CMD_2X[] PROGMEM = "<br/><b>Command Double</b> <i><small>(disabled, if empty)</small></i>";
const char HTML_BUTTONS_CMD_MULTI[] PROGMEM = "<br/><b>Command Multi</b> <i><small>(disabled, if empty)</small></i>";
const char HTML_BUTTONS_CMD_LONG[] PROGMEM = "<br/><b>Command Long</b> <i><small>(disabled, if empty)</small></i>";
const char HTML_BUTTONS_CMD_MQTT[] PROGMEM = "<input id='{btnCfgId}' name='{btnCfgId}' maxlength=127 placeholder='{btnDefaultCmd}' value='{btnCmd}'>";
const char HTML_BUTTONS_FORM_END[] PROGMEM = "<br/><br/><button type='submit'>Save</button></form>";
const char HTML_BUTTONS_BTN_BACK[] PROGMEM = "<hr><a href='/'><button>Back</button></a>";

int btnCurrentIndex = 0; // Control variable for loop

const char BTN_CMD_SEPERATOR[2] = "#";

#ifdef ESP8266
const uint16_t MAX_NUM_OF_BUTTONS = 8; // max is 8
const uint16_t NUM_OF_BUTTONS_USED = 4;  // 4 button usage
int btnId[MAX_NUM_OF_BUTTONS] = {0, 1, 2, 3, 4, 5, 6, 7};
const char btnName[MAX_NUM_OF_BUTTONS][16] = {"btnD0", "btnD1", "btnD2", "btnD5", "btnD6", "btnD7", "btnD8", "btnA0"};
char btnMqttCmdSingle[MAX_NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", ""};
char btnMqttCmdDouble[MAX_NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", ""};
char btnMqttCmdMulti[MAX_NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", ""};
char btnMqttCmdLong[MAX_NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", ""};
int btnPins[MAX_NUM_OF_BUTTONS] = {D0, D1, D2, D5, D6, D7, D8, A0};
OneButton *btnArray[MAX_NUM_OF_BUTTONS];
#elif ESP32
const uint16_t MAX_NUM_OF_BUTTONS = 20; // max is 20
const uint16_t NUM_OF_BUTTONS_USED = 10;
int btnId[NUM_OF_BUTTONS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
const char btnName[NUM_OF_BUTTONS][16] = {"btn05", "btn13", "btn14", "btn15", "btn16", "btn17", "btn18", "btn19", "btn21", "btn22", "btn23", "btn25", "btn26", "btn27", "btn32", "btn33", "btn34", "btn35", "btn36", "btn39"};
char btnMqttCmdSingle[NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
char btnMqttCmdDouble[NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
char btnMqttCmdMulti[NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
char btnMqttCmdLong[NUM_OF_BUTTONS][128] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
int btnPins[NUM_OF_BUTTONS] = {5, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
OneButton *btnArray[NUM_OF_BUTTONS];
#else
#error "Wrong board - ESP8266 or ESP32 must be used."
#endif

void btnSetup();

String btnSplitMqttCmd(String data, bool command);
void btnSingleClick(void *btnIndex);
void btnDoubleClick(void *btnIndex);
void btnMultiClick(void *btnIndex);
void btnLongPressStart(void *btnIndex);
void btnLoop();

String btnGetCmdTypeId(int index, int type);
String btnGetConfigId(int index, int type);
String btnGetMqttCmd(int index, int type, bool defaultIfEmpty);
String btnGetDefaultMqttCmd(int index, int type);
String btnGetBtnHtmlMqttCmd(int index, int type);

void btnConfigRead();
void btnConfigSave();

void webHandleButtons();
void webHandleSaveButtons();

void btnSetup()
{
  // Load button config if available
  btnConfigRead();

  // Attach button handlers
  for (int btnIndex = 0; btnIndex < NUM_OF_BUTTONS_USED; btnIndex++)
  {
    btnArray[btnIndex] = new OneButton(btnPins[btnIndex], false, false);

    btnArray[btnIndex]->attachClick(btnSingleClick, &btnId[btnIndex]);
    btnArray[btnIndex]->attachDoubleClick(btnDoubleClick, &btnId[btnIndex]);
    btnArray[btnIndex]->attachMultiClick(btnMultiClick, &btnId[btnIndex]);

    btnArray[btnIndex]->attachLongPressStart(btnLongPressStart, &btnId[btnIndex]);

    espNode->debugPrintln(String(F("** BTN: Initialized - ")) + btnName[btnIndex] + String(F("|")) + btnPins[btnIndex]);
  }

  // Register save callback
  espNode->configSaveAddCallback(btnConfigSave);

  // Register web handles
  espNode->webRegisterHandler("/buttons", webHandleButtons);
  espNode->webAddButtonHandler("/buttons", "Buttons");
  espNode->webRegisterHandler("/saveButtons", webHandleSaveButtons);

  delay(1000); // wait for pins to set down
}

String btnSplitMqttCmd(String data, bool command)
{
  int delimiter = data.indexOf(BTN_CMD_SEPERATOR);

  if (delimiter > 0)
  {
    if (!command)
    {
      return data.substring(0, delimiter);
    }
    else
    {
      return data.substring(delimiter + 1);
    }
  }

  return "";
}

void btnSingleClick(void *btnIndex)
{
  espNode->debugPrintln(String(F("BTN: Button ")) + *(int *)btnIndex + String(F(" single click.")));

  int index = *(int *)btnIndex;
  String mqttCmd = btnGetMqttCmd(index, BTN_TYPE_SINGLE, true);

  String topic = btnSplitMqttCmd(mqttCmd, false);
  String cmd = btnSplitMqttCmd(mqttCmd, true);

  if (!topic.isEmpty() && !cmd.isEmpty())
  {
    espNode->mqttSend(topic, cmd);
    espNode->debugPrintln(String(F("** BTN: Send MQTT[Topic|Cmd] ")) + topic + String(F("|")) + cmd);
  }
  else
  {
    espNode->debugPrintln(String(F("** BTN: No MQTT[Topic|Cmd] for singleclick defined [")) + mqttCmd + String(F("]")));
  }

  btnArray[index]->reset();
}

void btnDoubleClick(void *btnIndex)
{
  espNode->debugPrintln(String(F("BTN: Button ")) + *(int *)btnIndex + String(F(" double click.")));

  int index = *(int *)btnIndex;
  String mqttCmd = btnGetMqttCmd(index, BTN_TYPE_DOUBLE, true);

  String topic = btnSplitMqttCmd(mqttCmd, false);
  String cmd = btnSplitMqttCmd(mqttCmd, true);

  if (!topic.isEmpty() && !cmd.isEmpty())
  {
    espNode->mqttSend(topic, cmd);
    espNode->debugPrintln(String(F("** BTN: Send MQTT[Topic|Cmd] ")) + topic + String(F("|")) + cmd);
  }
  else
  {
    espNode->debugPrintln(String(F("** BTN: No MQTT[Topic|Cmd] for doubleclick defined [")) + mqttCmd + String(F("]")));
  }

  btnArray[index]->reset();
}

void btnMultiClick(void *btnIndex)
{
  espNode->debugPrintln(String(F("BTN: Button ")) + *(int *)btnIndex + String(F(" multi click.")));

  int index = *(int *)btnIndex;
  String mqttCmd = btnGetMqttCmd(index, BTN_TYPE_MULTI, true);

  String topic = btnSplitMqttCmd(mqttCmd, false);
  String cmd = btnSplitMqttCmd(mqttCmd, true);

  if (!topic.isEmpty() && !cmd.isEmpty())
  {
    espNode->mqttSend(topic, cmd);
    espNode->debugPrintln(String(F("** BTN: Send MQTT[Topic|Cmd] ")) + topic + String(F("|")) + cmd);
  }
  else
  {
    espNode->debugPrintln(String(F("** BTN: No MQTT[Topic|Cmd] for multiclick defined [")) + mqttCmd + String(F("]")));
  }

  btnArray[index]->reset();
}

void btnLongPressStart(void *btnIndex)
{
  espNode->debugPrintln(String(F("BTN: Button ")) + *(int *)btnIndex + String(F(" long click.")));

  int index = *(int *)btnIndex;
  String mqttCmd = btnGetMqttCmd(index, BTN_TYPE_LONG, true);

  String topic = btnSplitMqttCmd(mqttCmd, false);
  String cmd = btnSplitMqttCmd(mqttCmd, true);

  if (!topic.isEmpty() && !cmd.isEmpty())
  {
    espNode->mqttSend(topic, cmd);
    espNode->debugPrintln(String(F("** BTN: Send MQTT[Topic|Cmd] ")) + topic + String(F("|")) + cmd);
  }
  else
  {
    espNode->debugPrintln(String(F("** BTN: No MQTT[Topic|Cmd] for longclick defined [")) + mqttCmd + String(F("]")));
  }

  btnArray[index]->reset();
}

void btnLoop()
{
#ifdef ESP8266
  if (btnPins[btnCurrentIndex] == A0)
  {
    btnArray[btnCurrentIndex]->tick((analogRead(A0) > 100));
  }
  else
  {
    btnArray[btnCurrentIndex]->tick();
  }
#else
  btnArray[btnCurrentIndex]->tick();
#endif

  // If button is idle (no click received), skip to the next button - else stay with the button
  if (btnArray[btnCurrentIndex]->isIdle())
  {
    btnCurrentIndex++;
  }

  if (btnCurrentIndex >= NUM_OF_BUTTONS_USED)
  {
    btnCurrentIndex = 0;
  }
}

String btnGetCmdTypeId(int index, int type)
{
  switch (type)
  {
  case BTN_TYPE_SINGLE:
    return BTN_CMD_1X;
  case BTN_TYPE_DOUBLE:
    return BTN_CMD_2X;
  case BTN_TYPE_MULTI:
    return BTN_CMD_MU;
  case BTN_TYPE_LONG:
    return BTN_CMD_LO;
  }

  return String(F("CmdTypeUnknown"));
}

String btnGetConfigId(int index, int type)
{
  String configId = btnName[index];
  configId += btnGetCmdTypeId(index, type);

  return configId;
}

String btnGetMqttCmd(int index, int type, bool defaultIfEmpty)
{
  String mqttCmd = "";

  switch (type)
  {
  case BTN_TYPE_SINGLE:
    mqttCmd = btnMqttCmdSingle[index];
    break;
  case BTN_TYPE_DOUBLE:
    mqttCmd = btnMqttCmdDouble[index];
    break;
  case BTN_TYPE_MULTI:
    mqttCmd = btnMqttCmdMulti[index];
    break;
  case BTN_TYPE_LONG:
    mqttCmd = btnMqttCmdLong[index];
    break;
  }

  if (defaultIfEmpty && mqttCmd.isEmpty())
  {
    mqttCmd = btnGetDefaultMqttCmd(index, type);
  }

  return mqttCmd;
}

String btnGetDefaultMqttCmd(int index, int type)
{
  String mqttCmd = espNode->mqttGetNodeTopic(btnName[index]);
  mqttCmd += BTN_CMD_SEPERATOR + btnGetCmdTypeId(index, type);

  return mqttCmd;
}

String btnGetBtnHtmlMqttCmd(int index, int type)
{
  String btnConfigId = btnGetConfigId(index, type);

  String htmlMsg = HTML_BUTTONS_CMD_MQTT;
  htmlMsg.replace(String(F("{btnCfgId}")), btnConfigId);
  htmlMsg.replace(String(F("{btnDefaultCmd}")), btnGetDefaultMqttCmd(index, type));
  htmlMsg.replace(String(F("{btnCmd}")), btnGetMqttCmd(index, type, false));

  return htmlMsg;
}

void btnConfigRead()
{
  // Read saved multiConfig.json from SPIFFS
  espNode->debugPrintln(F("SPIFFS: Reading button config - /buttonConfig.json."));

  File configFile = espNode->configOpenFile("/buttonConfig.json", "r");
  if (configFile)
  {
    size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[configFileSize]);
    configFile.readBytes(buf.get(), configFileSize);

    DynamicJsonDocument configJson(CONFIG_SIZE);
    DeserializationError jsonError = deserializeJson(configJson, buf.get());

    if (jsonError)
    { // Couldn't parse the saved config
      bool removedJson = SPIFFS.remove("/buttonConfig.json");

      espNode->debugPrintln(String(F("SPIFFS: [ERROR] Failed to parse /buttonConfig.json: ")) + String(jsonError.c_str()));

      if (removedJson)
      {
        espNode->debugPrintln(String(F("SPIFFS: Removed corrupt file /buttonConfig.json")));
      }
      else
      {
        espNode->debugPrintln(String(F("SPIFFS: [ERROR] Corrupt file /buttonConfig.json could not be removed")));
      }
    }
    else
    {
      // Read Button configuration
      for (int index = 0; index < MAX_NUM_OF_BUTTONS; index++)
      {
        strcpy(btnMqttCmdSingle[index], configJson[btnGetConfigId(index, BTN_TYPE_SINGLE)]);
        strcpy(btnMqttCmdDouble[index], configJson[btnGetConfigId(index, BTN_TYPE_DOUBLE)]);
        strcpy(btnMqttCmdMulti[index], configJson[btnGetConfigId(index, BTN_TYPE_MULTI)]);
        strcpy(btnMqttCmdLong[index], configJson[btnGetConfigId(index, BTN_TYPE_LONG)]);
      }

      // Print read JSON configuration
      String configJsonStr;
      serializeJson(configJson, configJsonStr);

      espNode->debugPrintln(String(F("SPIFFS: parsed json:")) + configJsonStr);
    }
  }
  else
  {
    espNode->debugPrintln(F("SPIFFS: [ERROR] File not found /buttonConfig.json"));
  }
}

void btnConfigSave()
{ // Save the parameters to config.json
  espNode->debugPrintln(F("SPIFFS: Saving button config"));
  DynamicJsonDocument jsonConfigValues(CONFIG_SIZE);

  // Save button configuration
  for (int index = 0; index < MAX_NUM_OF_BUTTONS; index++)
  {
    jsonConfigValues[btnGetConfigId(index, BTN_TYPE_SINGLE)] = btnMqttCmdSingle[index];
    jsonConfigValues[btnGetConfigId(index, BTN_TYPE_DOUBLE)] = btnMqttCmdDouble[index];
    jsonConfigValues[btnGetConfigId(index, BTN_TYPE_MULTI)] = btnMqttCmdMulti[index];
    jsonConfigValues[btnGetConfigId(index, BTN_TYPE_LONG)] = btnMqttCmdLong[index];
  }

  File configFile = espNode->configOpenFile("/buttonConfig.json", "w");
  if (!configFile)
  {
    espNode->debugPrintln(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();

    // Print saved JSON configuration
    String configJsonStr;
    serializeJson(jsonConfigValues, configJsonStr);
    espNode->debugPrintln(String(F("SPIFFS: saved json:")) + configJsonStr);
  }

  delay(500);
}

void webHandleButtons()
{
  espNode->debugPrintln(String(F("HTTP: WebHandleButtons called.")));

  espNode->webStartHttpMsg(String(F("Buttons")), 200);
  espNode->webSendHttpContent(HTML_BUTTONS_FORM_START);

  String configButtonId;
  for (int index = 0; index < NUM_OF_BUTTONS_USED; index++)
  {
    // Prepare and send a html part for each button
    espNode->webSendHttpContent(HTML_BUTTONS_SECTION_START, String(F("{buttonName}")), btnName[index]);

    espNode->webSendHttpContent(HTML_BUTTONS_CMD_1X);
    espNode->webSendHttpContent(btnGetBtnHtmlMqttCmd(index, BTN_TYPE_SINGLE));

    espNode->webSendHttpContent(HTML_BUTTONS_CMD_2X);
    espNode->webSendHttpContent(btnGetBtnHtmlMqttCmd(index, BTN_TYPE_DOUBLE));

    espNode->webSendHttpContent(HTML_BUTTONS_CMD_MULTI);
    espNode->webSendHttpContent(btnGetBtnHtmlMqttCmd(index, BTN_TYPE_MULTI));

    espNode->webSendHttpContent(HTML_BUTTONS_CMD_LONG);
    espNode->webSendHttpContent(btnGetBtnHtmlMqttCmd(index, BTN_TYPE_LONG));
  }

  espNode->webSendHttpContent(HTML_BUTTONS_FORM_END);
  espNode->webSendHttpContent(HTML_BUTTONS_BTN_BACK);

  espNode->webEndHttpMsg();
}

void webHandleSaveButtons()
{
  espNode->debugPrintln(String(F("HTTP: WebHandleSaveButtons called.")));

  // check if button settings have changed
  String data = "";
  for (int index = 0; index < NUM_OF_BUTTONS_USED; index++)
  {

    data = espNode->webGetArg(btnGetConfigId(index, BTN_TYPE_SINGLE));
    data.trim();
    strcpy(btnMqttCmdSingle[index], data.c_str());

    data = espNode->webGetArg(btnGetConfigId(index, BTN_TYPE_DOUBLE));
    data.trim();
    strcpy(btnMqttCmdDouble[index], data.c_str());

    data = espNode->webGetArg(btnGetConfigId(index, BTN_TYPE_MULTI));
    data.trim();
    strcpy(btnMqttCmdMulti[index], data.c_str());

    data = espNode->webGetArg(btnGetConfigId(index, BTN_TYPE_LONG));
    data.trim();
    strcpy(btnMqttCmdLong[index], data.c_str());
  }

  // Config updated, notify user and trigger write of configurations
  espNode->debugPrintln(String(F("HTTP: Sending /saveButtons page to client.")));
  espNode->webStartHttpMsg(String(F("")), HTML_SAVESETTINGS_START_REDIR_15SEC, 200, String(F("/buttons")));
  espNode->webSendHttpContent(HTML_SAVESETTINGS_SAVE_NORESTART, HTML_REPLACE_REDIRURL, String(F("/buttons")));
  espNode->webEndHttpMsg();

  btnConfigSave();
}

void setup()
{
  espNode = new EspNode(nodeName, fwName, fwVersion);
  espNode->setup();

  btnSetup();
}

void loop()
{
  espNode->loop();

  btnLoop();
}