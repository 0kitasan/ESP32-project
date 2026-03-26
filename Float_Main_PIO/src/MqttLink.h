#ifndef MQTTLINK_H
#define MQTTLINK_H

#include <WiFi.h>
#include <PubSubClient.h>

class MqttLink
{
public:
  MqttLink();
  void begin();
  void update();

  bool isWifiConnected() const;
  bool isMqttConnected(); // const

  bool hasNewCommand() const;
  const String &latestCommand() const;
  void clearCommand();

  // 消息封装
  bool publishRaw(const char *topic, const char *payload); // 通用接口
  bool publishCounter(unsigned long value);                // 测试用，不过可能后面有用就留着
  // to do
  void sendRealtimeData(float depth);
  void sendHistoryLine(const String &line);

private:
  WiFiClient wifiClient_;
  PubSubClient mqttClient_;

  String latestCmd_;
  bool hasNewCmd_;

  unsigned long lastWifiRetryMs_;
  unsigned long lastMqttRetryMs_;

  static MqttLink *instance_;
  static void mqttCallback(char *topic, byte *payload, unsigned int length);

  void handleMessage(char *topic, byte *payload, unsigned int length);
  void connectWiFi();
  void connectMqtt();
};
#endif