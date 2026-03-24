#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "esp32_sjtu";
const char* WIFI_PASS = "esp32_sjtu";

const char* MQTT_HOST = "192.168.137.1";   // 改成你的 broker IP
const int   MQTT_PORT = 1883;
const char* MQTT_USER = "admin";           // 没开认证可留空
const char* MQTT_PASS = "Ciallo~114";          // 没开认证可留空

const char* TOPIC_CMD = "/cmd";
const char* TOPIC_COUNTER = "/counter";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool counting = false;
unsigned long counter = 0;
unsigned long lastTickMs = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP = ");
  Serial.println(WiFi.localIP());
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (String(topic) == TOPIC_CMD) {
    if (msg == "start") {
      counting = true;
      Serial.println("Counter started");
    } else if (msg == "stop") {
      counting = false;
      Serial.println("Counter stopped");
    } else if (msg == "clear") {
      counter = 0;
      Serial.println("Counter cleared");
      mqttClient.publish(TOPIC_COUNTER, "0");
    }
  }
}

void connectMQTT() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);

  while (!mqttClient.connected()) {
    Serial.print("Connecting MQTT...");

    bool ok;
    if (String(MQTT_USER).length() == 0) {
      ok = mqttClient.connect("esp32-counter");
    } else {
      ok = mqttClient.connect("esp32-counter", MQTT_USER, MQTT_PASS);
    }

    if (ok) {
      Serial.println("connected");
      mqttClient.subscribe(TOPIC_CMD);
      Serial.print("Subscribed: ");
      Serial.println(TOPIC_CMD);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retry in 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  connectWiFi();
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (counting && now - lastTickMs >= 1000) {
    lastTickMs = now;
    counter++;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", counter);

    mqttClient.publish(TOPIC_COUNTER, buf);

    Serial.print("Published /counter: ");
    Serial.println(buf);
  }
}