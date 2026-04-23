#include "Config.h"
#include "Control.h"
#include "Debug.h"
#include "FloatManager.h"
#include "MotorDriver.h"
#include "MqttLink.h"
#include "Pump.h"
#include "SensorDriver.h"
#include "StorageDriver.h"
#include <ArduinoOTA.h>

namespace
{
constexpr bool kMissionEnableVolumeLimit = false;
constexpr unsigned long kMissionForceDrainAfterMs = 30000;
constexpr unsigned long kMissionForceDrainDurationMs = 10000;
}

// 1. 实例化所有硬件驱动
SensorDriver mySensor;
MotorDriver myMotor;
MqttLink myMqtt;
Pump myPump(myMotor, true, 0.0f, 400.0f);
Control myControl;
StorageDriver myStorage;

// 2. 实例化管理器，并将驱动传给它
FloatManager myManager(&mySensor, &myMotor, &myMqtt, &myStorage);

volatile bool otaInProgress = false;

void setup()
{
  Serial.begin(115200);
  delay(500); // 等待串口稳定

  // 初始化硬件
  mySensor.init();
  myPump.init();
  myMqtt.begin();
  myStorage.init();
  delay(500); // 等待串口稳定

  // 初始化逻辑
  myManager.init();
  debugBegin(&myMqtt);
  // debug逻辑测试
  debugInfo("system boot");
  debugInfo("mission cmd topic: " MQTT_TOPIC_CMD_MISSION);
  debugInfo("motor cmd topic: " MQTT_TOPIC_CMD_MOTOR);
  debugInfo("pump cmd topic: " MQTT_TOPIC_CMD_PUMP);
  debugInfo("counter cmd topic: " MQTT_TOPIC_CMD_COUNTER);
  debugInfo("motor cmd format: <speed>,<duration_ms> or stop");
  debugInfo("mission status topic: " MQTT_TOPIC_STATUS);
  debugInfo("mission param topic: " MQTT_TOPIC_PARAM);
  debugInfo("pump sign convention: +fill/down, -drain/up");
  debugInfo("mission force drain defaults: after_ms=" +
            String(kMissionForceDrainAfterMs) + ", duration_ms=" +
            String(kMissionForceDrainDurationMs));
  debugInfo("mission cmd format: start:<depth_m>,kp=<v>,kd=<v>,lead_enable=<0|1>,lead_gain=<v>,lead_tau_s=<s>,lead_alpha=<v>,drain_after_ms=<ms>,drain_duration_ms=<ms> or {\"target_depth_m\":<depth_m>}");
  debugInfo("history sample format: {\"idx\":n,\"time_ms\":t,\"depth_m\":d,\"control_output\":u}");
  debugInfo("mission volume limit: " + String(kMissionEnableVolumeLimit ? "on" : "off"));
  debugInfo("status topic reports current depth before dive");
  debugWarn("example: wifi reconnecting");
  debugError("example: sensor init failed");
  //打印 ip 地址
  if (WiFi.status() == WL_CONNECTED)
  {
    debugInfo("wifi ip: " + WiFi.localIP().toString());
  }
  else
  {
    debugWarn("wifi ip unavailable: WiFi not connected");
  }
  // OTA 回调
  ArduinoOTA
  .onStart([]() {
    otaInProgress = true;
  })
  .onEnd([]() {
    otaInProgress = false;
  })
  .onError([](ota_error_t error) {
    otaInProgress = false;
  });

  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();
  delay(2);

  if (!otaInProgress){
    // 命令 topic 已拆分，但共享同一执行器的测试入口仍不应同时启用。
    debugDepthMission(myMqtt, mySensor, myPump, myControl,
                      kMissionEnableVolumeLimit,
                      kMissionForceDrainAfterMs,
                      kMissionForceDrainDurationMs);
    // debugFakeHistoryUpload(myMqtt);
    // debugMotorRemote(myMqtt, myMotor);
    // debugSensor(mySensor);
    // debugPumpRemote(myMqtt, myPump);
  }
}
