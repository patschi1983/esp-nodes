#include <Arduino.h>
#include <SPI.h>
#include <EspNode.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <MQUnifiedsensor.h>

//***** ESP Node *****//
char nodeName[32] = "sen_rel";        // Nodes name - default value, may be overridden
char fwName[16] = "esp-sen-rel"; // Name of the firmware
char fwVersion[8] = "1.3";            // Version of the firmware

EspNode *espNode;

Adafruit_ADS1115 multiAdc;     // ADC definition
#define MULTI_ADC_LIGHT_PIN 0  // ADC pin where light sensor is connected
#define MULTI_ADC_MQ_PIN 2     // ADC pin where MQ2 is connected
#define MULTI_ADC_MOTION_PIN 1 // ADC pin where motion sensor is connected
#define MULTI_ADC_VOLTAGE 5    // ADC voltage resolution
#define MULTI_ADC_RES 16       // ADC resolution
#define MULTI_RELAY_PIN_0 D5   // Relay pin 0
#define MULTI_RELAY_PIN_1 D6   // Relay pin 1
#define MULTI_RELAY_PIN_2 D7   // Relay pin 2


#define MULTI_MQ_BOARD "ESP8266"        // Board definition
#define MULTI_MQ_TYPE "MQ2/ADS1115"     // MQ-Type definition
#define MULTI_MQ2_RATIO_CLEANAIR (9.83) // MQ2 clean air ratio
#define MULTI_MQ2_WARMUP_SEC 20         // MQ warmup time in sec

bool multiAdcSensorInitialized = false; // Bool holding the initialization state of the adc

MQUnifiedsensor multiMqSensor(MULTI_MQ_BOARD, MULTI_ADC_VOLTAGE, MULTI_ADC_RES, -1, MULTI_MQ_TYPE);
unsigned long multiMqWarmupTimer = 0;    // Timestamp used to measure the warmup time
bool multiMqWarmedUp = false;            // Flag indicating that warmup has been done
bool multiMqCalibrated = false;          // Flag indicating that calibration has been done
String multiMqSensorState = "Initial";   // String holding the current state of the MQ sensor
int multiMqSmokeLimit = 500;             // Smoke limit in ppm - Default value, maybe overridden
bool multiMqSmokePending = true;         // Flag indicating that a smoke message is pending, true to send initial state
bool multiMqSmokeDetected = false;       // Flag indicating if smoke was detected, last read value
unsigned int multiMqSmokeHoldTime = 30;  // Minimum hold time for smoke detection state (sec) - Default value, maybe overridden
unsigned long multiMqSmokeHoldTimer = 0; // Timestamp used to measure the hold time

int16_t multiLightVoltage = 0; // Long holding last read light voltage value
long multiLightPercentage = 0; // Long holding last read light percentage value
long multiLightMinValue = 4;   // Minimum voltage indicating 0% light - Default value, maybe overridden
long multiLightMaxValue = 0;   // Maximum voltage indicating 100% light - Default value, maybe overridden

bool multiMotionPending = true;         // Flag indicating that a motion message is pending, true to send initial state
bool multiMotionDetected = false;       // Flag indicating if smoke was detected, last read value
unsigned int multiMotionHoldTime = 5;   // Minimum hold time for motion detection state (sec) - Default value, maybe overridden
unsigned long multiMotionHoldTimer = 0; // Timestamp used to measure hold time

const char HTML_MULTI_FORM_START[] PROGMEM = "<form method='POST' action='saveMulti'>";
const char HTML_MULTI_ADC_STATUS[] PROGMEM = "<b>ADC Status</b><input id='multiAdcSensorInitialized' readonly name='multiAdcSensorInitialized' placeholder='unknown' value='{multiAdcSensorInitialized}'>";
const char HTML_MULTI_MQ_STATUS[] PROGMEM = "<br/><br/><b>Smoke Sensor Status</b><input id='multiMqSensorState' readonly name='multiMqSensorState' placeholder='unknown' value='{multiMqSensorState}'>";
const char HTML_MULTI_MQ_SMOKE_STATUS[] PROGMEM = "<br/><b>Smoke Status</b><input id='multiMqSmokeDetected' readonly name='multiMqSmokeDetected' placeholder='unknown' value='{multiMqSmokeDetected}'>";
const char HTML_MULTI_MQ_SMOKE_LIMIT[] PROGMEM = "<br/><b>Smoke Limit (ppm)</b> <i><small>(required)</small></i><input id='multiMqSmokeLimit' required name='multiMqSmokeLimit' type='number' maxlength=5 placeholder='500' value='{multiMqSmokeLimit}'>";
const char HTML_MULTI_MQ_HOLDTIME[] PROGMEM = "<br/><b>Smoke Hold Time (sec)</b> <i><small>(required)</small></i><input id='multiMqSmokeHoldTime' required name='multiMqSmokeHoldTime' type='number' maxlength=5 placeholder='30' value='{multiMqSmokeHoldTime}'>";
const char HTML_MULTI_LIGHT_VAL[] PROGMEM = "<br/><br/><b>Light Sensor (v/%)</b><input id='multiLightSensorValue' readonly name='multiLightSensorValue' placeholder='unknown' value='{multiLightSensorValue}'>";
const char HTML_MULTI_LIGHT_MIN_VAL[] PROGMEM = "<br/><b>Light Sensor Min (v)</b><input id='multiLightMinValue' name='multiLightMinValue' type='number' maxlength=5 placeholder='4' value='{multiLightMinValue}'>";
const char HTML_MULTI_LIGHT_MAX_VAL[] PROGMEM = "<br/><b>Light Sensor Max (v)</b><input id='multiLightMaxValue' name='multiLightMaxValue' type='number' maxlength=5 placeholder='0' value='{multiLightMaxValue}'>";
const char HTML_MULTI_MOTION_VAL[] PROGMEM = "<br/><br/><b>Motion Sensor</b><input id='multiMotionDetected' readonly name='multiMotionDetected' placeholder='unknown' value='{multiMotionDetected}'>";
const char HTML_MULTI_MOTION_HOLDTIME[] PROGMEM = "<br/><b>Motion Hold Time (sec)</b> <i><small>(required)</small></i><input id='multiMotionHoldTime' required name='multiMotionHoldTime' type='number' maxlength=5 placeholder='5' value='{multiMotionHoldTime}'>";
const char HTML_MULTI_RELAY_0_STATE[] PROGMEM = "<br/><br/><b>Relay 0</b><input id='multiRelayState0' readonly name='multiRelayState0' placeholder='unknown' value='{multiRelayState}'>";
const char HTML_MULTI_RELAY_1_STATE[] PROGMEM = "<br/><b>Relay 1</b><input id='multiRelayState1' readonly name='multiRelayState1' placeholder='unknown' value='{multiRelayState}'>";
const char HTML_MULTI_RELAY_2_STATE[] PROGMEM = "<br/><b>Relay 2</b><input id='multiRelayState2' readonly name='multiRelayState2' placeholder='unknown' value='{multiRelayState}'>";
const char HTML_MULTI_BTN_SAVE_FORM_END[] PROGMEM = "<br/><br/><button type='submit'>Save</button></form>";
const char HTML_MULTI_BTN_BACK[] PROGMEM = "<hr><a href='/'><button>Back</button></a>";

void multiSetup();
int16_t multiMqReadAdcRaw();
bool multiMqCalibrate();
void multiMqLoop();
void multiLightLoop();
void multiMotionLoop();
void multiLoop();
void multiConfigRead();
void multiConfigSave();
void multiAvailable();
void multiRcvCallback(String &topic, String &payload);
void webHandleMultiSensor();
void webHandleMultiSensorSave();

void multiConfigRead();
void multiConfigSave();

void webHandleMultiSensor();
void webHandleMultiSensorSave();

void multiSetup()
{
  // Load configuration if available
  multiConfigRead();

  Wire.begin(D2, D1);
  multiAdcSensorInitialized = multiAdc.begin();

  if (!multiAdcSensorInitialized)
  {
    espNode->debugPrintln(String(F("MULTI: Failed to initialize ADC.")));
  }

  if (multiAdcSensorInitialized)
  {
    // Setup MQ2 sensor
    multiMqSensor.setRegressionMethod(1); // _PPM =  a*ratio^b
    multiMqSensor.setA(30000000);         // Configure for smoke concentration
    multiMqSensor.setB(-8.308);
    multiMqSensor.init();

    multiMqSensorState = String(F("Pending: Warming up..."));
  }
  else
  {
    multiMqSensorState = String(F("Error: ADC initialization failed."));
  }

  // set digital pin to output
  pinMode(MULTI_RELAY_PIN_0, OUTPUT);
  digitalWrite(MULTI_RELAY_PIN_0, LOW); // init with low
  pinMode(MULTI_RELAY_PIN_1, OUTPUT);
  digitalWrite(MULTI_RELAY_PIN_1, LOW); // init with low
  pinMode(MULTI_RELAY_PIN_2, OUTPUT);
  digitalWrite(MULTI_RELAY_PIN_2, LOW); // init with low

  // Register save callback
  espNode->configSaveAddCallback(multiConfigSave);

  // Register web handles
  espNode->webRegisterHandler("/multi", webHandleMultiSensor);
  espNode->webAddButtonHandler("/multi", "Sensors & Relay");
  espNode->webRegisterHandler("/saveMulti", webHandleMultiSensorSave);

  // Register mqtt callback
  espNode->mqttAvailableAddCallback(multiAvailable);
  espNode->mqttRcvAddCallback(multiRcvCallback);

  delay(1000); // wait for pins to set down
}

int16_t multiMqReadAdcRaw()
{
  return multiAdc.readADC_SingleEnded(MULTI_ADC_MQ_PIN);
}
bool multiMqWarmUp()
{
  if (multiMqWarmedUp)
  {
    return true;
  }

  if (multiAdcSensorInitialized && !multiMqWarmedUp)
  {
    unsigned long secondsPassed = 0;

    if (multiMqWarmupTimer == 0)
    {
      multiMqWarmupTimer = millis();

      espNode->debugPrintln(String(F("MULTI: MQ2 sensor warming up for ")) + String(MULTI_MQ2_WARMUP_SEC) + String(F(" sec...")));
    }
    else
    {
      secondsPassed = (millis() - multiMqWarmupTimer) / 1000;
    }

    multiMqWarmedUp = (secondsPassed >= MULTI_MQ2_WARMUP_SEC);

    if (multiMqWarmedUp)
    {
      espNode->debugPrintln(String(F("MULTI: MQ2 sensor warmed up.")));

      multiMqSensorState = String(F("Pending: Calibrating sensor..."));
    }
  }

  return false;
}

bool multiMqCalibrate()
{
  if (multiMqCalibrated)
  {
    return true;
  }

  if (multiMqWarmUp() && !multiMqCalibrated)
  {
    espNode->debugPrintln("MULTI: MQ sensor calibrating please wait...");

    float calcR0 = 0;
    for (int i = 1; i <= 10; i++)
    {
      multiMqSensor.setADC(multiMqReadAdcRaw());
      calcR0 += multiMqSensor.calibrate(MULTI_MQ2_RATIO_CLEANAIR);
    }
    multiMqSensor.setR0(calcR0 / 10);

    if (isinf(calcR0))
    {
      multiMqSensorState = String(F("Error: Connection issue founded, R0 is infite."));

      espNode->debugPrintln("MULTI: Warning, MQ sensor connection issue founded, R0 is infite (open circuit detected) please check your wiring and supply.");
    }
    else if (calcR0 == 0)
    {
      multiMqSensorState = String(F("Error: Connection issue founded, R0 i zero."));

      espNode->debugPrintln("MULTI: Warning, MQ sensor connection issue founded, R0 is zero (Analog pin with short circuit to ground) please check your wiring and supply");
    }
    else
    {
      multiMqCalibrated = true;
      multiMqSensorState = String(F("Ready..."));

      espNode->debugPrintln("MULTI: MQ sensor calibrated.");
    }
  }

  return false;
}

void multiMqLoop()
{
  // Send pending message if needed
  if (multiMqSmokePending)
  {
    espNode->debugPrintln(String(F("MULTI: MQ sensor sending state ---> ")) + (multiMqSmokeDetected ? String(F("detected")) : String(F("none"))));

    multiMqSmokePending = !espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("smoke"))), multiMqSmokeDetected ? String(F("detected")) : String(F("none")));
  }

  // Return if warm up and calibration are not done yet
  if (!multiMqWarmUp() && !multiMqCalibrate())
  {
    return; // do not start reading if
  }

  // Read current smoke state
  multiMqSensor.setADC(multiMqReadAdcRaw());
  float smokePPM = multiMqSensor.readSensor(); // Sensor will read PPM concentration using the model and a and b values setted before or in the setup
  boolean smokeDetected = (smokePPM >= multiMqSmokeLimit);

  // Smoke was detected, evaluate timer state
  if (smokeDetected)
  {
    if (multiMqSmokeHoldTimer != 0)
    {
      // Timer was already set prior, so just set a new time value
      multiMqSmokeHoldTimer = millis();
    }
    else
    {
      espNode->debugPrintln(String(F("MULTI: Smoke detected --> setting timer.")));

      // Timer should be set for the first time
      multiMqSmokeHoldTimer = millis();
      // Set pending message, if timer is set for the first time and return from function
      multiMqSmokePending = true;
      multiMqSmokeDetected = smokeDetected;

      return;
    }
  }

  // Evaluate if hold time expired
  unsigned long secondsPassed = 0;
  if (multiMqSmokeHoldTimer != 0)
  {
    secondsPassed = (millis() - multiMqSmokeHoldTimer) / 1000;
  }

  if (secondsPassed >= multiMqSmokeHoldTime)
  {
    espNode->debugPrintln(String(F("MULTI: Smoke hold time expired --> setting pending message.")));

    // Set pending message, if hold time expired
    multiMqSmokePending = true;
    multiMqSmokeDetected = smokeDetected;
    // Reset timer for next detected smoke
    multiMqSmokeHoldTimer = 0;
  }
}

void multiLightLoop()
{
  int16_t lightRawValue = multiAdc.readADC_SingleEnded(MULTI_ADC_LIGHT_PIN);
  multiLightVoltage = multiAdc.computeVolts(lightRawValue);

  long lightPercentage = map(multiLightVoltage, multiLightMinValue, multiLightMaxValue, 0, 100);
  lightPercentage = constrain(lightPercentage, 0, 100);

  if (lightPercentage != multiLightPercentage)
  {
    espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("light"))), String(lightPercentage));
  }

  multiLightPercentage = lightPercentage; // Save result and send message if value changes
}

void multiMotionLoop()
{
  // Send pending message if needed
  if (multiMotionPending)
  {
    espNode->debugPrintln(String(F("MULTI: Motion sensor sending state ---> ")) + (multiMotionDetected ? String(F("detected")) : String(F("none"))));

    multiMotionPending = !espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("motion"))), multiMotionDetected ? String(F("detected")) : String(F("none")));
  }

  // Read current motion state
  int16_t motionRawValue = multiAdc.readADC_SingleEnded(MULTI_ADC_MOTION_PIN);
  int16_t motionVoltage = multiAdc.computeVolts(motionRawValue);

  bool motionDetected = (motionVoltage >= 3);

  // Motion was detected, evaluate timer state
  if (motionDetected)
  {
    if (multiMotionHoldTimer != 0)
    {
      // Timer was already set prior, so just set a new time value
      multiMotionHoldTimer = millis();
    }
    else
    {
      espNode->debugPrintln(String(F("MULTI: Motion detected --> setting timer.")));

      // Timer should be set for the first time
      multiMotionHoldTimer = millis();
      // Set pending message, if timer is set for the first time and return from function
      multiMotionPending = true;
      multiMotionDetected = motionDetected;

      return;
    }
  }

  // Evaluate if hold time expired
  unsigned long secondsPassed = 0;
  if (multiMotionHoldTimer != 0)
  {
    secondsPassed = (millis() - multiMotionHoldTimer) / 1000;
  }

  if (secondsPassed >= multiMotionHoldTime)
  {
    espNode->debugPrintln(String(F("MULTI: Motion hold time expired --> setting pending message.")));

    // Set pending message, if hold time expired
    multiMotionPending = true;
    multiMotionDetected = motionDetected;
    // Reset timer for next detected motion
    multiMotionHoldTimer = 0;
  }
}

void multiAvailable()
{
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("light"))), String(multiLightPercentage));
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("smoke"))), multiMqSmokeDetected ? String(F("detected")) : String(F("none")));
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("motion"))), multiMotionDetected ? String(F("detected")) : String(F("none")));
  
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("relay/0"))), espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_0)));
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("relay/1"))), espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_1)));
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("relay/2"))), espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_2)));
}

void multiRcvCallback(String &topic, String &payload)
{
  espNode->debugPrintln(String(F("MULTI: Message arrived on topic: '")) + topic + String(F("' with payload: '")) + payload + String(F("'.")));

  if (topic.equals(espNode->mqttGetNodeCmdTopic(String(F("relay/0")))))
  {
    if (payload.equals(espNode->mqttGetOnOffPayload(true))) {
      digitalWrite(MULTI_RELAY_PIN_0, HIGH);
    }

    if (payload.equals(espNode->mqttGetOnOffPayload(false))) {
      digitalWrite(MULTI_RELAY_PIN_0, LOW);
    }

    if (payload.equals(String(F("toogle")))) {
      digitalWrite(MULTI_RELAY_PIN_0, !digitalRead(MULTI_RELAY_PIN_0));
    }

    multiAvailable();
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(String(F("relay/1")))))
  {
    if (payload.equals(espNode->mqttGetOnOffPayload(true))) {
      digitalWrite(MULTI_RELAY_PIN_1, HIGH);
    }

    if (payload.equals(espNode->mqttGetOnOffPayload(false))) {
      digitalWrite(MULTI_RELAY_PIN_1, LOW);
    }

    if (payload.equals(String(F("toogle")))) {
      digitalWrite(MULTI_RELAY_PIN_1, !digitalRead(MULTI_RELAY_PIN_1));
    }

    multiAvailable();
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(String(F("relay/2")))))
  {
    if (payload.equals(espNode->mqttGetOnOffPayload(true))) {
      digitalWrite(MULTI_RELAY_PIN_2, HIGH);
    }

    if (payload.equals(espNode->mqttGetOnOffPayload(false))) {
      digitalWrite(MULTI_RELAY_PIN_2, LOW);
    }

    if (payload.equals(String(F("toogle")))) {
      digitalWrite(MULTI_RELAY_PIN_2, !digitalRead(MULTI_RELAY_PIN_2));
    }

    multiAvailable();
  }
}

void multiLoop()
{
  multiMqLoop();
  multiLightLoop();
  multiMotionLoop();
}

void multiConfigRead()
{
  // Read saved multiConfig.json from SPIFFS
  espNode->debugPrintln(F("SPIFFS: mounting SPIFFS"));

  File configFile = espNode->configOpenFile("/multiConfig.json", "r");
  if (configFile)
  {

    size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[configFileSize]);
    configFile.readBytes(buf.get(), configFileSize);

    DynamicJsonDocument configJson(CONFIG_SIZE);
    DeserializationError jsonError = deserializeJson(configJson, buf.get());

    if (jsonError)
    { // Couldn't parse the saved config
      bool removedJson = SPIFFS.remove("/multiConfig.json");

      espNode->debugPrintln(String(F("SPIFFS: [ERROR] Failed to parse /multiConfig.json: ")) + String(jsonError.c_str()));

      if (removedJson)
      {
        espNode->debugPrintln(String(F("SPIFFS: Removed corrupt file /multiConfig.json")));
      }
      else
      {
        espNode->debugPrintln(String(F("SPIFFS: [ERROR] Corrupt file /multiConfig.json could not be removed")));
      }
    }
    else
    {
      // Read Multi Sensor configuration
      if (!configJson["multiMqSmokeLimit"].isNull())
      {
        multiMqSmokeLimit = configJson["multiMqSmokeLimit"];
      }
      if (!configJson["multiMqSmokeHoldTime"].isNull())
      {
        multiMqSmokeHoldTime = configJson["multiMqSmokeHoldTime"];
      }
      if (!configJson["multiLightMinValue"].isNull())
      {
        multiLightMinValue = configJson["multiLightMinValue"];
      }
      if (!configJson["multiLightMaxValue"].isNull())
      {
        multiLightMaxValue = configJson["multiLightMaxValue"];
      }
      if (!configJson["multiMotionHoldTime"].isNull())
      {
        multiMotionHoldTime = configJson["multiMotionHoldTime"];
      }

      // Print read JSON configuration
      String configJsonStr;
      serializeJson(configJson, configJsonStr);

      espNode->debugPrintln(String(F("SPIFFS: parsed json:")) + configJsonStr);
    }
  }
  else
  {
    espNode->debugPrintln(F("SPIFFS: [ERROR] File not found /multiConfig.json"));
  }
}

void multiConfigSave()
{ // Save the parameters to config.json
  espNode->debugPrintln(F("SPIFFS: Saving multisensor config"));
  DynamicJsonDocument jsonConfigValues(CONFIG_SIZE);

  // Save multi sensor configuration
  jsonConfigValues["multiMqSmokeLimit"] = multiMqSmokeLimit;
  jsonConfigValues["multiMqSmokeHoldTime"] = multiMqSmokeHoldTime;
  jsonConfigValues["multiLightMinValue"] = multiLightMinValue;
  jsonConfigValues["multiLightMaxValue"] = multiLightMaxValue;
  jsonConfigValues["multiMotionHoldTime"] = multiMotionHoldTime;

  File configFile = espNode->configOpenFile("/multiConfig.json", "w");
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

void webHandleMultiSensor()
{
  espNode->debugPrintln(String(F("HTTP: WebHandleMultiSensor called from client: ")));

  espNode->webStartHttpMsg(String(F("Multi Sensor Relay")), 200);

  espNode->webSendHttpContent(HTML_MULTI_FORM_START);
  espNode->webSendHttpContent(HTML_MULTI_ADC_STATUS, String(F("{multiAdcSensorInitialized}")), (multiAdcSensorInitialized ? String(F("ok")) : String(F("error"))));
  espNode->webSendHttpContent(HTML_MULTI_MQ_STATUS, String(F("{multiMqSensorState}")), multiMqSensorState);

  espNode->webSendHttpContent(HTML_MULTI_MQ_SMOKE_STATUS, String(F("{multiMqSmokeDetected}")), (multiMqSmokeDetected ? String(F("detected")) : String(F("none"))));
  espNode->webSendHttpContent(HTML_MULTI_MQ_SMOKE_LIMIT, String(F("{multiMqSmokeLimit}")), String(multiMqSmokeLimit));
  espNode->webSendHttpContent(HTML_MULTI_MQ_HOLDTIME, String(F("{multiMqSmokeHoldTime}")), String(multiMqSmokeHoldTime));

  String lightSensorValue = String(multiLightVoltage) + String(F("v / ")) + String(multiLightPercentage) + String(F("%"));
  espNode->webSendHttpContent(HTML_MULTI_LIGHT_VAL, String(F("{multiLightSensorValue}")), lightSensorValue);
  espNode->webSendHttpContent(HTML_MULTI_LIGHT_MIN_VAL, String(F("{multiLightMinValue}")), String(multiLightMinValue));
  espNode->webSendHttpContent(HTML_MULTI_LIGHT_MAX_VAL, String(F("{multiLightMaxValue}")), String(multiLightMaxValue));

  espNode->webSendHttpContent(HTML_MULTI_MOTION_VAL, String(F("{multiMotionDetected}")), (multiMotionDetected ? String(F("detected")) : String(F("none"))));
  espNode->webSendHttpContent(HTML_MULTI_MOTION_HOLDTIME, String(F("{multiMotionHoldTime}")), String(multiMotionHoldTime));

  espNode->webSendHttpContent(HTML_MULTI_RELAY_0_STATE,String(F("{multiRelayState}")), String(espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_0))));
  espNode->webSendHttpContent(HTML_MULTI_RELAY_1_STATE,String(F("{multiRelayState}")), String(espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_1))));
  espNode->webSendHttpContent(HTML_MULTI_RELAY_2_STATE,String(F("{multiRelayState}")), String(espNode->mqttGetOnOffPayload(digitalRead(MULTI_RELAY_PIN_2))));


  espNode->webSendHttpContent(HTML_MULTI_BTN_SAVE_FORM_END);
  espNode->webSendHttpContent(HTML_MULTI_BTN_BACK);

  espNode->webEndHttpMsg();

  espNode->debugPrintln(String(F("HTTP: WebHandleMultiSensor page sent.")));
}

void webHandleMultiSensorSave()
{
  espNode->debugPrintln(String(F("HTTP: webHandleMultiSensorSave called from client: ")));
  espNode->debugPrintln(String(F("HTTP: Checking for changed settings...")));

  // check if multi sensor settings have changed
  multiMqSmokeLimit = atoi(espNode->webGetArg(String(F("multiMqSmokeLimit"))).c_str());
  multiMqSmokeHoldTime = atoi(espNode->webGetArg(String(F("multiMqSmokeHoldTime"))).c_str());
  multiLightMinValue = atoi(espNode->webGetArg(String(F("multiLightMinValue"))).c_str());
  multiLightMaxValue = atoi(espNode->webGetArg(String(F("multiLightMaxValue"))).c_str());
  multiMotionHoldTime = atoi(espNode->webGetArg(String(F("multiMotionHoldTime"))).c_str());

  espNode->debugPrintln(String(F("HTTP: Sending /saveMulti page to client")));
  espNode->webStartHttpMsg(String(F("")), HTML_SAVESETTINGS_START_REDIR_3SEC, 200, String(F("/multi")));
  espNode->webSendHttpContent(HTML_SAVESETTINGS_NOCHANGE, HTML_REPLACE_REDIRURL, String(F("/multi")));
  espNode->webEndHttpMsg();

  multiConfigSave();
}

void setup()
{
  espNode = new EspNode(nodeName, fwName, fwVersion);
  espNode->setup();

  multiSetup();
}

void loop()
{
  espNode->loop();

  multiLoop();
}