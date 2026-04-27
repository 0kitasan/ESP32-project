#ifndef MQTTLINK_H
#define MQTTLINK_H

#include "Config.h"
#include <WiFi.h>
#include <PubSubClient.h>

class MqttLink
{
public:
  MqttLink();
  void begin();
  void update(bool allowReconnect = true);

  bool isWifiConnected() const;
  bool isMqttConnected(); // const

  bool hasNewCommand(const char *topic) const;
  const String &latestCommand(const char *topic) const;
  void clearCommand(const char *topic);

  bool hasNewCommand() const;
  const String &latestCommand() const;
  void clearCommand();

  // 消息封装
  bool publishRaw(const char *topic, const char *payload); // 通用接口
  bool publishCounter(unsigned long value);                // 测试用，不过可能后面有用就留着
  bool publishDepthSample(unsigned long idx, unsigned long timeMs, float depthM,
                          float controlOutput);
  void sendRealtimeData(float depth);
  void sendHistoryLine(const String &line);

private:
  struct CommandSlot
  {
    const char *topic;
    String latestCmd;
    bool hasNewCmd;
  };

  static constexpr size_t kCommandSlotCount = 5;

  WiFiClient wifiClient_;
  PubSubClient mqttClient_;

  CommandSlot commandSlots_[kCommandSlotCount];
  String emptyCmd_;

  unsigned long lastWifiRetryMs_;
  unsigned long lastMqttRetryMs_;
  unsigned long lastTimeSyncRetryMs_;
  bool timeSyncStarted_;

  static MqttLink *instance_;
  static void mqttCallback(char *topic, byte *payload, unsigned int length);

  CommandSlot *findCommandSlot(const char *topic);
  const CommandSlot *findCommandSlot(const char *topic) const;
  void handleMessage(char *topic, byte *payload, unsigned int length);
  void connectWiFi();
  void connectMqtt();
  void ensureTimeSync();
};
#endif
