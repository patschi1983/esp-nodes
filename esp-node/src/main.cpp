#include <Arduino.h>
#include <EspNode.h>

//***** ESP Node *****//
char fwName[16] = "test_fw";      // Name of the firmware
char fwVersion[8] = "47.11";          // Version of the firmware
char nodeName[32] = "test_node"; // Nodes name - default value, may be overridden

EspNode* espNode;

void setup()
{
  espNode = new EspNode(fwName, fwVersion);
  espNode->setup();
}

void loop()
{
  espNode->loop();
}