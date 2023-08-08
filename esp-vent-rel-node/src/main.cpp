#include <Arduino.h>
#include <EspNode.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <MQTTClient.h>
#include <HTTPUpdateServer.h>
#include <LiquidCrystal_I2C.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

//***** ESP Node *****//
char nodeName[32] = "vent_rel";   // Nodes name - default value, may be overridden
char fwName[16] = "esp-vent-rel"; // Name of the firmware
char fwVersion[8] = "1.0";        // Version of the firmware

EspNode *espNode;

//***** DHT Sensors *****//
// Uncomment the type of sensor in use.
// #define DHTTYPE    DHT11     // DHT 11
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)
#define DHTTYPE DHT22 // DHT 22 (AM2302)

#define DHT_S1_PIN 19
#define DHT_S2_PIN 23

DHT_Unified dhtSensor1(DHT_S1_PIN, DHTTYPE);
uint32_t dhtSensor1Delay = 0;
unsigned long dhtSensor1Millis = 0;
int dhtSensor1Temp = 0;
int dhtSensor1Humidity = 0;
bool dhtSensor1Error = false;

DHT_Unified dhtSensor2(DHT_S2_PIN, DHTTYPE);
uint32_t dhtSensor2Delay = 0;
unsigned long dhtSensor2Millis = 0;
int dhtSensor2Temp = 0;
int dhtSensor2Humidity = 0;
bool dhtSensor2Error = false;

void dhtSetup();
void dhtLoop();

//***** Ventilation *****//

#define VENT_STATE_PIN 17        // LOW = ON - HIGH = OFF
#define VENT_SPEED_PIN 16        // NONE = 0, HALF=128, FULL=256
#define VENT_DIR_PIN 18          // LOW = INPUT/SUCK - HIGH = OUTPUT/BLOW
#define VENT_FLAPS_MOVE_PIN 27   // LOW = MOVE_FLAPS (BOTH) - HIGH = OFF
#define VENT_FLAP_SUCKIN_PIN 26  // LOW = OPEN - HIGH = CLOSE  - INPUT/SUCK
#define VENT_FLAP_BLOWOUT_PIN 25 // LOW = OPEN - HIGH = CLOSE  - OUTPUT/BLOW

enum ventState
{
  on = LOW,  // vent on
  off = HIGH // vent off
};
const String ventStateOnText = String(F("on"));
const String ventStateOffText = String(F("off"));

enum ventSpeed
{
  none = 0,
  low  = 64,
  mid = 128,
  high = 254
};
const String ventSpeedNoneText = String(F("0"));
const String ventSpeedLowText = String(F("1"));
const String ventSpeedMidText = String(F("2"));
const String ventSpeedHighText = String(F("3"));

ventSpeed ventSpeedCurrent = none; 


enum ventMode
{
  unknown = 0,
  suck_in = 1,  // suck in
  blow_out = 2, // blow out
  pending = 3   // mode is set up/switched
};
const String ventModeUnknownText = String(F("unknown"));
const String ventModeSuckInText = String(F("suck_in"));
const String ventModeBlowOutText = String(F("blow_out"));
const String ventModePendingText = String(F("pending"));

bool ventSendError = false;
ventMode ventModeWanted = unknown;  // wanted is set to unknown at first
ventMode ventModeCurrent = unknown; // at boot-up the current mode is unknown
unsigned long ventModeTimerMillis = 0;
const unsigned long long ventModeTimout = 90000;

void ventSetup();

void ventSetState(ventState state, bool silent = false);
ventState ventGetState();
String ventGetStateText();
ventState ventGetStateFromText(String payload);

void ventSetSpeed(ventSpeed speed, bool silent = false);
ventSpeed ventGetSpeed();
String ventGetSpeedText();
ventSpeed ventGetSpeedFromText(String payload);

void ventSetMode(ventMode mode, bool silent = false);
ventMode ventGetMode();
String ventGetModeText();
ventMode ventGetModeFromText(String payload);
void ventRefreshMode();
void ventSendStatus(bool force);

void ventLoop();

//***** Relays *****//
#define VENTREL_RELAY_PIN_0 32   // Relay pin 0
#define VENTREL_RELAY_PIN_1 33   // Relay pin 1

//***** MQTT & Web *****//

void ventRelRcvCallback(String &topic, String &payload);
void ventRelAvailable();

void webHandleVentRelay();

const char HTML_VENTREL_STATE[] PROGMEM = "<b>Vent State</b><input id='ventState' readonly name='ventState' placeholder='unknown' value='{ventState}'>";
const char HTML_VENTREL_SPEED[] PROGMEM = "<br/><b>Vent Speed</b><input id='ventSpeed' readonly name='ventSpeed' placeholder='unknown' value='{ventSpeed}'>";
const char HTML_VENTREL_MODE[] PROGMEM = "<br/><b>Vent Mode</b><input id='ventMode' readonly name='ventMode' placeholder='unknown' value='{ventMode}'>";
const char HTML_VENTREL_HUM_1[] PROGMEM = "<br/><br/><b>Humidity_1</b><input id='ventHum1' readonly name='ventHum1' type='number'placeholder='-1' value='{ventHum}'>";
const char HTML_VENTREL_TEMP_1[] PROGMEM = "<br/><b>Temperature_1</b><input id='ventTemp1' readonly name='ventTemp1' type='number' placeholder='-1' value='{ventTemp}'>";
const char HTML_VENTREL_HUM_2[] PROGMEM = "<br/><br/><b>Humidity_1</b><input id='ventHum2' readonly name='ventHum2' type='number'placeholder='-1' value='{ventHum}'>";
const char HTML_VENTREL_TEMP_2[] PROGMEM = "<br/><b>Temperature_1</b><input id='ventTemp2' readonly name='ventTemp2' type='number' placeholder='-1' value='{ventTemp}'>";
const char HTML_VENTREL_RELAY_0_STATE[] PROGMEM = "<br/><br/><b>Relay 0</b><input id='ventRelayState0' readonly name='ventRelayState0' placeholder='unknown' value='{ventRelayState}'>";
const char HTML_VENTREL_RELAY_1_STATE[] PROGMEM = "<br/><b>Relay 1</b><input id='ventRelayState1' readonly name='ventRelayState1' placeholder='unknown' value='{ventRelayState}'>";
const char HTML_VENTREL_BTN_BACK[] PROGMEM = "<hr><a href='/'><button>Back</button></a>";

void setup()
{
  espNode = new EspNode(nodeName, fwName, fwVersion);
  espNode->setup();

  dhtSetup();

  ventSetup();

  // set digital pin to output
  pinMode(VENTREL_RELAY_PIN_0, OUTPUT);
  digitalWrite(VENTREL_RELAY_PIN_0, HIGH); // init with HIGH - relay active low
  pinMode(VENTREL_RELAY_PIN_1, OUTPUT);
  digitalWrite(VENTREL_RELAY_PIN_1, HIGH); // init with HIGH - relay active low

  // Register mqtt callback
  espNode->mqttRcvAddCallback(ventRelRcvCallback);
  espNode->mqttAvailableAddCallback(ventRelAvailable);

  // Register web handles
  espNode->webRegisterHandler("/ventRel", webHandleVentRelay);
  espNode->webAddButtonHandler("/ventRel", "Ventilation & Relay");
}

void loop()
{
  espNode->loop();

  dhtLoop();

  ventLoop();
}

void dhtSetupSensor(DHT_Unified *dht, String sensorText, uint32_t *sensorDelay, unsigned long *sensorMillis, boolean *sensorError)
{
  dht->begin();
  espNode->debugPrintln(String(F("DHT: Setting up sensor ")) + sensorText + String(F("...")));
  sensor_t sensor;

  // Print temperature sensor details.
  dht->temperature().getSensor(&sensor);
  espNode->debugPrintln(String(F(" * Temperature Sensor")));
  espNode->debugPrintln(String(F(" ** Sensor Type: ")) + String(sensor.name));
  espNode->debugPrintln(String(F(" ** Driver Ver:  ")) + String(sensor.version));
  espNode->debugPrintln(String(F(" ** Unique ID:   ")) + String(sensor.sensor_id));
  espNode->debugPrintln(String(F(" ** Max Value:   ")) + String(sensor.max_value) + String(F("°C")));
  espNode->debugPrintln(String(F(" ** Min Value:   ")) + String(sensor.min_value) + String(F("°C")));
  espNode->debugPrintln(String(F(" ** Resolution:  ")) + String(sensor.resolution));

  // Print humidity sensor details.
  dht->humidity().getSensor(&sensor);
  espNode->debugPrintln(String(F(" * Humidity Sensor")));
  espNode->debugPrintln(String(F(" ** Sensor Type: ")) + String(sensor.name));
  espNode->debugPrintln(String(F(" ** Driver Ver:  ")) + String(sensor.version));
  espNode->debugPrintln(String(F(" ** Unique ID:   ")) + String(sensor.sensor_id));
  espNode->debugPrintln(String(F(" ** Max Value:   ")) + String(sensor.max_value) + String(F("%")));
  espNode->debugPrintln(String(F(" ** Min Value:   ")) + String(sensor.min_value) + String(F("%")));
  espNode->debugPrintln(String(F(" ** Resolution:  ")) + String(sensor.resolution));

  // Save delay between sensor readings based on sensor details.
  uint32_t delayValue = sensor.min_delay / 1000;
  *sensorDelay = delayValue;

  // Setup millis for reading
  *sensorMillis = millis();

  espNode->debugPrintln(String(F("DHT: Sensor ")) + sensorText + String(F("has been setup, delay ")) + (String)*sensorMillis + String(F(" was read.")));

  *sensorError = false;
}

void dhtReadSensor(DHT_Unified *dht, String sensorText, uint32_t *sensorDelay, unsigned long *sensorMillis, boolean *sensorError, int *sensorTemp, int *sensorHumidity)
{
  // Check the delay
  unsigned long millisPassed = millis() - *sensorMillis;
  if (millisPassed < *sensorDelay)
  {
    return;
  }

  // Get temperature event and print its value.
  sensors_event_t event;
  dht->temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    *sensorError = true;
    espNode->debugPrintln(String(F(" * DHT: ")) + sensorText + String(F("error reading temperature!")));
  }
  else
  {
    *sensorError = false;

    bool changed = (*sensorTemp != int(event.temperature));
    *sensorTemp = event.temperature;

    if (changed)
    {
      espNode->debugPrintln(String(F(" * DHT: ")) + sensorText + String(F(" temperature read - ")) + String(*sensorTemp) + String(F("°C")));
      espNode->mqttSend(espNode->mqttGetNodeTopic(sensorText + String(F("/temperature"))), String(*sensorTemp));
    }
  }

  // Get humidity event and print its value.
  dht->humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    *sensorError = true;
    espNode->debugPrintln(String(F(" * DHT: ")) + sensorText + String(F("error reading humidity!")));
  }
  else
  {
    *sensorError = false;

    bool changed = (*sensorHumidity != int(event.relative_humidity));
    *sensorHumidity = event.relative_humidity;

    if (changed)
    {
      espNode->debugPrintln(String(F(" * DHT: ")) + sensorText + String(F(" humidity read - ")) + String(*sensorHumidity) + String(F("%")));
      espNode->mqttSend(espNode->mqttGetNodeTopic(sensorText + String(F("/humidity"))), String(*sensorHumidity));
    }
  }
}

void dhtSetup()
{
  dhtSetupSensor(&dhtSensor1, F("sensor1"), &dhtSensor1Delay, &dhtSensor1Millis, &dhtSensor1Error);
  dhtSetupSensor(&dhtSensor2, F("sensor2"), &dhtSensor2Delay, &dhtSensor2Millis, &dhtSensor2Error);
}

void dhtLoop()
{
  dhtReadSensor(&dhtSensor1, F("sensor1"), &dhtSensor1Delay, &dhtSensor1Millis, &dhtSensor1Error, &dhtSensor1Temp, &dhtSensor1Humidity);
  dhtReadSensor(&dhtSensor2, F("sensor2"), &dhtSensor2Delay, &dhtSensor2Millis, &dhtSensor2Error, &dhtSensor2Temp, &dhtSensor2Humidity);
}

void ventSetup()
{
  // Set up pins for fan control
  pinMode(VENT_STATE_PIN, OUTPUT);
  digitalWrite(VENT_STATE_PIN, HIGH); // init with high
  ledcSetup(0, 250, 8);               // channel 0, frequency 500 MHz, resolution 8bit (0-255) //TODO 256???
  ledcAttachPin(VENT_SPEED_PIN, 0);
  pinMode(VENT_DIR_PIN, OUTPUT);
  digitalWrite(VENT_DIR_PIN, HIGH); // init with high - inactive relay mode
  pinMode(VENT_FLAPS_MOVE_PIN, OUTPUT);
  digitalWrite(VENT_FLAPS_MOVE_PIN, HIGH); // init with high
  pinMode(VENT_FLAP_SUCKIN_PIN, OUTPUT);
  digitalWrite(VENT_FLAP_SUCKIN_PIN, HIGH); // init with high
  pinMode(VENT_FLAP_BLOWOUT_PIN, OUTPUT);
  digitalWrite(VENT_FLAP_BLOWOUT_PIN, HIGH); // init with high

  ventSetState(off, true);
  ventSetSpeed(low, true);
  ventSetMode(suck_in, true); // setup to suck mode on start up - air will flow through the filter into the room
  ventSendStatus(true);
}

void ventSetSpeed(ventSpeed speed, bool silent)
{
  ventSpeedCurrent = speed;
  ledcWrite(0, ventSpeedCurrent);

  if (ventSpeedCurrent == none)
  {
    ventSetState(off, silent); // turn of if speed is none
  }

  espNode->debugPrintln(String(F("VENT: Set speed to pwm/")) + String(ventSpeedCurrent));

  if (!silent) // send change silence is false
  {
    ventSendStatus(true);
  }
}

ventSpeed ventGetSpeed()
{
  espNode->debugPrintln(String(F("VENT: Read speed pwm/")) + String(ventSpeedCurrent));

  return static_cast<ventSpeed>(ventSpeedCurrent);
}

String ventGetSpeedText()
{
  ventSpeed speedVal = ventGetSpeed();

  switch (speedVal)
  {
  case none:
    return ventSpeedNoneText;
  case low:
    return ventSpeedLowText;
  case mid:
    return ventSpeedMidText;
  case high:
    return ventSpeedHighText;

  default:
    espNode->debugPrintln(String(F("VENT: Unknown speed '")) + String(speedVal) + String(F("'...reset.")));
    return "unknown";
  }
}

ventSpeed ventGetSpeedFromText(String payload)
{
  if (ventSpeedNoneText.equals(payload))
    return none;
  if (ventSpeedLowText.equals(payload))
    return low;
  if (ventSpeedMidText.equals(payload))
    return mid;
  if (ventSpeedHighText.equals(payload))
    return high;

  espNode->debugPrintln(String(F("VENT: Unknown speed payload '")) + payload + String(F("'...reset.")));
  return ventGetSpeed();
}

void ventSetState(ventState state, bool silent)
{
  if (ventGetState() != state)
  {
    ventMode modeVal = ventGetMode();

    if (modeVal == suck_in || modeVal == blow_out) // only turn on allows if mode is set
    {
      if (state == on) // set direction before changing state
      {
        switch (modeVal)
        {
        case suck_in:
          digitalWrite(VENT_DIR_PIN, LOW); // set vent direction to suck in
          break;
        case blow_out:
          digitalWrite(VENT_DIR_PIN, HIGH); // set vent direction to blow out
        }
      }

      digitalWrite(VENT_STATE_PIN, state);

      if (state == off)
      {
        digitalWrite(VENT_DIR_PIN, HIGH); // set vent direction to blow out - inactive relay mode
      }

      espNode->debugPrintln(String(F("VENT: Set state to ")) + (state == on ? String(F("ON")) : String(F("OFF"))));

      if (!silent) // send change silence is false
      {
        ventSendStatus(true);
      }
    }
    else
    {
      digitalWrite(VENT_STATE_PIN, off);
      digitalWrite(VENT_DIR_PIN, HIGH); // set vent direction to blow out - inactive relay mode

      espNode->debugPrintln(String(F("VENT: Forced state to 'OFF' due to mode '")) + ventGetModeText() + String(F("'.")));

      if (!silent) // send change silence is false
      {
        ventSendStatus(true);
      }
    }
  }
  else
  {
    espNode->debugPrintln(String(F("VENT: State unchanged.")));
  }
}

ventState ventGetState()
{
  int stateVal = digitalRead(VENT_STATE_PIN);
  ventState state = (stateVal == on ? on : off);

  espNode->debugPrintln(String(F("VENT: Read state - ")) + (state == on ? String(F("ON")) : String(F("OFF"))));
  return state;
}

String ventGetStateText()
{
  ventState sateVal = ventGetState();

  switch (sateVal)
  {
  case on:
    return ventStateOnText;
  case off:
    return ventStateOffText;

  default:
    espNode->debugPrintln(String(F("VENT: Unknown state '")) + String(sateVal) + String(F("'...reset.")));
    return "unknown";
  }
}

ventState ventGetStateFromText(String payload)
{
  if (ventStateOnText.equals(payload))
    return on;
  if (ventStateOffText.equals(payload))
    return off;

  espNode->debugPrintln(String(F("VENT: Unknown state payload '")) + payload + String(F("'...reset.")));
  return ventGetState();
}

void ventSetMode(ventMode mode, bool silent)
{
  if (ventModeCurrent != mode)
  {
    espNode->debugPrintln(String(F("VENT: Mode was changed from '")) + mode + String(F("' to '")) + ventModeWanted + String(F("'")));

    ventModeCurrent = pending; // save new mode and set mode to pending
    ventModeWanted = mode;
    ventSetState(off, silent); // turn of vent if mode gets changed

    digitalWrite(VENT_FLAPS_MOVE_PIN, HIGH); // stop flaps from moving
    switch (ventModeWanted)                  // only suck_in or blow_out are allowed to be selected
    {
    case suck_in:
      digitalWrite(VENT_FLAP_SUCKIN_PIN, LOW);   // set desired flap state opened
      digitalWrite(VENT_FLAP_BLOWOUT_PIN, HIGH); // set desired flap state closed
      break;

    case blow_out:
      digitalWrite(VENT_FLAP_SUCKIN_PIN, HIGH); // set desired flap state closed
      digitalWrite(VENT_FLAP_BLOWOUT_PIN, LOW); // set desired flap state opened
      break;

    default:
      espNode->debugPrintln(String(F("VENT: Unknown or invalid mode selected '")) + ventModeWanted + String(F("' (only suck_in or blow_out are settable)...reset.")));
    }

    digitalWrite(VENT_FLAPS_MOVE_PIN, LOW); // start flaps moving
    ventModeTimerMillis = millis();         // reset timer
    espNode->debugPrintln(String(F("VENT: Starting to set up mode '")) + ventModeWanted + String(F("'")));

    if (!silent) // send change silence is false
    {
      ventSendStatus(true);
    }
  }
}

ventMode ventGetMode()
{
  return ventModeCurrent;
}

String ventGetModeText()
{
  ventMode modeVal = ventGetMode();

  switch (modeVal)
  {
  case unknown:
    return ventModeUnknownText;
  case suck_in:
    return ventModeSuckInText;
  case blow_out:
    return ventModeBlowOutText;
  case pending:
    return ventModePendingText;

  default:
    espNode->debugPrintln(String(F("VENT: Unknown state '")) + String(modeVal) + String(F("'...reset.")));
    return "unknown";
  }
}

ventMode ventGetModeFromText(String payload)
{
  if (ventModeSuckInText.equals(payload))
    return suck_in;
  if (ventModeBlowOutText.equals(payload))
    return blow_out;

  espNode->debugPrintln(String(F("VENT: Unknown or invlaid mode payload '")) + payload + String(F("' - returning current mode.")));
  return ventGetMode();
}

void ventRefreshMode()
{
  if (ventModeWanted != ventModeCurrent)
  {
    unsigned long millisPassed = millis() - ventModeTimerMillis;

    if (millisPassed >= ventModeTimout)
    {
      espNode->debugPrintln(String(F("VENT: Vent mode '")) + ventModeWanted + String(F("' has been set up.")));

      ventModeCurrent = ventModeWanted;
      ventModeTimerMillis = 0;

      digitalWrite(VENT_FLAPS_MOVE_PIN, HIGH);   // stop flaps moving
      digitalWrite(VENT_FLAP_SUCKIN_PIN, HIGH);  // set desired flap state closed - inactive relay mode
      digitalWrite(VENT_FLAP_BLOWOUT_PIN, HIGH); // set desired flap state closed - inactive relay mode

      ventSendStatus(true);
    }
  }
}

void ventSendStatus(bool force)
{
  if (force || ventSendError)
  {
    ventSendError = !(espNode->mqttSend(espNode->mqttGetNodeTopic(F("vent/speed")), ventGetSpeedText()) && espNode->mqttSend(espNode->mqttGetNodeTopic(F("vent/state")), ventGetStateText()) && espNode->mqttSend(espNode->mqttGetNodeTopic(F("vent/mode")), ventGetModeText()));
    espNode->debugPrintln(String(F("VENT: Send status via mqtt.")));
  }
}

void ventLoop()
{
  ventRefreshMode();
  ventSendStatus(false);
}

void ventRelRcvCallback(String &topic, String &payload)
{
  espNode->debugPrintln(String(F("VENT: Message arrived on topic: '")) + topic + String(F("' with payload: '")) + payload + String(F("'.")));

  if (topic.equals(espNode->mqttGetNodeCmdTopic(F("vent/speed"))))
  {
    ventSetSpeed(ventGetSpeedFromText(payload));
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(F("vent/state"))))
  {
    ventSetState(ventGetStateFromText(payload));
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(F("vent/mode"))))
  {
    ventSetMode(ventGetModeFromText(payload));
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(String(F("relay/0")))))
  {
    if (payload.equals(espNode->mqttGetOnOffPayload(true))) {
      digitalWrite(VENTREL_RELAY_PIN_0, LOW);   // relay is active low
    }

    if (payload.equals(espNode->mqttGetOnOffPayload(false))) {
      digitalWrite(VENTREL_RELAY_PIN_0, HIGH);
    }

    if (payload.equals(String(F("toogle")))) {
      digitalWrite(VENTREL_RELAY_PIN_0, !digitalRead(VENTREL_RELAY_PIN_0));
    }

    ventRelAvailable();
  }

  if (topic.equals(espNode->mqttGetNodeCmdTopic(String(F("relay/1")))))
  {
    if (payload.equals(espNode->mqttGetOnOffPayload(true))) {
      digitalWrite(VENTREL_RELAY_PIN_1, LOW);  // relay is active low
    }

    if (payload.equals(espNode->mqttGetOnOffPayload(false))) {
      digitalWrite(VENTREL_RELAY_PIN_1, HIGH);
    }

    if (payload.equals(String(F("toogle")))) {
      digitalWrite(VENTREL_RELAY_PIN_1, !digitalRead(VENTREL_RELAY_PIN_1));
    }

    ventRelAvailable();
  }
}

void ventRelAvailable()
{
  ventSendStatus(true);

  // relay is active low
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("relay/0"))), espNode->mqttGetOnOffPayload(!digitalRead(VENTREL_RELAY_PIN_0)));
  espNode->mqttSend(espNode->mqttGetNodeTopic(String(F("relay/1"))), espNode->mqttGetOnOffPayload(!digitalRead(VENTREL_RELAY_PIN_1)));
}

void webHandleVentRelay()
{
  espNode->debugPrintln(String(F("HTTP: webHandleVent called from client: ")));

  espNode->webStartHttpMsg(String(F("Ventilation & Relay")), 200);

  espNode->webSendHttpContent(HTML_VENTREL_STATE, String(F("{ventState}")), ventGetStateText());
  espNode->webSendHttpContent(HTML_VENTREL_SPEED, String(F("{ventSpeed}")), ventGetSpeedText());
  espNode->webSendHttpContent(HTML_VENTREL_MODE, String(F("{ventMode}")), ventGetModeText());

  espNode->webSendHttpContent(HTML_VENTREL_HUM_1, String(F("{ventHum}")), String(dhtSensor1Humidity));
  espNode->webSendHttpContent(HTML_VENTREL_TEMP_1, String(F("{ventTemp}")), String(dhtSensor1Temp));
  espNode->webSendHttpContent(HTML_VENTREL_HUM_2, String(F("{ventHum}")), String(dhtSensor2Humidity));
  espNode->webSendHttpContent(HTML_VENTREL_TEMP_2, String(F("{ventTemp}")), String(dhtSensor2Temp));

  espNode->webSendHttpContent(HTML_VENTREL_RELAY_0_STATE,String(F("{ventRelayState}")), String(espNode->mqttGetOnOffPayload(!digitalRead(VENTREL_RELAY_PIN_0))));
  espNode->webSendHttpContent(HTML_VENTREL_RELAY_1_STATE,String(F("{ventRelayState}")), String(espNode->mqttGetOnOffPayload(!digitalRead(VENTREL_RELAY_PIN_1))));
  

  espNode->webSendHttpContent(HTML_VENTREL_BTN_BACK);

  espNode->webEndHttpMsg();

  espNode->debugPrintln(String(F("HTTP: webHandleVent page sent.")));
}
