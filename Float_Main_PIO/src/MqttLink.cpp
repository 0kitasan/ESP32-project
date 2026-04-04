#include "MqttLink.h"

#include <string.h>

MqttLink *MqttLink::instance_ = nullptr;

MqttLink::MqttLink()
    : mqttClient_(wifiClient_),
      commandSlots_{{MQTT_TOPIC_CMD_MISSION, "", false},
                    {MQTT_TOPIC_CMD_MOTOR, "", false},
                    {MQTT_TOPIC_CMD_PUMP, "", false},
                    {MQTT_TOPIC_CMD_COUNTER, "", false}},
      emptyCmd_(""),
      lastWifiRetryMs_(0),
      lastMqttRetryMs_(0)
{
  instance_ = this;
}

void MqttLink::begin()
{
  WiFi.mode(WIFI_STA);
  mqttClient_.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient_.setCallback(MqttLink::mqttCallback);

  connectWiFi();
  connectMqtt();
}

void MqttLink::update()
{
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    if (now - lastWifiRetryMs_ >= 3000)
    {
      lastWifiRetryMs_ = now;
      connectWiFi();
    }
    return;
  }

  if (!mqttClient_.connected())
  {
    if (now - lastMqttRetryMs_ >= 3000)
    {
      lastMqttRetryMs_ = now;
      connectMqtt();
    }
    return;
  }

  mqttClient_.loop();
}

bool MqttLink::isWifiConnected() const
{
  return WiFi.status() == WL_CONNECTED;
}

bool MqttLink::isMqttConnected()
{
  return mqttClient_.connected();
}

bool MqttLink::hasNewCommand(const char *topic) const
{
  const CommandSlot *slot = findCommandSlot(topic);
  return slot != nullptr && slot->hasNewCmd;
}

const String &MqttLink::latestCommand(const char *topic) const
{
  const CommandSlot *slot = findCommandSlot(topic);
  return slot != nullptr ? slot->latestCmd : emptyCmd_;
}

void MqttLink::clearCommand(const char *topic)
{
  CommandSlot *slot = findCommandSlot(topic);
  if (slot == nullptr)
  {
    return;
  }

  slot->latestCmd = "";
  slot->hasNewCmd = false;
}

bool MqttLink::hasNewCommand() const
{
  return hasNewCommand(MQTT_TOPIC_CMD_MISSION);
}

const String &MqttLink::latestCommand() const
{
  return latestCommand(MQTT_TOPIC_CMD_MISSION);
}

void MqttLink::clearCommand()
{
  clearCommand(MQTT_TOPIC_CMD_MISSION);
}

bool MqttLink::publishRaw(const char *topic, const char *payload)
{
  if (!mqttClient_.connected())
  {
    return false;
  }
  return mqttClient_.publish(topic, payload);
}

bool MqttLink::publishCounter(unsigned long value)
{
  if (!mqttClient_.connected())
  {
    return false;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%lu", value);
  return mqttClient_.publish(MQTT_TOPIC_COUNTER, buf);
}

bool MqttLink::publishDepthSample(unsigned long idx, unsigned long timeMs,
                                  float depthM)
{
  String payload;
  payload.reserve(80);
  payload += "{\"idx\":";
  payload += String(idx);
  payload += ",\"time_ms\":";
  payload += String(timeMs);
  payload += ",\"depth_m\":";
  payload += String(depthM, 3);
  payload += "}";

  return publishRaw(MQTT_TOPIC_HISTORY, payload.c_str());
}

void MqttLink::sendRealtimeData(float depth)
{
  String payload;
  payload.reserve(48);
  payload += "{\"time_ms\":";
  payload += String(millis());
  payload += ",\"depth_m\":";
  payload += String(depth, 3);
  payload += "}";

  publishRaw(MQTT_TOPIC_REALTIME, payload.c_str());
}

void MqttLink::sendHistoryLine(const String &line)
{
  publishRaw(MQTT_TOPIC_HISTORY, line.c_str());
}

MqttLink::CommandSlot *MqttLink::findCommandSlot(const char *topic)
{
  if (topic == nullptr)
  {
    return nullptr;
  }

  for (size_t i = 0; i < kCommandSlotCount; ++i)
  {
    if (strcmp(commandSlots_[i].topic, topic) == 0)
    {
      return &commandSlots_[i];
    }
  }

  return nullptr;
}

const MqttLink::CommandSlot *MqttLink::findCommandSlot(const char *topic) const
{
  if (topic == nullptr)
  {
    return nullptr;
  }

  for (size_t i = 0; i < kCommandSlotCount; ++i)
  {
    if (strcmp(commandSlots_[i].topic, topic) == 0)
    {
      return &commandSlots_[i];
    }
  }

  return nullptr;
}

void MqttLink::mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (instance_ != nullptr)
  {
    instance_->handleMessage(topic, payload, length);
  }
}

void MqttLink::handleMessage(char *topic, byte *payload, unsigned int length)
{
  String msg;
  msg.reserve(length);

  for (unsigned int i = 0; i < length; ++i)
  {
    msg += (char)payload[i];
  }

  Serial.print("MQTT message [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  CommandSlot *slot = findCommandSlot(topic);
  if (slot != nullptr)
  {
    slot->latestCmd = msg;
    slot->hasNewCmd = true;
  }
}

void MqttLink::connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("WiFi connected, IP = ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("WiFi connect timeout");
  }
}

void MqttLink::connectMqtt()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (mqttClient_.connected())
  {
    return;
  }

  Serial.print("Connecting MQTT...");

  bool ok = false;
  if (String(MQTT_USER).length() == 0)
  {
    ok = mqttClient_.connect(MQTT_CLIENT_ID);
  }
  else
  {
    ok = mqttClient_.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  }

  if (ok)
  {
    Serial.println("connected");
    for (size_t i = 0; i < kCommandSlotCount; ++i)
    {
      mqttClient_.subscribe(commandSlots_[i].topic);
      Serial.print("Subscribed: ");
      Serial.println(commandSlots_[i].topic);
    }
  }
  else
  {
    Serial.print("failed, rc=");
    Serial.println(mqttClient_.state());
  }
}
