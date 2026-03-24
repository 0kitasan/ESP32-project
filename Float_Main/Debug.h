#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "MqttLink.h"

void debugMQTT(MqttLink& mqtt, bool& running, unsigned long& counter, unsigned long& lastSendMs);
void debugSensor();

#endif
