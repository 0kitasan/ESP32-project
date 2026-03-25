#include "Debug.h"
#include "Config.h"

namespace {
  MqttLink* g_mqtt = nullptr;
  unsigned long g_logSeq = 0;

  void debugWrite(const char* level, const String& msg) {
    g_logSeq++;

    String line;
    line.reserve(msg.length() + 24);
    line += String(g_logSeq);
    line += "|";
    line += level;
    line += "|";
    line += msg;

    // 本地串口始终输出
    Serial.println(line);

    // MQTT 可用时，顺手发到 debug topic
    if (g_mqtt != nullptr && g_mqtt->isMqttConnected()) {
      g_mqtt->publishRaw(MQTT_TOPIC_DEBUG, line.c_str());
    }
  }
}

void debugBegin(MqttLink* mqtt) {
  g_mqtt = mqtt;
}

void debugInfo(const String& msg) {
  debugWrite("INFO", msg);
}

void debugWarn(const String& msg) {
  debugWrite("WARN", msg);
}

void debugError(const String& msg) {
  debugWrite("ERROR", msg);
}

void debugMQTT(MqttLink& mqtt, bool& running, unsigned long& counter, unsigned long& lastSendMs) {
  mqtt.update();

  if (mqtt.hasNewCommand()) {
    String cmd = mqtt.latestCommand();

    if (cmd == "start") {
      running = true;
      debugInfo("cmd=start");
    } else if (cmd == "stop") {
      running = false;
      debugInfo("cmd=stop");
    } else if (cmd == "clear") {
      counter = 0;
      debugInfo("cmd=clear");
    } else {
      debugWarn(String("unknown cmd: ") + cmd);
    }

    mqtt.clearCommand();
  }

  unsigned long now = millis();
  if (running && now - lastSendMs >= 1000) {
    lastSendMs = now;
    counter++;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", counter);

    if (mqtt.publishRaw(MQTT_TOPIC_COUNTER, buf)) {
      debugInfo(String("publish counter=") + buf);
    } else {
      debugWarn("publish counter failed");
    }
  }
}

void debugSensor() {
  // 获取当前时间
  // unsigned long currentMillis = millis();

  // // 1. 更新感知 (读取所有传感器)
  // mySensor.update();

  // // 获取当前水压计数据
  // float depth = mySensor.getDepth();
  // float pressure = mySensor.getPressure();
  // float temp = mySensor.getTemp();

  // Serial.print("[Time: ");
  // Serial.print(currentMillis / 1000.0);
  // Serial.print("s] ");

  // Serial.print("Depth: ");
  // Serial.print(depth, 3);
  // Serial.print(" m  |  ");

  // Serial.print("Pressure: ");
  // Serial.print(pressure, 1);
  // Serial.print(" mbar  |  ");

  // Serial.print("Temp: ");
  // Serial.print(temp, 2);
  // Serial.println(" C");
}