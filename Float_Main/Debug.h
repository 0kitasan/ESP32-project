#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "MqttLink.h"
#include "SensorDriver.h"
#include "MotorDriver.h"
#include "Pump.h"

// ---------- 日志接口 ----------
void debugBegin(MqttLink* mqtt);
void debugInfo(const String& msg);
void debugWarn(const String& msg);
void debugError(const String& msg);

// ---------- MQTT 最小链路 ----------
void debugMQTT(MqttLink& mqtt, bool& running, unsigned long& counter, unsigned long& lastSendMs);

// ---------- 模块测试 ----------
void debugSensor(SensorDriver& sensor);
void debugPump(Pump& pump);
void debugPumpStartThreshold(MotorDriver& motor);

#endif