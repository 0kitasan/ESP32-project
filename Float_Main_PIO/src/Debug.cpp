#include "Debug.h"
#include "Config.h"

namespace
{
  MqttLink *g_mqtt = nullptr;
  unsigned long g_logSeq = 0;
  constexpr unsigned long kFakeHistoryIntervalMs = 10;
  constexpr unsigned long kFakeHistorySampleCount = 50;
  constexpr unsigned long kFakeMissionDurationMs = 29500;
  constexpr unsigned long kDebugReadyStatusIntervalMs = 2000;
  constexpr unsigned long kDebugHistoryLogIntervalMs = 500;
  constexpr unsigned long kDebugUploadIntervalMs = 10;
  constexpr size_t kDebugMaxHistorySamples = 256;

  struct DebugDepthSample
  {
    unsigned long timeMs;
    float depthM;
    float controlOutput;
  };

  struct MissionCommand
  {
    enum Type
    {
      NONE,
      START,
      STATUS,
      SURFACE,
      INVALID
    };

    Type type = NONE;
    float targetDepthM = 0.0f;
    unsigned long forceDrainAfterMs = 30000;
    unsigned long forceDrainDurationMs = 10000;
    Control::Params params;
  };

  Control::Params defaultControlParams()
  {
    Control::Params params;
    params.kp = CTRL_KP_DEFAULT;
    params.kd = CTRL_KD_DEFAULT;
    params.outputLimit = CTRL_OUTPUT_LIMIT_DEFAULT;
    params.minActuationCmd = CTRL_MIN_ACTUATION_CMD_DEFAULT;
    params.holdEnterBandM = CTRL_HOLD_ENTER_BAND_M_DEFAULT;
    params.holdExitBandM = CTRL_HOLD_EXIT_BAND_M_DEFAULT;
    params.derivativeFilterAlpha = CTRL_DERIVATIVE_FILTER_ALPHA_DEFAULT;
    params.leadEnabled = CTRL_LEAD_ENABLE_DEFAULT != 0;
    params.leadGain = CTRL_LEAD_GAIN_DEFAULT;
    params.leadTauS = CTRL_LEAD_TAU_S_DEFAULT;
    params.leadAlpha = CTRL_LEAD_ALPHA_DEFAULT;
    return params;
  }

  float lerpDepth(float startDepth, float endDepth, unsigned long elapsedMs,
                  unsigned long durationMs)
  {
    if (durationMs == 0)
    {
      return endDepth;
    }

    float alpha = (float)elapsedMs / (float)durationMs;
    alpha = constrain(alpha, 0.0f, 1.0f);
    return startDepth + (endDepth - startDepth) * alpha;
  }

  float fakeDepthAt(unsigned long timeMs)
  {
    if (timeMs <= 5000)
    {
      return lerpDepth(0.0f, 2.5f, timeMs, 5000);
    }
    if (timeMs <= 11000)
    {
      return 2.5f;
    }
    if (timeMs <= 16000)
    {
      return lerpDepth(2.5f, 0.4f, timeMs - 11000, 5000);
    }
    if (timeMs <= 20000)
    {
      return 0.4f;
    }
    if (timeMs <= 25000)
    {
      return lerpDepth(0.4f, 1.8f, timeMs - 20000, 5000);
    }
    if (timeMs <= 27500)
    {
      return 1.8f;
    }
    if (timeMs <= kFakeMissionDurationMs)
    {
      return lerpDepth(1.8f, 0.0f, timeMs - 27500, 2000);
    }
    return 0.0f;
  }

  void debugWrite(const char *level, const String &msg)
  {
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
    if (g_mqtt != nullptr && g_mqtt->isMqttConnected())
    {
      g_mqtt->publishRaw(MQTT_TOPIC_DEBUG, line.c_str());
    }
  }

  bool parseMotorRemoteCommand(const String &rawCmd, float &speed,
                               unsigned long &durationMs)
  {
    String cmd = rawCmd;
    cmd.trim();

    if (cmd.length() == 0)
    {
      return false;
    }

    if (cmd.startsWith("motor"))
    {
      cmd.remove(0, 5);
      cmd.trim();
      if (cmd.startsWith(":") || cmd.startsWith(","))
      {
        cmd.remove(0, 1);
        cmd.trim();
      }
    }

    cmd.replace(",", " ");
    cmd.replace(":", " ");
    cmd.replace(";", " ");
    cmd.trim();

    float parsedSpeed = 0.0f;
    unsigned long parsedDurationMs = 0;
    if (sscanf(cmd.c_str(), "%f %lu", &parsedSpeed, &parsedDurationMs) != 2)
    {
      return false;
    }

    if (parsedSpeed < -1.0f || parsedSpeed > 1.0f || parsedDurationMs == 0)
    {
      return false;
    }

    speed = parsedSpeed;
    durationMs = parsedDurationMs;
    return true;
  }

  bool appendDepthSample(DebugDepthSample *samples, size_t capacity,
                         size_t &sampleCount, unsigned long timeMs,
                         float depthM, float controlOutput)
  {
    if (sampleCount >= capacity)
    {
      return false;
    }

    samples[sampleCount].timeMs = timeMs;
    samples[sampleCount].depthM = depthM;
    samples[sampleCount].controlOutput = controlOutput;
    sampleCount++;
    return true;
  }

  bool parseLeadingFloat(const String &text, float &value)
  {
    for (int i = 0; i < text.length(); ++i)
    {
      char c = text.charAt(i);
      if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')
      {
        return sscanf(text.c_str() + i, "%f", &value) == 1;
      }
    }
    return false;
  }

  bool extractJsonFloat(const String &text, const char *key, float &value)
  {
    int keyPos = text.indexOf(key);
    if (keyPos < 0)
    {
      return false;
    }

    int colonPos = text.indexOf(':', keyPos);
    if (colonPos < 0)
    {
      return false;
    }

    String tail = text.substring(colonPos + 1);
    tail.trim();
    return parseLeadingFloat(tail, value);
  }

  bool extractAssignedFloat(const String &text, const char *key, float &value)
  {
    String pattern = String(key) + "=";
    int pos = text.indexOf(pattern);
    if (pos < 0)
    {
      return false;
    }

    String tail = text.substring(pos + pattern.length());
    tail.trim();
    return parseLeadingFloat(tail, value);
  }

  bool extractAssignedBool(const String &text, const char *key, bool &value)
  {
    String pattern = String(key) + "=";
    int pos = text.indexOf(pattern);
    if (pos < 0)
    {
      return false;
    }

    String tail = text.substring(pos + pattern.length());
    tail.trim();

    if (tail.startsWith("true"))
    {
      value = true;
      return true;
    }

    if (tail.startsWith("false"))
    {
      value = false;
      return true;
    }

    float parsed = 0.0f;
    if (!parseLeadingFloat(tail, parsed))
    {
      return false;
    }

    value = parsed != 0.0f;
    return true;
  }

  bool parseLeadingUnsignedLong(const String &text, unsigned long &value)
  {
    for (int i = 0; i < text.length(); ++i)
    {
      char c = text.charAt(i);
      if (c >= '0' && c <= '9')
      {
        return sscanf(text.c_str() + i, "%lu", &value) == 1;
      }
    }
    return false;
  }

  bool extractJsonUnsignedLong(const String &text, const char *key,
                               unsigned long &value)
  {
    int keyPos = text.indexOf(key);
    if (keyPos < 0)
    {
      return false;
    }

    int colonPos = text.indexOf(':', keyPos);
    if (colonPos < 0)
    {
      return false;
    }

    String tail = text.substring(colonPos + 1);
    tail.trim();
    return parseLeadingUnsignedLong(tail, value);
  }

  bool extractAssignedUnsignedLong(const String &text, const char *key,
                                   unsigned long &value)
  {
    String pattern = String(key) + "=";
    int pos = text.indexOf(pattern);
    if (pos < 0)
    {
      return false;
    }

    String tail = text.substring(pos + pattern.length());
    tail.trim();
    return parseLeadingUnsignedLong(tail, value);
  }

  bool extractJsonBool(const String &text, const char *key, bool &value)
  {
    int keyPos = text.indexOf(key);
    if (keyPos < 0)
    {
      return false;
    }

    int colonPos = text.indexOf(':', keyPos);
    if (colonPos < 0)
    {
      return false;
    }

    String tail = text.substring(colonPos + 1);
    tail.trim();

    if (tail.startsWith("true"))
    {
      value = true;
      return true;
    }

    if (tail.startsWith("false"))
    {
      value = false;
      return true;
    }

    float parsed = 0.0f;
    if (!parseLeadingFloat(tail, parsed))
    {
      return false;
    }

    value = parsed != 0.0f;
    return true;
  }

  bool parsePumpRemoteCommand(const String &rawCmd, float &thrust,
                              unsigned long &durationMs, bool &hasVolumeLimit,
                              bool &volumeLimitEnabled)
  {
    String cmd = rawCmd;
    cmd.trim();

    if (cmd.length() == 0)
    {
      return false;
    }

    if (cmd.startsWith("pump"))
    {
      cmd.remove(0, 4);
      cmd.trim();
      if (cmd.startsWith(":") || cmd.startsWith(","))
      {
        cmd.remove(0, 1);
        cmd.trim();
      }
    }

    String lower = cmd;
    lower.toLowerCase();

    hasVolumeLimit = false;
    if (extractAssignedBool(lower, "limit", volumeLimitEnabled) ||
        extractAssignedBool(lower, "volume_limit", volumeLimitEnabled))
    {
      hasVolumeLimit = true;
    }

    lower.replace(",", " ");
    lower.replace(":", " ");
    lower.replace(";", " ");
    lower.trim();

    float parsedThrust = 0.0f;
    unsigned long parsedDurationMs = 0;
    if (sscanf(lower.c_str(), "%f %lu", &parsedThrust, &parsedDurationMs) != 2)
    {
      return false;
    }

    if (parsedThrust < -1.0f || parsedThrust > 1.0f || parsedDurationMs == 0)
    {
      return false;
    }

    thrust = parsedThrust;
    durationMs = parsedDurationMs;
    return true;
  }

  MissionCommand parseMissionCommand(const String &rawCmd,
                                     unsigned long defaultForceDrainAfterMs,
                                     unsigned long defaultForceDrainDurationMs)
  {
    MissionCommand result;
    result.params = defaultControlParams();
    result.forceDrainAfterMs = defaultForceDrainAfterMs;
    result.forceDrainDurationMs = defaultForceDrainDurationMs;

    String cmd = rawCmd;
    cmd.trim();
    if (cmd.length() == 0)
    {
      return result;
    }

    String lower = cmd;
    lower.toLowerCase();

    if (lower == "status" || lower == "depth" || lower == "depth?")
    {
      result.type = MissionCommand::STATUS;
      return result;
    }

    if (lower == "surface" || lower == "stop" || lower == "abort")
    {
      result.type = MissionCommand::SURFACE;
      return result;
    }

    float targetDepthM = 0.0f;
    if (extractJsonFloat(lower, "target_depth_m", targetDepthM) ||
        extractJsonFloat(lower, "depth_m", targetDepthM))
    {
      extractJsonFloat(lower, "kp", result.params.kp);
      extractJsonFloat(lower, "kd", result.params.kd);
      extractJsonBool(lower, "pulse_enable", result.params.pulseEnabled);
      extractJsonFloat(lower, "pulse_window_m", result.params.pulseWindowM);
      extractJsonUnsignedLong(lower, "pulse_min_on_ms",
                              result.params.pulseMinOnMs);
      extractJsonUnsignedLong(lower, "pulse_max_on_ms",
                              result.params.pulseMaxOnMs);
      extractJsonUnsignedLong(lower, "pulse_off_ms",
                              result.params.pulseOffMs);
      extractJsonFloat(lower, "pulse_coast_rate_mps",
                       result.params.pulseCoastRateMps);
      extractJsonFloat(lower, "pulse_cmd", result.params.pulseCmd);
      extractJsonBool(lower, "lead_enable", result.params.leadEnabled);
      extractJsonFloat(lower, "lead_gain", result.params.leadGain);
      extractJsonFloat(lower, "lead_tau_s", result.params.leadTauS);
      extractJsonFloat(lower, "lead_alpha", result.params.leadAlpha);
      extractJsonUnsignedLong(lower, "force_drain_after_ms",
                              result.forceDrainAfterMs);
      extractJsonUnsignedLong(lower, "drain_after_ms",
                              result.forceDrainAfterMs);
      extractJsonUnsignedLong(lower, "force_drain_duration_ms",
                              result.forceDrainDurationMs);
      extractJsonUnsignedLong(lower, "drain_duration_ms",
                              result.forceDrainDurationMs);
      result.type = MissionCommand::START;
      result.targetDepthM = targetDepthM;
      return result;
    }

    if (lower.startsWith("start") || lower.startsWith("dive") ||
        lower.startsWith("target"))
    {
      extractAssignedFloat(lower, "kp", result.params.kp);
      extractAssignedFloat(lower, "kd", result.params.kd);
      extractAssignedBool(lower, "pulse_enable", result.params.pulseEnabled);
      extractAssignedFloat(lower, "pulse_window_m", result.params.pulseWindowM);
      extractAssignedUnsignedLong(lower, "pulse_min_on_ms",
                                  result.params.pulseMinOnMs);
      extractAssignedUnsignedLong(lower, "pulse_max_on_ms",
                                  result.params.pulseMaxOnMs);
      extractAssignedUnsignedLong(lower, "pulse_off_ms",
                                  result.params.pulseOffMs);
      extractAssignedFloat(lower, "pulse_coast_rate_mps",
                           result.params.pulseCoastRateMps);
      extractAssignedFloat(lower, "pulse_cmd", result.params.pulseCmd);
      extractAssignedBool(lower, "lead_enable", result.params.leadEnabled);
      extractAssignedFloat(lower, "lead_gain", result.params.leadGain);
      extractAssignedFloat(lower, "lead_tau_s", result.params.leadTauS);
      extractAssignedFloat(lower, "lead_alpha", result.params.leadAlpha);
      extractAssignedUnsignedLong(lower, "force_drain_after_ms",
                                  result.forceDrainAfterMs);
      extractAssignedUnsignedLong(lower, "drain_after_ms",
                                  result.forceDrainAfterMs);
      extractAssignedUnsignedLong(lower, "force_drain_duration_ms",
                                  result.forceDrainDurationMs);
      extractAssignedUnsignedLong(lower, "drain_duration_ms",
                                  result.forceDrainDurationMs);
      result.type = parseLeadingFloat(lower, targetDepthM) ? MissionCommand::START
                                                           : MissionCommand::INVALID;
      result.targetDepthM = targetDepthM;
      return result;
    }

    if (parseLeadingFloat(lower, targetDepthM))
    {
      result.type = MissionCommand::START;
      result.targetDepthM = targetDepthM;
      return result;
    }

    result.type = MissionCommand::INVALID;
    return result;
  }

  bool publishMissionStatus(MqttLink &mqtt, const char *state,
                            unsigned long missionTimeMs, float depthM,
                            float targetDepthM, size_t sampleCount,
                            size_t uploadIndex,
                            unsigned long forceDrainAfterMs,
                            unsigned long forceDrainDurationMs)
  {
    String payload;
    payload.reserve(380);
    payload += "{\"state\":\"";
    payload += state;
    payload += "\",\"time_ms\":";
    payload += String(missionTimeMs);
    payload += ",\"depth_m\":";
    payload += String(depthM, 3);
    payload += ",\"target_depth_m\":";
    payload += String(targetDepthM, 3);
    payload += ",\"force_drain_after_ms\":";
    payload += String(forceDrainAfterMs);
    payload += ",\"force_drain_duration_ms\":";
    payload += String(forceDrainDurationMs);
    payload += ",\"history_count\":";
    payload += String((unsigned long)sampleCount);
    payload += ",\"upload_index\":";
    payload += String((unsigned long)uploadIndex);
    payload += "}";

    return mqtt.publishRaw(MQTT_TOPIC_STATUS, payload.c_str());
  }

  bool publishMissionParams(MqttLink &mqtt, const Control::Params &params)
  {
    String payload;
    payload.reserve(280);
    payload += "{\"kp\":";
    payload += String(params.kp, 3);
    payload += ",\"kd\":";
    payload += String(params.kd, 3);
    payload += ",\"pulse_enable\":";
    payload += String(params.pulseEnabled ? 1 : 0);
    payload += ",\"pulse_window_m\":";
    payload += String(params.pulseWindowM, 3);
    payload += ",\"pulse_min_on_ms\":";
    payload += String(params.pulseMinOnMs);
    payload += ",\"pulse_max_on_ms\":";
    payload += String(params.pulseMaxOnMs);
    payload += ",\"pulse_off_ms\":";
    payload += String(params.pulseOffMs);
    payload += ",\"pulse_coast_rate_mps\":";
    payload += String(params.pulseCoastRateMps, 3);
    payload += ",\"pulse_cmd\":";
    payload += String(params.pulseCmd, 3);
    payload += ",\"lead_enable\":";
    payload += String(params.leadEnabled ? 1 : 0);
    payload += ",\"lead_gain\":";
    payload += String(params.leadGain, 3);
    payload += ",\"lead_tau_s\":";
    payload += String(params.leadTauS, 3);
    payload += ",\"lead_alpha\":";
    payload += String(params.leadAlpha, 3);
    payload += "}";

    return mqtt.publishRaw(MQTT_TOPIC_PARAM, payload.c_str());
  }
}

void debugBegin(MqttLink *mqtt)
{
  g_mqtt = mqtt;
}

void debugInfo(const String &msg)
{
  debugWrite("INFO", msg);
}

void debugWarn(const String &msg)
{
  debugWrite("WARN", msg);
}

void debugError(const String &msg)
{
  debugWrite("ERROR", msg);
}

void debugMQTT(MqttLink &mqtt, bool &running, unsigned long &counter, unsigned long &lastSendMs)
{
  mqtt.update();

  if (mqtt.hasNewCommand(MQTT_TOPIC_CMD_COUNTER))
  {
    String cmd = mqtt.latestCommand(MQTT_TOPIC_CMD_COUNTER);

    if (cmd == "start")
    {
      running = true;
      Serial.println("cmd=start");
    }
    else if (cmd == "stop")
    {
      running = false;
      Serial.println("cmd=stop");
    }
    else if (cmd == "clear")
    {
      counter = 0;
      Serial.println("cmd=clear");
    }
    else
    {
      Serial.print("unknown cmd: ");
      Serial.println(cmd);
    }

    mqtt.clearCommand(MQTT_TOPIC_CMD_COUNTER);
  }

  unsigned long now = millis();
  if (running && now - lastSendMs >= 1000)
  {
    lastSendMs = now;
    counter++;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", counter);

    if (!mqtt.publishRaw(MQTT_TOPIC_COUNTER, buf))
    {
      Serial.println("publish counter failed");
    }
  }
}

void debugMotorRemote(MqttLink &mqtt, MotorDriver &motor)
{
  static bool motorRunning = false;
  static float currentSpeed = 0.0f;
  static unsigned long stopAtMs = 0;

  mqtt.update();

  unsigned long now = millis();

  if (motorRunning && (long)(now - stopAtMs) >= 0)
  {
    motor.stop();
    motorRunning = false;

    String msg = "motor stop timeout speed=";
    msg += String(currentSpeed, 3);
    debugInfo(msg);
  }

  if (!mqtt.hasNewCommand(MQTT_TOPIC_CMD_MOTOR))
  {
    return;
  }

  String cmd = mqtt.latestCommand(MQTT_TOPIC_CMD_MOTOR);
  mqtt.clearCommand(MQTT_TOPIC_CMD_MOTOR);
  cmd.trim();

  if (cmd.equalsIgnoreCase("stop") || cmd.equalsIgnoreCase("motor stop") ||
      cmd.equalsIgnoreCase("motor_stop"))
  {
    motor.stop();
    motorRunning = false;
    currentSpeed = 0.0f;
    stopAtMs = 0;
    debugWarn("motor stop by remote cmd");
    return;
  }

  float speed = 0.0f;
  unsigned long durationMs = 0;
  if (!parseMotorRemoteCommand(cmd, speed, durationMs))
  {
    debugError("invalid motor cmd, use: <speed>,<duration_ms> or stop");
    return;
  }

  motor.setThrust(speed);
  motorRunning = true;
  currentSpeed = speed;
  stopAtMs = now + durationMs;

  String msg = "motor start speed=";
  msg += String(speed, 3);
  msg += ", duration_ms=";
  msg += String(durationMs);
  debugInfo(msg);
}

void debugFakeHistoryUpload(MqttLink &mqtt)
{
  static bool announced = false;
  static bool started = false;
  static bool completed = false;
  static unsigned long lastSendMs = 0;
  static unsigned long sampleIdx = 0;

  mqtt.update();

  if (!announced)
  {
    announced = true;
    debugInfo("fake history upload armed, waiting for MQTT");
  }

  if (completed || !mqtt.isMqttConnected())
  {
    return;
  }

  unsigned long now = millis();

  if (!started)
  {
    started = true;
    lastSendMs = now;
    String msg = "fake surfaced history upload started -> ";
    msg += MQTT_TOPIC_HISTORY;
    msg += ", samples=";
    msg += String(kFakeHistorySampleCount);
    msg += ", send_interval_ms=";
    msg += String(kFakeHistoryIntervalMs);
    debugInfo(msg);
  }

  if (now - lastSendMs < kFakeHistoryIntervalMs)
  {
    return;
  }

  lastSendMs = now;

  unsigned long sampleTimeMs = 0;
  if (kFakeHistorySampleCount > 1)
  {
    sampleTimeMs =
        (sampleIdx * kFakeMissionDurationMs) / (kFakeHistorySampleCount - 1);
  }

  float depth = fakeDepthAt(sampleTimeMs);
  if (!mqtt.publishDepthSample(sampleIdx, sampleTimeMs, depth, 0.0f))
  {
    debugError("fake history upload publish failed");
    return;
  }

  sampleIdx++;

  if (sampleIdx >= kFakeHistorySampleCount)
  {
    completed = true;
    String msg = "fake surfaced history upload completed, samples=";
    msg += String(kFakeHistorySampleCount);
    debugInfo(msg);
  }
}

void debugDepthMission(MqttLink &mqtt, SensorDriver &sensor, Pump &pump,
                       Control &control, bool enableVolumeLimit,
                       unsigned long forceDrainAfterMs,
                       unsigned long forceDrainDurationMs)
{
  enum Phase
  {
    WAIT_START_CMD,
    CONTROL_TO_DEPTH,
    FORCE_DRAIN,
    WAIT_UPLOAD_LINK,
    UPLOADING_HISTORY
  };

  static Phase phase = WAIT_START_CMD;
  static unsigned long missionStartMs = 0;
  static unsigned long drainStartMs = 0;
  static unsigned long lastReadyStatusMs = 0;
  static unsigned long lastLogMs = 0;
  static unsigned long lastUploadMs = 0;
  static size_t sampleCount = 0;
  static size_t uploadIndex = 0;
  static bool warnedHistoryFull = false;
  static bool readyAnnounced = false;
  static float targetDepthM = 0.0f;
  static unsigned long activeForceDrainAfterMs = 0;
  static unsigned long activeForceDrainDurationMs = 0;
  static Control::Params activeParams = defaultControlParams();
  static DebugDepthSample history[kDebugMaxHistorySamples];

  pump.setVolumeLimitEnabled(enableVolumeLimit);

  if (phase == WAIT_START_CMD || phase == WAIT_UPLOAD_LINK ||
      phase == UPLOADING_HISTORY)
  {
    mqtt.update();
  }
  sensor.update();

  unsigned long now = millis();
  float depth = sensor.getDepthFilter();

  auto missionElapsedMs = [&]() -> unsigned long
  {
    return missionStartMs == 0 ? 0 : now - missionStartMs;
  };

  auto logHistorySample = [&](bool force, float controlOutput)
  {
    if (!force && now - lastLogMs < kDebugHistoryLogIntervalMs)
    {
      return;
    }

    unsigned long timeMs = missionElapsedMs();
    lastLogMs = now;

    if (appendDepthSample(history, kDebugMaxHistorySamples, sampleCount, timeMs,
                          depth, controlOutput))
    {
      return;
    }

    if (!warnedHistoryFull)
    {
      warnedHistoryFull = true;
      debugWarn("depth history buffer full, dropping later samples");
    }
  };

  auto publishReadyStatus = [&](bool force)
  {
    if (!mqtt.isMqttConnected())
    {
      return;
    }

    if (!force && now - lastReadyStatusMs < kDebugReadyStatusIntervalMs)
    {
      return;
    }

    lastReadyStatusMs = now;
    publishMissionStatus(mqtt, "ready", 0, depth, 0.0f, 0, 0,
                         forceDrainAfterMs, forceDrainDurationMs);
  };

  auto startMission = [&](const MissionCommand &cmd)
  {
    missionStartMs = now;
    drainStartMs = 0;
    lastLogMs = 0;
    lastUploadMs = 0;
    sampleCount = 0;
    uploadIndex = 0;
    warnedHistoryFull = false;
    targetDepthM = cmd.targetDepthM;
    activeParams = cmd.params;
    activeForceDrainAfterMs = cmd.forceDrainAfterMs;
    activeForceDrainDurationMs = cmd.forceDrainDurationMs;

    control.setParams(activeParams);
    control.setEnabled(true);
    control.setTargetDepth(targetDepthM);
    control.reset(depth, now);

    pump.stop();

    logHistorySample(true, control.getLastCommand());
    publishMissionParams(mqtt, activeParams);
    publishMissionStatus(mqtt, "started", 0, depth, targetDepthM, sampleCount,
                         0, activeForceDrainAfterMs,
                         activeForceDrainDurationMs);

    String msg = "depth mission start, target_depth_m=";
    msg += String(targetDepthM, 3);
    msg += ", kp=";
    msg += String(activeParams.kp, 3);
    msg += ", kd=";
    msg += String(activeParams.kd, 3);
    msg += ", lead_enable=";
    msg += String(activeParams.leadEnabled ? 1 : 0);
    msg += ", lead_gain=";
    msg += String(activeParams.leadGain, 3);
    msg += ", lead_tau_s=";
    msg += String(activeParams.leadTauS, 3);
    msg += ", lead_alpha=";
    msg += String(activeParams.leadAlpha, 3);
    msg += ", force_drain_after_ms=";
    msg += String(activeForceDrainAfterMs);
    msg += ", force_drain_duration_ms=";
    msg += String(activeForceDrainDurationMs);
    msg += ", cmd_topic=";
    msg += MQTT_TOPIC_CMD_DEBUG_MISSION;
    debugInfo(msg);
    phase = CONTROL_TO_DEPTH;
  };

  auto forceDrain = [&](const String &reason)
  {
    control.setEnabled(false);
    drainStartMs = now;
    pump.setCommand(-1.0f);
    pump.update();
    logHistorySample(true, -1.0f);
    publishMissionStatus(mqtt, "ascending", missionElapsedMs(), depth,
                         targetDepthM, sampleCount, uploadIndex,
                         activeForceDrainAfterMs,
                         activeForceDrainDurationMs);
    debugWarn(reason);
    phase = FORCE_DRAIN;
  };

  auto handlePendingCommand = [&]()
  {
    if (!mqtt.hasNewCommand(MQTT_TOPIC_CMD_DEBUG_MISSION))
    {
      return;
    }

    String rawCmd = mqtt.latestCommand(MQTT_TOPIC_CMD_DEBUG_MISSION);
    mqtt.clearCommand(MQTT_TOPIC_CMD_DEBUG_MISSION);

    MissionCommand cmd =
        parseMissionCommand(rawCmd, forceDrainAfterMs, forceDrainDurationMs);
    switch (cmd.type)
    {
    case MissionCommand::NONE:
      return;

    case MissionCommand::STATUS:
    {
      publishReadyStatus(true);
      String msg = "current depth_m=";
      msg += String(depth, 3);
      msg += ", status_topic=";
      msg += MQTT_TOPIC_STATUS;
      debugInfo(msg);
      return;
    }

    case MissionCommand::START:
      startMission(cmd);
      return;

    case MissionCommand::SURFACE:
      debugWarn("surface cmd ignored: mission not started");
      return;

    case MissionCommand::INVALID:
      debugError(
          "invalid mission cmd, use start:<depth_m>,kp=<v>,kd=<v>,lead_enable=<0|1>,lead_gain=<v>,lead_tau_s=<s>,lead_alpha=<v> or {\"target_depth_m\":<depth_m>}");
      return;
    }
  };

  switch (phase)
  {
  case WAIT_START_CMD:
    pump.setCommand(0.0f);
    pump.update();
    control.setEnabled(false);

    if (!readyAnnounced)
    {
      readyAnnounced = true;
      publishReadyStatus(true);

      String msg = "depth mission ready, current_depth_m=";
      msg += String(depth, 3);
      msg += ", wait cmd on ";
      msg += MQTT_TOPIC_CMD_DEBUG_MISSION;
      debugInfo(msg);
    }

    publishReadyStatus(false);
    handlePendingCommand();
    break;

  case CONTROL_TO_DEPTH:
    control.apply(pump, depth, now);
    pump.update();
    logHistorySample(false, control.getLastCommand());

    if (now - missionStartMs >= activeForceDrainAfterMs)
    {
      String msg = "depth mission: force drain after ";
      msg += String(activeForceDrainAfterMs);
      msg += " ms, duration_ms=";
      msg += String(activeForceDrainDurationMs);
      forceDrain(msg);
    }
    break;

  case FORCE_DRAIN:
    pump.setCommand(-1.0f);
    pump.update();
    logHistorySample(false, -1.0f);

    if (now - drainStartMs >= activeForceDrainDurationMs)
    {
      pump.stop();
      logHistorySample(true, 0.0f);
      publishMissionStatus(mqtt, "surfaced", missionElapsedMs(), depth,
                           targetDepthM, sampleCount, 0,
                           activeForceDrainAfterMs,
                           activeForceDrainDurationMs);
      debugInfo("depth mission: force drain complete, preparing history upload");
      phase = WAIT_UPLOAD_LINK;
    }
    break;

  case WAIT_UPLOAD_LINK:
    pump.setCommand(0.0f);
    pump.update();

    if (!mqtt.isMqttConnected())
    {
      return;
    }

    lastUploadMs = now;
    uploadIndex = 0;
    publishMissionStatus(mqtt, "uploading", missionElapsedMs(), depth,
                         targetDepthM, sampleCount, uploadIndex,
                         activeForceDrainAfterMs,
                         activeForceDrainDurationMs);
    debugInfo("depth mission: MQTT ready, upload history begin");
    phase = UPLOADING_HISTORY;
    break;

  case UPLOADING_HISTORY:
    if (!mqtt.isMqttConnected())
    {
      phase = WAIT_UPLOAD_LINK;
      debugWarn("depth mission: MQTT lost during upload, waiting to resume");
      return;
    }

    if (uploadIndex >= sampleCount)
    {
      publishMissionStatus(mqtt, "complete", missionElapsedMs(), depth,
                           targetDepthM, sampleCount, uploadIndex,
                           activeForceDrainAfterMs,
                           activeForceDrainDurationMs);
      debugInfo("depth mission: history upload complete");
      phase = WAIT_START_CMD;
      missionStartMs = 0;
      lastReadyStatusMs = 0;
      readyAnnounced = false;
      activeForceDrainAfterMs = forceDrainAfterMs;
      activeForceDrainDurationMs = forceDrainDurationMs;
      activeParams = defaultControlParams();
      return;
    }

    if (now - lastUploadMs < kDebugUploadIntervalMs)
    {
      return;
    }

    if (!mqtt.publishDepthSample((unsigned long)uploadIndex,
                                 history[uploadIndex].timeMs,
                                 history[uploadIndex].depthM,
                                 history[uploadIndex].controlOutput))
    {
      return;
    }

    lastUploadMs = now;
    uploadIndex++;
    break;
  }
}

void debugSensor(SensorDriver &sensor)
{
  static unsigned long lastPrintMs = 0;
  const unsigned long printIntervalMs = 500;

  unsigned long now = millis();

  // 1. 更新感知
  sensor.update();

  if (now - lastPrintMs < printIntervalMs)
  {
    return;
  }
  lastPrintMs = now;

  // 2. 读取数据
  float depth = sensor.getDepth();
  float pressure = sensor.getPressure();
  float temp = sensor.getTemp();

  // 3. 拼接日志
  String msg;
  msg.reserve(96);

  msg += "[Time: ";
  msg += String(now / 1000.0, 2);
  msg += "s] ";

  msg += "Depth: ";
  msg += String(depth, 3);
  msg += " m | ";

  msg += "Pressure: ";
  msg += String(pressure, 1);
  msg += " mbar | ";

  msg += "Temp: ";
  msg += String(temp, 2);
  msg += " C";

  debugInfo(msg);
}

void debugPumpRemote(MqttLink &mqtt, Pump &pump)
{
  static bool pumpRunning = false;
  static bool volumeLimitEnabled = true;
  static float currentThrust = 0.0f;
  static unsigned long stopAtMs = 0;
  static unsigned long lastPrintMs = 0;
  const unsigned long printIntervalMs = 500; // 每 0.5 秒打印一次

  mqtt.update();

  unsigned long now = millis();

  if (pumpRunning && (long)(now - stopAtMs) >= 0)
  {
    pumpRunning = false;
    currentThrust = 0.0f;

    String msg = "pump stop timeout";
    msg += ", volume_limit=";
    msg += String(volumeLimitEnabled ? 1 : 0);
    debugInfo(msg);
  }

  pump.setVolumeLimitEnabled(volumeLimitEnabled);
  pump.setCommand(pumpRunning ? currentThrust : 0.0f);
  pump.update();

  if (now - lastPrintMs >= printIntervalMs)
  {
    lastPrintMs = now;

    String msg;
    msg.reserve(96);
    msg += "pump volume=";
    msg += String(pump.getEstimatedVolumeMl(), 2);
    msg += " ml, upper=";
    msg += String(pump.isAtUpperLimit() ? 1 : 0);
    msg += ", lower=";
    msg += String(pump.isAtLowerLimit() ? 1 : 0);
    msg += ", running=";
    msg += String(pumpRunning ? 1 : 0);
    msg += ", thrust=";
    msg += String(currentThrust, 3);
    msg += ", volume_limit=";
    msg += String(volumeLimitEnabled ? 1 : 0);

    debugInfo(msg);
  }

  if (!mqtt.hasNewCommand(MQTT_TOPIC_CMD_PUMP))
  {
    return;
  }

  String cmd = mqtt.latestCommand(MQTT_TOPIC_CMD_PUMP);
  mqtt.clearCommand(MQTT_TOPIC_CMD_PUMP);
  cmd.trim();

  if (cmd.equalsIgnoreCase("stop") || cmd.equalsIgnoreCase("pump stop") ||
      cmd.equalsIgnoreCase("pump_stop"))
  {
    pumpRunning = false;
    currentThrust = 0.0f;
    pump.setCommand(0.0f);
    pump.update();
    debugWarn("pump stop by remote cmd");
    return;
  }

  float thrust = 0.0f;
  unsigned long durationMs = 0;
  bool hasVolumeLimit = false;
  bool requestedVolumeLimit = volumeLimitEnabled;

  if (!parsePumpRemoteCommand(cmd, thrust, durationMs, hasVolumeLimit,
                              requestedVolumeLimit))
  {
    debugError(
        "invalid pump cmd, use: pump:<thrust>,<duration_ms>,limit=<0|1> or stop");
    return;
  }

  if (hasVolumeLimit)
  {
    volumeLimitEnabled = requestedVolumeLimit;
  }

  pump.setVolumeLimitEnabled(volumeLimitEnabled);
  pump.setCommand(thrust);
  pump.update();

  pumpRunning = true;
  currentThrust = thrust;
  stopAtMs = now + durationMs;

  String msg = "pump start thrust=";
  msg += String(thrust, 3);
  msg += ", duration_ms=";
  msg += String(durationMs);
  msg += ", volume_limit=";
  msg += String(volumeLimitEnabled ? 1 : 0);
  debugInfo(msg);
}
