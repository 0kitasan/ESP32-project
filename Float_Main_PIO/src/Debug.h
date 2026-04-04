#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "MqttLink.h"
#include "SensorDriver.h"
#include "MotorDriver.h"
#include "Pump.h"
#include "Control.h"

// ---------- 日志接口 ----------
void debugBegin(MqttLink *mqtt);
void debugInfo(const String &msg);
void debugWarn(const String &msg);
void debugError(const String &msg);

// ---------- MQTT 最小链路 ----------
void debugMQTT(MqttLink &mqtt, bool &running, unsigned long &counter, unsigned long &lastSendMs);
void debugMotorRemote(MqttLink &mqtt, MotorDriver &motor);
void debugFakeHistoryUpload(MqttLink &mqtt);
void debugDepthMission(MqttLink &mqtt, SensorDriver &sensor, Pump &pump,
                       Control &control, bool enableVolumeLimit = true,
                       unsigned long forceDrainAfterMs = 30000,
                       unsigned long forceDrainDurationMs = 10000);

// ---------- 模块测试 ----------
void debugSensor(SensorDriver &sensor);
void debugPumpRemote(MqttLink &mqtt, Pump &pump);

#endif
