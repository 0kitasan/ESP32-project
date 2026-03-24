#include "MqttLink.h"
#include "Config.h"

MqttLink* MqttLink::instance_ = nullptr;

MqttLink::MqttLink()
  : mqttClient_(wifiClient_),
    latestCmd_(""),
    hasNewCmd_(false),
    lastWifiRetryMs_(0),
    lastMqttRetryMs_(0) {
  instance_ = this;
}

void MqttLink::begin() {
  WiFi.mode(WIFI_STA);
  mqttClient_.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient_.setCallback(MqttLink::mqttCallback);

  connectWiFi();
  connectMqtt();
}

void MqttLink::update() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiRetryMs_ >= 3000) {
      lastWifiRetryMs_ = now;
      connectWiFi();
    }
    return;
  }

  if (!mqttClient_.connected()) {
    if (now - lastMqttRetryMs_ >= 3000) {
      lastMqttRetryMs_ = now;
      connectMqtt();
    }
    return;
  }

  mqttClient_.loop();
}

bool MqttLink::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool MqttLink::isMqttConnected() {
  return mqttClient_.connected();
}

bool MqttLink::publishRaw(const char* topic, const char* payload) {
  if (!mqttClient_.connected()) {
    return false;
  }
  return mqttClient_.publish(topic, payload);
}

bool MqttLink::publishCounter(unsigned long value) {
  if (!mqttClient_.connected()) {
    return false;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%lu", value);
  return mqttClient_.publish(MQTT_TOPIC_COUNTER, buf);
}

bool MqttLink::hasNewCommand() const {
  return hasNewCmd_;
}

const String& MqttLink::latestCommand() const {
  return latestCmd_;
}

void MqttLink::clearCommand() {
  latestCmd_ = "";
  hasNewCmd_ = false;
}

void MqttLink::mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (instance_ != nullptr) {
    instance_->handleMessage(topic, payload, length);
  }
}

void MqttLink::handleMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);

  for (unsigned int i = 0; i < length; ++i) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT message [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (String(topic) == MQTT_TOPIC_CMD) {
    latestCmd_ = msg;
    hasNewCmd_ = true;
  }
}

void MqttLink::connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP = ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout");
  }
}

void MqttLink::connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient_.connected()) {
    return;
  }

  Serial.print("Connecting MQTT...");

  bool ok = false;
  if (String(MQTT_USER).length() == 0) {
    ok = mqttClient_.connect(MQTT_CLIENT_ID);
  } else {
    ok = mqttClient_.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  }

  if (ok) {
    Serial.println("connected");
    mqttClient_.subscribe(MQTT_TOPIC_CMD);
    Serial.print("Subscribed: ");
    Serial.println(MQTT_TOPIC_CMD);
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient_.state());
  }
}