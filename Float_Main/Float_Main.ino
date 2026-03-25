#include "Config.h"
#include "FloatManager.h"
#include "MotorDriver.h"
#include "MqttLink.h"
#include "SensorDriver.h"
#include "StorageDriver.h"
#include "Debug.h"
#include <ArduinoOTA.h>

// 1. 实例化所有硬件驱动
SensorDriver mySensor;
MotorDriver myMotor;
MqttLink myMqtt;
StorageDriver myStorage;

// 2. 实例化管理器，并将驱动传给它
FloatManager myManager(&mySensor, &myMotor, &myMqtt, &myStorage);

void setup() {
  Serial.begin(115200);
  delay(500); // 等待串口稳定

  // 初始化硬件
  mySensor.init();
  myMotor.init();
  myMqtt.begin();
  myStorage.init();
  delay(500); // 等待串口稳定

  // 初始化逻辑
  myManager.init();
  debugBegin(&myMqtt);
  // debug逻辑测试
  debugInfo("system boot");
  debugWarn("example: wifi reconnecting");
  debugError("example: sensor init failed");
  ArduinoOTA.begin();
}

bool running = false;
unsigned long counter = 0;
unsigned long lastSendMs = 0;

void loop() {

  debugMQTT(myMqtt, running, counter, lastSendMs);
  debugSensor(mySensor);
  delay(100);

  // myMotor.setThrust(1.0);
  // Serial.println("Thrust set to 1.0 (100%)");
  // delay(2000); // 持续2秒
  // // 停止
  // myMotor.stop();
  // Serial.println("Pump stopped");
  // delay(2000); // 停止2秒
  // myMotor.setThrust(-1.0);
  // Serial.println("Thrust set to -1.0 (-100%)");
  // delay(2000); // 持续2秒
  // // 停止
  // myMotor.brake();
  // Serial.println("Pump stopped");
  // delay(2000); // 停止2秒
  // // 2. 更新决策 (计算状态机、PID、处理数据记录)
  // myManager.update();

  // 3. (可选) 如果MotorDriver需要平滑控制，也可以在这里加 update
  // myMotor.update();

  // 4. 控制频率 (简单的延时，或者用 millis 控制固定周期)
  delay(10);
  ArduinoOTA.handle();
}
