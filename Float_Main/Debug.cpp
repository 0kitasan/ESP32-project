#include "Debug.h"
#include <ArduinoOTA.h>
#include "Config.h"

void debugMQTT(MqttLink& mqtt, bool& running, unsigned long& counter, unsigned long& lastSendMs) {
  mqtt.update();

  if (mqtt.hasNewCommand()) {
    String cmd = mqtt.latestCommand();

    if (cmd == "start") {
      running = true;
      Serial.println("cmd=start");
    } else if (cmd == "stop") {
      running = false;
      Serial.println("cmd=stop");
    } else if (cmd == "clear") {
      counter = 0;
      Serial.println("cmd=clear");
    } else {
      Serial.print("unknown cmd: ");
      Serial.println(cmd);
    }

    mqtt.clearCommand();
  }

  unsigned long now = millis();
  if (running && now - lastSendMs >= 1000) {
    lastSendMs = now;
    counter++;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", counter);
    mqtt.publishRaw(MQTT_TOPIC_COUNTER, buf);
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