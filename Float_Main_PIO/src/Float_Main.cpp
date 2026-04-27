#include "Config.h"
#include "Control.h"
#include "Debug.h"
#include "Mission.h"
#include "MotorDriver.h"
#include "MqttLink.h"
#include "Pump.h"
#include "SensorDriver.h"
#include <ArduinoOTA.h>

namespace
{
  constexpr bool kMissionEnableVolumeLimit = false;
  constexpr unsigned long kMissionForceDrainAfterMs = 30000;
  constexpr unsigned long kMissionForceDrainDurationMs = 10000;
  constexpr unsigned int kOtaProgressLogStepPercent = 10;
}

// 1. 实例化所有硬件驱动
SensorDriver mySensor;
MotorDriver myMotor;
MqttLink myMqtt;
Pump myPump(myMotor, true, 0.0f, 400.0f);
Control myControl;

// 2. 实例化管理器，并将驱动传给它
Mission myMission(mySensor, myPump, myMqtt, myControl);

volatile bool otaInProgress = false;
unsigned int otaLastLoggedProgressPercent = 0;

String otaErrorMessage(ota_error_t error)
{
  switch (error)
  {
  case OTA_AUTH_ERROR:
    return "auth_error";
  case OTA_BEGIN_ERROR:
    return "begin_error";
  case OTA_CONNECT_ERROR:
    return "connect_error";
  case OTA_RECEIVE_ERROR:
    return "receive_error";
  case OTA_END_ERROR:
    return "end_error";
  default:
    return "unknown_error";
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500); // 等待串口稳定

  // 初始化硬件
  mySensor.init();
  myPump.init();
  myMqtt.begin();
  delay(500); // 等待串口稳定

  // 初始化逻辑
  myMission.init();
  debugBegin(&myMqtt);
  // debug逻辑测试
  debugInfo("system boot");
  debugInfo("mission cmd topic: " MQTT_TOPIC_CMD_MISSION);
  debugInfo("debug mission cmd topic: " MQTT_TOPIC_CMD_DEBUG_MISSION);
  debugInfo("motor cmd topic: " MQTT_TOPIC_CMD_MOTOR);
  debugInfo("pump cmd topic: " MQTT_TOPIC_CMD_PUMP);
  debugInfo("counter cmd topic: " MQTT_TOPIC_CMD_COUNTER);
  debugInfo("motor cmd format: <speed>,<duration_ms> or stop");
  debugInfo("mission status topic: " MQTT_TOPIC_STATUS);
  debugInfo("mission param topic: " MQTT_TOPIC_PARAM);
  debugInfo("mission debug history topic: " MQTT_TOPIC_HISTORY_DEBUG);
  debugInfo("pump sign convention: +fill/down, -drain/up");
  String missionDefaults = "mission stage defaults: stage1_depth_m=";
  missionDefaults += String(MISSION_STAGE1_TARGET_DEPTH_M_DEFAULT, 3);
  missionDefaults += ", stage1_hold_ms=";
  missionDefaults += String(MISSION_STAGE1_HOLD_MS_DEFAULT);
  missionDefaults += ", stage2_depth_m=";
  missionDefaults += String(MISSION_STAGE2_TARGET_DEPTH_M_DEFAULT, 3);
  missionDefaults += ", stage2_hold_ms=";
  missionDefaults += String(MISSION_STAGE2_HOLD_MS_DEFAULT);
  missionDefaults += ", ballast_fill_duration_ms=";
  missionDefaults += String(MISSION_BALLAST_FILL_DURATION_MS_DEFAULT);
  missionDefaults += ", tracking_timeout_enable=";
  missionDefaults += String(MISSION_TRACKING_TIMEOUT_ENABLE_DEFAULT);
  missionDefaults += ", tracking_timeout_ms=";
  missionDefaults += String(MISSION_TRACKING_TIMEOUT_MS_DEFAULT);
  missionDefaults += ", surface_drain_duration_ms=";
  missionDefaults += String(MISSION_SURFACE_DRAIN_DURATION_MS_DEFAULT);
  missionDefaults += ", hold_enter_band_m=";
  missionDefaults += String(CTRL_HOLD_ENTER_BAND_M_DEFAULT, 3);
  missionDefaults += ", hold_exit_band_m=";
  missionDefaults += String(CTRL_HOLD_EXIT_BAND_M_DEFAULT, 3);
  missionDefaults += ", pulse_window_m=";
  missionDefaults += String(CTRL_PULSE_WINDOW_M_DEFAULT, 3);
  missionDefaults += ", pulse_on_ms=";
  missionDefaults += String(CTRL_PULSE_MIN_ON_MS_DEFAULT);
  missionDefaults += "-";
  missionDefaults += String(CTRL_PULSE_MAX_ON_MS_DEFAULT);
  missionDefaults += ", pulse_off_ms=";
  missionDefaults += String(CTRL_PULSE_OFF_MS_DEFAULT);
  missionDefaults += ", pulse_cmd=";
  missionDefaults += String(CTRL_PULSE_CMD_DEFAULT, 3);
  missionDefaults += ", debug_history_interval_ms=";
  missionDefaults += String(MISSION_DEBUG_HISTORY_INTERVAL_MS);
  missionDefaults += ", debug_history_capacity=";
  missionDefaults += String((unsigned long)MISSION_DEBUG_HISTORY_CAPACITY);
  debugInfo(missionDefaults);

  String debugMissionDefaults = "debug mission force drain defaults: after_ms=";
  debugMissionDefaults += String(kMissionForceDrainAfterMs);
  debugMissionDefaults += ", duration_ms=";
  debugMissionDefaults += String(kMissionForceDrainDurationMs);
  debugInfo(debugMissionDefaults);
  debugInfo("mission cmd format: start,stage1_depth_m=<m>,stage1_hold_ms=<ms>,stage2_depth_m=<m>,stage2_hold_ms=<ms>,ballast_fill_duration_ms=<ms>,tracking_timeout_enable=<0|1>,tracking_timeout_ms=<ms>,utc_epoch_s=<s>,kp=<v>,kd=<v>,pulse_enable=<0|1>,pulse_window_m=<m>,pulse_min_on_ms=<ms>,pulse_max_on_ms=<ms>,pulse_off_ms=<ms>,pulse_coast_rate_mps=<mps>,pulse_cmd=<v>,lead_enable=<0|1>,lead_gain=<v>,lead_tau_s=<s>,lead_alpha=<v>,surface_drain_duration_ms=<ms> or JSON");
  debugInfo("debug mission cmd format: start:<depth_m>,kp=<v>,kd=<v>,pulse_enable=<0|1>,pulse_window_m=<m>,pulse_min_on_ms=<ms>,pulse_max_on_ms=<ms>,pulse_off_ms=<ms>,pulse_coast_rate_mps=<mps>,pulse_cmd=<v>,lead_enable=<0|1>,lead_gain=<v>,lead_tau_s=<s>,lead_alpha=<v>,drain_after_ms=<ms>,drain_duration_ms=<ms> or {\"target_depth_m\":<depth_m>}");
  debugInfo("history sample format: {\"idx\":n,\"time_ms\":t,\"depth_m\":d,\"control_output\":u}");
  String missionVolumeLimit = "mission volume limit: ";
  missionVolumeLimit += kMissionEnableVolumeLimit ? "on" : "off";
  debugInfo(missionVolumeLimit);
  debugInfo("status topic reports current depth and mission stage");
  debugWarn("example: wifi reconnecting");
  debugError("example: sensor init failed");
  // 打印 ip 地址
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
      .onStart([]()
               {
                 otaInProgress = true;
                 otaLastLoggedProgressPercent = 0;
                 debugWarn("ota start"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  {
                    if (total == 0)
                    {
                      return;
                    }

                    unsigned int percent = (progress * 100U) / total;
                    if (percent < otaLastLoggedProgressPercent + kOtaProgressLogStepPercent &&
                        percent < 100U)
                    {
                      return;
                    }

                    otaLastLoggedProgressPercent = percent;

                    String msg = "ota progress ";
                    msg += String(percent);
                    msg += "% (";
                    msg += String(progress);
                    msg += "/";
                    msg += String(total);
                    msg += ")";
                    debugInfo(msg); })
      .onEnd([]()
             {
               otaInProgress = false;
               debugWarn("ota end"); })
      .onError([](ota_error_t error)
               {
                 otaInProgress = false;

                 String msg = "ota error: ";
                 msg += otaErrorMessage(error);
                 msg += " (";
                 msg += String((unsigned int)error);
                 msg += ")";
                 debugError(msg); });

  ArduinoOTA.begin();

  String otaReadyMsg = "ota ready";
  if (WiFi.status() == WL_CONNECTED)
  {
    otaReadyMsg += ", ip=";
    otaReadyMsg += WiFi.localIP().toString();
  }
  otaReadyMsg += ", hostname=";
  otaReadyMsg += ArduinoOTA.getHostname();
  debugWarn(otaReadyMsg);
}

void loop()
{
  ArduinoOTA.handle();
  delay(2);

  if (!otaInProgress)
  {
    // 命令 topic 已拆分，但共享同一执行器的测试入口仍不应同时启用。
    myMission.update();
    // debugDepthMission(myMqtt, mySensor, myPump, myControl,
    //                   kMissionEnableVolumeLimit,
    //                   kMissionForceDrainAfterMs,
    //                   kMissionForceDrainDurationMs);
    // debugFakeHistoryUpload(myMqtt);
    // debugMotorRemote(myMqtt, myMotor);
    // debugSensor(mySensor);
    // debugPumpRemote(myMqtt, myPump);
  }
}
