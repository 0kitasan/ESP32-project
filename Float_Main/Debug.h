#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "MqttLink.h"

// ---------- 日志接口 ----------
void debugBegin(MqttLink* mqtt);
void debugInfo(const String& msg);
void debugWarn(const String& msg);
void debugError(const String& msg);

// ---------- 最小链路 / 模块测试 ----------
void debugMQTT(MqttLink& mqtt, bool& running, unsigned long& counter, unsigned long& lastSendMs);
void debugSensor();

#endif