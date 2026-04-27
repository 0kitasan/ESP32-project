#include "Mission.h"

#include "Config.h"
#include "Debug.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

namespace
{
constexpr unsigned long kMissionReadyStatusIntervalMs = 2000;
constexpr unsigned long kMissionUploadIntervalMs = 10;
constexpr unsigned long kMissionDebugHistoryLogIntervalMs =
    MISSION_DEBUG_HISTORY_INTERVAL_MS;
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
  Mission::TaskConfig config;
  unsigned long unixTimeS = 0;
  bool hasUnixTimeS = false;
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

Mission::TaskConfig defaultTaskConfig()
{
  Mission::TaskConfig config;
  config.stage1TargetDepthM = MISSION_STAGE1_TARGET_DEPTH_M_DEFAULT;
  config.stage1HoldMs = MISSION_STAGE1_HOLD_MS_DEFAULT;
  config.stage2TargetDepthM = MISSION_STAGE2_TARGET_DEPTH_M_DEFAULT;
  config.stage2HoldMs = MISSION_STAGE2_HOLD_MS_DEFAULT;
  config.ballastFillDurationMs = MISSION_BALLAST_FILL_DURATION_MS_DEFAULT;
  config.trackingTimeoutEnabled = MISSION_TRACKING_TIMEOUT_ENABLE_DEFAULT != 0;
  config.trackingTimeoutMs = MISSION_TRACKING_TIMEOUT_MS_DEFAULT;
  config.surfaceDrainDurationMs = MISSION_SURFACE_DRAIN_DURATION_MS_DEFAULT;
  config.controlParams = defaultControlParams();
  return config;
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

bool extractMissionFloatJson(const String &text, Mission::TaskConfig &config)
{
  bool found = false;
  found |= extractJsonFloat(text, "stage1_depth_m", config.stage1TargetDepthM);
  found |= extractJsonFloat(text, "target_depth_1_m", config.stage1TargetDepthM);
  found |= extractJsonFloat(text, "deep_depth_m", config.stage1TargetDepthM);
  found |= extractJsonFloat(text, "stage2_depth_m", config.stage2TargetDepthM);
  found |= extractJsonFloat(text, "target_depth_2_m", config.stage2TargetDepthM);
  found |= extractJsonFloat(text, "shallow_depth_m", config.stage2TargetDepthM);
  return found;
}

bool extractMissionDurationJson(const String &text, Mission::TaskConfig &config)
{
  bool found = false;
  found |= extractJsonUnsignedLong(text, "stage1_hold_ms", config.stage1HoldMs);
  found |= extractJsonUnsignedLong(text, "hover_1_ms", config.stage1HoldMs);
  found |= extractJsonUnsignedLong(text, "deep_hold_ms", config.stage1HoldMs);
  found |= extractJsonUnsignedLong(text, "stage2_hold_ms", config.stage2HoldMs);
  found |= extractJsonUnsignedLong(text, "hover_2_ms", config.stage2HoldMs);
  found |= extractJsonUnsignedLong(text, "shallow_hold_ms", config.stage2HoldMs);
  found |= extractJsonUnsignedLong(text, "ballast_fill_duration_ms",
                                   config.ballastFillDurationMs);
  found |= extractJsonUnsignedLong(text, "initial_ballast_ms",
                                   config.ballastFillDurationMs);
  found |= extractJsonUnsignedLong(text, "initial_fill_ms",
                                   config.ballastFillDurationMs);
  found |= extractJsonUnsignedLong(text, "tracking_timeout_ms",
                                   config.trackingTimeoutMs);
  found |= extractJsonUnsignedLong(text, "stage_timeout_ms",
                                   config.trackingTimeoutMs);
  found |= extractJsonUnsignedLong(text, "surface_drain_duration_ms",
                                   config.surfaceDrainDurationMs);
  found |= extractJsonUnsignedLong(text, "force_drain_duration_ms",
                                   config.surfaceDrainDurationMs);
  found |= extractJsonUnsignedLong(text, "drain_duration_ms",
                                   config.surfaceDrainDurationMs);
  return found;
}

bool extractMissionFloatAssigned(const String &text, Mission::TaskConfig &config)
{
  bool found = false;
  found |=
      extractAssignedFloat(text, "stage1_depth_m", config.stage1TargetDepthM);
  found |=
      extractAssignedFloat(text, "target_depth_1_m", config.stage1TargetDepthM);
  found |= extractAssignedFloat(text, "deep_depth_m", config.stage1TargetDepthM);
  found |=
      extractAssignedFloat(text, "stage2_depth_m", config.stage2TargetDepthM);
  found |=
      extractAssignedFloat(text, "target_depth_2_m", config.stage2TargetDepthM);
  found |=
      extractAssignedFloat(text, "shallow_depth_m", config.stage2TargetDepthM);
  return found;
}

bool extractMissionDurationAssigned(const String &text,
                                    Mission::TaskConfig &config)
{
  bool found = false;
  found |=
      extractAssignedUnsignedLong(text, "stage1_hold_ms", config.stage1HoldMs);
  found |= extractAssignedUnsignedLong(text, "hover_1_ms", config.stage1HoldMs);
  found |= extractAssignedUnsignedLong(text, "deep_hold_ms", config.stage1HoldMs);
  found |=
      extractAssignedUnsignedLong(text, "stage2_hold_ms", config.stage2HoldMs);
  found |= extractAssignedUnsignedLong(text, "hover_2_ms", config.stage2HoldMs);
  found |=
      extractAssignedUnsignedLong(text, "shallow_hold_ms", config.stage2HoldMs);
  found |= extractAssignedUnsignedLong(text, "ballast_fill_duration_ms",
                                       config.ballastFillDurationMs);
  found |= extractAssignedUnsignedLong(text, "initial_ballast_ms",
                                       config.ballastFillDurationMs);
  found |= extractAssignedUnsignedLong(text, "initial_fill_ms",
                                       config.ballastFillDurationMs);
  found |= extractAssignedUnsignedLong(text, "tracking_timeout_ms",
                                       config.trackingTimeoutMs);
  found |= extractAssignedUnsignedLong(text, "stage_timeout_ms",
                                       config.trackingTimeoutMs);
  found |= extractAssignedUnsignedLong(text, "surface_drain_duration_ms",
                                       config.surfaceDrainDurationMs);
  found |= extractAssignedUnsignedLong(text, "force_drain_duration_ms",
                                       config.surfaceDrainDurationMs);
  found |= extractAssignedUnsignedLong(text, "drain_duration_ms",
                                       config.surfaceDrainDurationMs);
  return found;
}

bool extractMissionTimeJson(const String &text, unsigned long &unixTimeS)
{
  bool found = false;
  found |= extractJsonUnsignedLong(text, "utc_epoch_s", unixTimeS);
  found |= extractJsonUnsignedLong(text, "unix_time", unixTimeS);
  found |= extractJsonUnsignedLong(text, "epoch", unixTimeS);
  return found;
}

bool extractMissionTimeAssigned(const String &text, unsigned long &unixTimeS)
{
  bool found = false;
  found |= extractAssignedUnsignedLong(text, "utc_epoch_s", unixTimeS);
  found |= extractAssignedUnsignedLong(text, "unix_time", unixTimeS);
  found |= extractAssignedUnsignedLong(text, "epoch", unixTimeS);
  return found;
}

bool extractMissionBoolJson(const String &text, Mission::TaskConfig &config)
{
  bool found = false;
  found |= extractJsonBool(text, "tracking_timeout_enable",
                           config.trackingTimeoutEnabled);
  found |= extractJsonBool(text, "tracking_timeout_enabled",
                           config.trackingTimeoutEnabled);
  found |= extractJsonBool(text, "stage_timeout_enable",
                           config.trackingTimeoutEnabled);
  found |= extractJsonBool(text, "stage_timeout_enabled",
                           config.trackingTimeoutEnabled);
  return found;
}

bool extractMissionBoolAssigned(const String &text, Mission::TaskConfig &config)
{
  bool found = false;
  found |= extractAssignedBool(text, "tracking_timeout_enable",
                               config.trackingTimeoutEnabled);
  found |= extractAssignedBool(text, "tracking_timeout_enabled",
                               config.trackingTimeoutEnabled);
  found |= extractAssignedBool(text, "stage_timeout_enable",
                               config.trackingTimeoutEnabled);
  found |= extractAssignedBool(text, "stage_timeout_enabled",
                               config.trackingTimeoutEnabled);
  return found;
}

bool extractControlJson(const String &text, Control::Params &params)
{
  bool found = false;
  found |= extractJsonFloat(text, "kp", params.kp);
  found |= extractJsonFloat(text, "kd", params.kd);
  found |= extractJsonBool(text, "pulse_enable", params.pulseEnabled);
  found |= extractJsonFloat(text, "pulse_window_m", params.pulseWindowM);
  found |= extractJsonUnsignedLong(text, "pulse_min_on_ms",
                                   params.pulseMinOnMs);
  found |= extractJsonUnsignedLong(text, "pulse_max_on_ms",
                                   params.pulseMaxOnMs);
  found |= extractJsonUnsignedLong(text, "pulse_off_ms", params.pulseOffMs);
  found |= extractJsonFloat(text, "pulse_coast_rate_mps",
                            params.pulseCoastRateMps);
  found |= extractJsonFloat(text, "pulse_cmd", params.pulseCmd);
  found |= extractJsonBool(text, "lead_enable", params.leadEnabled);
  found |= extractJsonFloat(text, "lead_gain", params.leadGain);
  found |= extractJsonFloat(text, "lead_tau_s", params.leadTauS);
  found |= extractJsonFloat(text, "lead_alpha", params.leadAlpha);
  return found;
}

bool extractControlAssigned(const String &text, Control::Params &params)
{
  bool found = false;
  found |= extractAssignedFloat(text, "kp", params.kp);
  found |= extractAssignedFloat(text, "kd", params.kd);
  found |= extractAssignedBool(text, "pulse_enable", params.pulseEnabled);
  found |= extractAssignedFloat(text, "pulse_window_m", params.pulseWindowM);
  found |= extractAssignedUnsignedLong(text, "pulse_min_on_ms",
                                       params.pulseMinOnMs);
  found |= extractAssignedUnsignedLong(text, "pulse_max_on_ms",
                                       params.pulseMaxOnMs);
  found |= extractAssignedUnsignedLong(text, "pulse_off_ms",
                                       params.pulseOffMs);
  found |= extractAssignedFloat(text, "pulse_coast_rate_mps",
                                params.pulseCoastRateMps);
  found |= extractAssignedFloat(text, "pulse_cmd", params.pulseCmd);
  found |= extractAssignedBool(text, "lead_enable", params.leadEnabled);
  found |= extractAssignedFloat(text, "lead_gain", params.leadGain);
  found |= extractAssignedFloat(text, "lead_tau_s", params.leadTauS);
  found |= extractAssignedFloat(text, "lead_alpha", params.leadAlpha);
  return found;
}

MissionCommand parseMissionCommand(const String &rawCmd)
{
  MissionCommand result;
  result.config = defaultTaskConfig();

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

  if (lower.startsWith("{"))
  {
    bool hasMissionParams = false;
    hasMissionParams |= extractMissionFloatJson(lower, result.config);
    hasMissionParams |= extractMissionDurationJson(lower, result.config);
    hasMissionParams |= extractMissionBoolJson(lower, result.config);
    hasMissionParams |= extractControlJson(lower, result.config.controlParams);
    result.hasUnixTimeS = extractMissionTimeJson(lower, result.unixTimeS);

    bool hasStartToken = lower.indexOf("\"start\"") >= 0 ||
                         lower.indexOf("\"command\":\"start\"") >= 0 ||
                         lower.indexOf("\"command\": \"start\"") >= 0;

    result.type = (hasStartToken || hasMissionParams) ? MissionCommand::START
                                                      : MissionCommand::INVALID;
    return result;
  }

  if (lower.startsWith("start") || lower.startsWith("mission") ||
      lower.startsWith("task"))
  {
    extractMissionFloatAssigned(lower, result.config);
    extractMissionDurationAssigned(lower, result.config);
    extractMissionBoolAssigned(lower, result.config);
    extractControlAssigned(lower, result.config.controlParams);
    result.hasUnixTimeS = extractMissionTimeAssigned(lower, result.unixTimeS);
    result.type = MissionCommand::START;
    return result;
  }

  result.type = MissionCommand::INVALID;
  return result;
}
} // namespace

Mission::Mission(SensorDriver &sensor, Pump &pump, MqttLink &mqtt,
                 Control &control)
    : sensor_(sensor), pump_(pump), mqtt_(mqtt), control_(control),
      phase_(WAIT_START_CMD), missionStartMs_(0), phaseStartMs_(0),
      surfaceStartMs_(0), lastReadyStatusMs_(0), lastUploadMs_(0),
      lastDebugLogMs_(0), sampleCount_(0), uploadIndex_(0),
      debugSampleCount_(0), debugUploadIndex_(0), holdSampleStartIndex_(0),
      holdPacketIndex_(0), warnedHistoryFull_(false),
      warnedDebugHistoryFull_(false), readyAnnounced_(false),
      activeConfig_(defaultTaskConfig())
{
}

void Mission::init()
{
  phase_ = WAIT_START_CMD;
  activeConfig_ = defaultTaskConfig();
  resetMissionRuntime();
  pump_.setVolumeLimitEnabled(false);
  phaseStartMs_ = millis();
  readyAnnounced_ = false;
  String msg = "mission init complete, wait cmd on ";
  msg += MQTT_TOPIC_CMD_MISSION;
  debugInfo(msg);
}

void Mission::resetMissionRuntime()
{
  missionStartMs_ = 0;
  surfaceStartMs_ = 0;
  lastReadyStatusMs_ = 0;
  lastUploadMs_ = 0;
  lastDebugLogMs_ = 0;
  sampleCount_ = 0;
  uploadIndex_ = 0;
  debugSampleCount_ = 0;
  debugUploadIndex_ = 0;
  holdSampleStartIndex_ = 0;
  holdPacketIndex_ = 0;
  warnedHistoryFull_ = false;
  warnedDebugHistoryFull_ = false;
}

void Mission::enterPhase(Phase newPhase, unsigned long nowMs)
{
  phase_ = newPhase;
  phaseStartMs_ = nowMs;
}

unsigned long Mission::missionElapsedMs(unsigned long nowMs) const
{
  return missionStartMs_ == 0 ? 0 : nowMs - missionStartMs_;
}

Mission::RuntimeSample Mission::acquireRuntimeSample()
{
  sensor_.update();

  RuntimeSample sample;
  sample.nowMs = millis();
  sample.depthM = sensor_.getDepthFilter();
  sample.pressureKPa = sensor_.getPressure() / 10.0f;
  return sample;
}

bool Mission::handleCommand(float depthM, unsigned long nowMs)
{
  if (!mqtt_.hasNewCommand(MQTT_TOPIC_CMD_MISSION))
  {
    return false;
  }

  String rawCmd = mqtt_.latestCommand(MQTT_TOPIC_CMD_MISSION);
  mqtt_.clearCommand(MQTT_TOPIC_CMD_MISSION);

  MissionCommand cmd = parseMissionCommand(rawCmd);
  switch (cmd.type)
  {
  case MissionCommand::STATUS:
    publishCurrentStatus(depthM, nowMs);
    return false;

  case MissionCommand::START:
    if (phase_ != WAIT_START_CMD)
    {
      debugWarn("start cmd ignored: mission already active");
      publishCurrentStatus(depthM, nowMs);
      return false;
    }

    if (cmd.hasUnixTimeS && cmd.unixTimeS >= 946684800UL)
    {
      timeval tv;
      tv.tv_sec = (time_t)cmd.unixTimeS;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);

      String msg = "mission clock set from cmd, utc_epoch_s=";
      msg += String(cmd.unixTimeS);
      debugInfo(msg);
    }

    startMission(cmd.config, depthM, nowMs);
    return true;

  case MissionCommand::SURFACE:
    if (phase_ == WAIT_START_CMD)
    {
      debugWarn("surface cmd ignored: mission not started");
      return false;
    }

    if (phase_ == FORCE_SURFACE)
    {
      debugWarn("surface cmd ignored: mission already surfacing");
      return false;
    }

    if (phase_ == WAIT_UPLOAD_LINK || phase_ == UPLOADING_HISTORY)
    {
      debugWarn("surface cmd ignored: mission already surfaced");
      publishCurrentStatus(depthM, nowMs);
      return false;
    }

    startForceSurface(depthM, nowMs, "mission surface cmd");
    return true;

  case MissionCommand::INVALID:
    debugError(
        "invalid mission cmd, use start,stage1_depth_m=<m>,stage1_hold_ms=<ms>,stage2_depth_m=<m>,stage2_hold_ms=<ms>,ballast_fill_duration_ms=<ms>,tracking_timeout_enable=<0|1>,tracking_timeout_ms=<ms>,utc_epoch_s=<s>,kp=<v>,kd=<v>,lead_* or JSON");
    return false;

  case MissionCommand::NONE:
  default:
    return false;
  }
}

void Mission::publishCurrentStatus(float depthM, unsigned long nowMs)
{
  switch (phase_)
  {
  case WAIT_START_CMD:
    publishReadyStatus(depthM, true, nowMs);
    break;

  case INITIAL_BALLAST:
    publishStatus("ballasting", depthM, activeConfig_.stage1TargetDepthM, nowMs,
                  1);
    break;

  case STAGE1_TO_DEPTH:
    publishStatus("stage1_tracking", depthM, activeConfig_.stage1TargetDepthM,
                  nowMs, 1);
    break;

  case HOLD_STAGE1:
    publishStatus("stage1_hold", depthM, activeConfig_.stage1TargetDepthM,
                  nowMs, 1);
    break;

  case STAGE2_TO_DEPTH:
    publishStatus("stage2_tracking", depthM, activeConfig_.stage2TargetDepthM,
                  nowMs, 2);
    break;

  case HOLD_STAGE2:
    publishStatus("stage2_hold", depthM, activeConfig_.stage2TargetDepthM,
                  nowMs, 2);
    break;

  case FORCE_SURFACE:
    publishStatus("surfacing", depthM, 0.0f, nowMs, 0);
    break;

  case WAIT_UPLOAD_LINK:
    publishStatus("surfaced", depthM, 0.0f, nowMs, 0);
    break;

  case UPLOADING_HISTORY:
    publishStatus("uploading", depthM, 0.0f, nowMs, 0);
    break;
  }
}

void Mission::resetHoldReportCollection()
{
  holdSampleStartIndex_ = sampleCount_;
  holdPacketIndex_ = 0;
}

void Mission::rollbackHoldReportCollection()
{
  sampleCount_ = holdSampleStartIndex_;
  holdPacketIndex_ = 0;
}

bool Mission::reachedHoldEntryThreshold(float depthM, float targetDepthM,
                                        int stage) const
{
  float enterBandM = activeConfig_.controlParams.holdEnterBandM;

  if (stage == 1)
  {
    return depthM >= (targetDepthM - enterBandM);
  }

  if (stage == 2)
  {
    return depthM <= (targetDepthM + enterBandM);
  }

  return fabs(depthM - targetDepthM) <= enterBandM;
}

void Mission::collectMissionReportData(const RuntimeSample &sample, int stage)
{
  while (holdPacketIndex_ < MISSION_REPORT_PACKET_COUNT &&
         (sample.nowMs - phaseStartMs_) >=
             holdPacketIndex_ * MISSION_REPORT_INTERVAL_MS)
  {
    if (sampleCount_ >= kHistoryCapacity)
    {
      if (!warnedHistoryFull_)
      {
        warnedHistoryFull_ = true;
        debugWarn("mission report buffer full, dropping later samples");
      }
      return;
    }

    time_t unixTime = time(nullptr);
    if (unixTime >= 946684800)
    {
      struct tm utcTime;
      gmtime_r(&unixTime, &utcTime);
      strftime(history_[sampleCount_].utcTime,
               sizeof(history_[sampleCount_].utcTime), "%H:%M:%S UTC",
               &utcTime);
    }
    else
    {
      strncpy(history_[sampleCount_].utcTime, "00:00:00 UTC",
              sizeof(history_[sampleCount_].utcTime));
      history_[sampleCount_].utcTime[sizeof(history_[sampleCount_].utcTime) - 1] =
          '\0';
    }

    history_[sampleCount_].stage = stage;
    history_[sampleCount_].depthM = sample.depthM;
    history_[sampleCount_].pressureKPa = sample.pressureKPa;
    sampleCount_++;
    holdPacketIndex_++;
  }
}

String Mission::buildHistoryLine(size_t index) const
{
  String line;
  line.reserve(64);
  line += MISSION_TEAM_ID;
  line += " ";
  line += history_[index].utcTime;
  line += " ";
  line += String(history_[index].pressureKPa, 1);
  line += " kpa ";
  line += String(history_[index].depthM, 2);
  line += " meters";
  return line;
}

void Mission::collectDebugHistorySample(unsigned long nowMs, float depthM,
                                        float controlOutput, bool force)
{
  if (!force && nowMs - lastDebugLogMs_ < kMissionDebugHistoryLogIntervalMs)
  {
    return;
  }

  lastDebugLogMs_ = nowMs;

  if (debugSampleCount_ >= kDebugHistoryCapacity)
  {
    if (!warnedDebugHistoryFull_)
    {
      warnedDebugHistoryFull_ = true;
      debugWarn("mission debug history buffer full, dropping later samples");
    }
    return;
  }

  debugHistory_[debugSampleCount_].timeMs = missionElapsedMs(nowMs);
  debugHistory_[debugSampleCount_].depthM = depthM;
  debugHistory_[debugSampleCount_].controlOutput = controlOutput;
  debugSampleCount_++;
}

String Mission::buildDebugHistoryLine(size_t index) const
{
  String payload;
  payload.reserve(112);
  payload += "{\"idx\":";
  payload += String((unsigned long)index);
  payload += ",\"time_ms\":";
  payload += String(debugHistory_[index].timeMs);
  payload += ",\"depth_m\":";
  payload += String(debugHistory_[index].depthM, 3);
  payload += ",\"control_output\":";
  payload += String(debugHistory_[index].controlOutput, 3);
  payload += "}";
  return payload;
}

void Mission::publishReadyStatus(float depthM, bool force, unsigned long nowMs)
{
  if (!mqtt_.isMqttConnected())
  {
    return;
  }

  if (!force && nowMs - lastReadyStatusMs_ < kMissionReadyStatusIntervalMs)
  {
    return;
  }

  lastReadyStatusMs_ = nowMs;
  publishStatus("ready", depthM, 0.0f, nowMs, 0);
}

void Mission::publishStatus(const char *state, float depthM, float targetDepthM,
                            unsigned long nowMs, int stage)
{
  String payload;
  payload.reserve(224);
  payload += "{\"state\":\"";
  payload += state;
  payload += "\",\"time_ms\":";
  payload += String(missionElapsedMs(nowMs));
  payload += ",\"depth_m\":";
  payload += String(depthM, 3);
  payload += ",\"target_depth_m\":";
  payload += String(targetDepthM, 3);
  payload += ",\"stage\":";
  payload += String(stage);
  payload += ",\"history_count\":";
  payload += String((unsigned long)sampleCount_);
  payload += ",\"upload_index\":";
  payload += String((unsigned long)uploadIndex_);
  payload += "}";

  mqtt_.publishRaw(MQTT_TOPIC_STATUS, payload.c_str());
}

void Mission::publishParams()
{
  const Control::Params &params = activeConfig_.controlParams;

  String payload;
  payload.reserve(420);
  payload += "{\"stage1_depth_m\":";
  payload += String(activeConfig_.stage1TargetDepthM, 3);
  payload += ",\"stage1_hold_ms\":";
  payload += String(activeConfig_.stage1HoldMs);
  payload += ",\"stage2_depth_m\":";
  payload += String(activeConfig_.stage2TargetDepthM, 3);
  payload += ",\"stage2_hold_ms\":";
  payload += String(activeConfig_.stage2HoldMs);
  payload += ",\"ballast_fill_duration_ms\":";
  payload += String(activeConfig_.ballastFillDurationMs);
  payload += ",\"tracking_timeout_enable\":";
  payload += String(activeConfig_.trackingTimeoutEnabled ? 1 : 0);
  payload += ",\"tracking_timeout_ms\":";
  payload += String(activeConfig_.trackingTimeoutMs);
  payload += ",\"surface_drain_duration_ms\":";
  payload += String(activeConfig_.surfaceDrainDurationMs);
  payload += ",\"kp\":";
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

  mqtt_.publishRaw(MQTT_TOPIC_PARAM, payload.c_str());
}

void Mission::startMission(const TaskConfig &config, float depthM,
                           unsigned long nowMs)
{
  activeConfig_ = config;
  resetMissionRuntime();
  missionStartMs_ = nowMs;

  pump_.setVolumeLimitEnabled(false);
  control_.setParams(activeConfig_.controlParams);
  control_.setEnabled(false);
  pump_.stop();
  collectDebugHistorySample(nowMs, depthM,
                            activeConfig_.ballastFillDurationMs > 0 ? 1.0f
                                                                    : 0.0f,
                            true);
  publishParams();
  publishStatus("started", depthM, activeConfig_.stage1TargetDepthM, nowMs, 1);

  String msg = "mission start, stage1_depth_m=";
  msg += String(activeConfig_.stage1TargetDepthM, 3);
  msg += ", stage1_hold_ms=";
  msg += String(activeConfig_.stage1HoldMs);
  msg += ", stage2_depth_m=";
  msg += String(activeConfig_.stage2TargetDepthM, 3);
  msg += ", stage2_hold_ms=";
  msg += String(activeConfig_.stage2HoldMs);
  msg += ", ballast_fill_duration_ms=";
  msg += String(activeConfig_.ballastFillDurationMs);
  msg += ", tracking_timeout_enable=";
  msg += String(activeConfig_.trackingTimeoutEnabled ? 1 : 0);
  msg += ", tracking_timeout_ms=";
  msg += String(activeConfig_.trackingTimeoutMs);
  msg += ", surface_drain_duration_ms=";
  msg += String(activeConfig_.surfaceDrainDurationMs);
  debugInfo(msg);

  if (activeConfig_.ballastFillDurationMs == 0)
  {
    startStage1Tracking(depthM, nowMs);
    return;
  }

  publishStatus("ballasting", depthM, activeConfig_.stage1TargetDepthM, nowMs,
                1);
  debugInfo("mission initial ballast start");
  enterPhase(INITIAL_BALLAST, nowMs);
}

void Mission::startStage1Tracking(float depthM, unsigned long nowMs)
{
  control_.setEnabled(true);
  control_.setTargetDepth(activeConfig_.stage1TargetDepthM);
  control_.reset(depthM, nowMs);
  collectDebugHistorySample(nowMs, depthM, control_.getLastCommand(), true);
  publishStatus("stage1_tracking", depthM, activeConfig_.stage1TargetDepthM,
                nowMs, 1);
  debugInfo("mission stage1 start");
  enterPhase(STAGE1_TO_DEPTH, nowMs);
}

void Mission::startStage2(float depthM, unsigned long nowMs)
{
  control_.setTargetDepth(activeConfig_.stage2TargetDepthM);
  control_.reset(depthM, nowMs);
  collectDebugHistorySample(nowMs, depthM, control_.getLastCommand(), true);
  publishStatus("stage2_tracking", depthM, activeConfig_.stage2TargetDepthM,
                nowMs, 2);
  debugInfo("mission stage2 start");
  enterPhase(STAGE2_TO_DEPTH, nowMs);
}

void Mission::startForceSurface(float depthM, unsigned long nowMs,
                                const String &reason)
{
  control_.setEnabled(false);
  surfaceStartMs_ = nowMs;
  pump_.setCommand(-1.0f);
  pump_.update();
  collectDebugHistorySample(nowMs, depthM, -1.0f, true);
  publishStatus("surfacing", depthM, 0.0f, nowMs, 0);
  debugWarn(reason);
  enterPhase(FORCE_SURFACE, nowMs);
}

void Mission::handleWaiting(float depthM, unsigned long nowMs)
{
  pump_.setCommand(0.0f);
  pump_.update();
  control_.setEnabled(false);

  if (!readyAnnounced_)
  {
    readyAnnounced_ = true;
    publishReadyStatus(depthM, true, nowMs);
    String msg = "mission ready, current_depth_m=";
    msg += String(depthM, 3);
    msg += ", wait cmd on ";
    msg += MQTT_TOPIC_CMD_MISSION;
    debugInfo(msg);
  }

  publishReadyStatus(depthM, false, nowMs);
}

void Mission::handleInitialBallast(float depthM, unsigned long nowMs)
{
  control_.setEnabled(false);
  pump_.setCommand(1.0f);
  pump_.update();
  collectDebugHistorySample(nowMs, depthM, 1.0f, false);

  if (nowMs - phaseStartMs_ < activeConfig_.ballastFillDurationMs)
  {
    return;
  }

  pump_.stop();
  debugInfo("mission initial ballast complete");
  startStage1Tracking(depthM, nowMs);
}

void Mission::handleStageTracking(float depthM, unsigned long nowMs,
                                  Phase holdPhase, float targetDepthM,
                                  int stage)
{
  control_.apply(pump_, depthM, nowMs);
  pump_.update();
  collectDebugHistorySample(nowMs, depthM, control_.getLastCommand(), false);

  if (!reachedHoldEntryThreshold(depthM, targetDepthM, stage))
  {
    if (activeConfig_.trackingTimeoutEnabled &&
        activeConfig_.trackingTimeoutMs > 0 &&
        nowMs - phaseStartMs_ >= activeConfig_.trackingTimeoutMs)
    {
      if (stage == 1)
      {
        debugWarn("mission stage1 tracking timeout, skip to stage2");
        startStage2(depthM, nowMs);
        return;
      }

      debugWarn("mission stage2 tracking timeout, force surface");
      startForceSurface(depthM, nowMs,
                        "mission stage2 tracking timeout, surfacing");
      return;
    }

    return;
  }

  RuntimeSample sample{nowMs, depthM, sensor_.getPressure() / 10.0f};
  enterPhase(holdPhase, nowMs);
  resetHoldReportCollection();
  collectMissionReportData(sample, stage);
  String state = stage == 1 ? "stage1_hold" : "stage2_hold";
  publishStatus(state.c_str(), depthM, targetDepthM, nowMs, stage);
  debugInfo(state + " entered");
}

void Mission::handleStageHold(float depthM, unsigned long nowMs,
                              Phase fallbackPhase, float targetDepthM,
                              unsigned long holdMs, int stage)
{
  control_.apply(pump_, depthM, nowMs);
  pump_.update();
  collectDebugHistorySample(nowMs, depthM, control_.getLastCommand(), false);
  RuntimeSample sample{nowMs, depthM, sensor_.getPressure() / 10.0f};

  if (!control_.isHolding())
  {
    // 暂时保持在 hold 阶段，不因为掉出带宽就退回 tracking。
    // String state = stage == 1 ? "stage1_tracking" : "stage2_tracking";
    // publishStatus(state.c_str(), depthM, targetDepthM, nowMs, stage);
    // debugWarn(state + " resumed: hold lost");
    // enterPhase(fallbackPhase, nowMs);
    // return;
  }

  collectMissionReportData(sample, stage);

  if (nowMs - phaseStartMs_ < holdMs)
  {
    return;
  }

  if (stage == 1)
  {
    startStage2(depthM, nowMs);
    return;
  }

  startForceSurface(depthM, nowMs, "mission stage2 hold complete, surfacing");
}

void Mission::handleForceSurface(float depthM, unsigned long nowMs)
{
  pump_.setCommand(-1.0f);
  pump_.update();
  collectDebugHistorySample(nowMs, depthM, -1.0f, false);

  if (nowMs - surfaceStartMs_ < activeConfig_.surfaceDrainDurationMs)
  {
    return;
  }

  pump_.stop();
  collectDebugHistorySample(nowMs, depthM, 0.0f, true);
  publishStatus("surfaced", depthM, 0.0f, nowMs, 0);
  debugInfo("mission surface drain complete, waiting upload link");
  enterPhase(WAIT_UPLOAD_LINK, nowMs);
}

void Mission::handleWaitingUpload(float depthM, unsigned long nowMs)
{
  pump_.setCommand(0.0f);
  pump_.update();

  if (!mqtt_.isMqttConnected())
  {
    return;
  }

  lastUploadMs_ = nowMs;
  uploadIndex_ = 0;
  debugUploadIndex_ = 0;
  publishStatus("uploading", depthM, 0.0f, nowMs, 0);
  String msg = "mission MQTT ready, upload history begin, history_count=";
  msg += String((unsigned long)sampleCount_);
  msg += ", debug_history_count=";
  msg += String((unsigned long)debugSampleCount_);
  debugInfo(msg);
  if (sampleCount_ == 0)
  {
    debugWarn(
        "mission history is empty: formal mission only records packets while hold is maintained");
  }
  enterPhase(UPLOADING_HISTORY, nowMs);
}

void Mission::handleUploading(float depthM, unsigned long nowMs)
{
  if (!mqtt_.isMqttConnected())
  {
    debugWarn("mission MQTT lost during upload, waiting to resume");
    enterPhase(WAIT_UPLOAD_LINK, nowMs);
    return;
  }

  bool officialDone = uploadIndex_ >= sampleCount_;
  bool debugDone = debugUploadIndex_ >= debugSampleCount_;
  if (officialDone && debugDone)
  {
    publishStatus("complete", depthM, 0.0f, nowMs, 0);
    debugInfo("mission history upload complete");
    activeConfig_ = defaultTaskConfig();
    readyAnnounced_ = false;
    enterPhase(WAIT_START_CMD, nowMs);
    resetMissionRuntime();
    return;
  }

  if (nowMs - lastUploadMs_ < kMissionUploadIntervalMs)
  {
    return;
  }

  if (!officialDone)
  {
    String line = buildHistoryLine(uploadIndex_);
    if (!mqtt_.publishRaw(MQTT_TOPIC_HISTORY, line.c_str()))
    {
      return;
    }

    lastUploadMs_ = nowMs;
    uploadIndex_++;
    return;
  }

  String line = buildDebugHistoryLine(debugUploadIndex_);
  if (!mqtt_.publishRaw(MQTT_TOPIC_HISTORY_DEBUG, line.c_str()))
  {
    return;
  }

  lastUploadMs_ = nowMs;
  debugUploadIndex_++;
}

void Mission::update()
{
  bool allowReconnect = phase_ == WAIT_START_CMD || phase_ == WAIT_UPLOAD_LINK ||
                        phase_ == UPLOADING_HISTORY;
  mqtt_.update(allowReconnect);

  RuntimeSample sample = acquireRuntimeSample();
  unsigned long now = sample.nowMs;
  float depth = sample.depthM;

  if (handleCommand(depth, now))
  {
    return;
  }

  switch (phase_)
  {
  case WAIT_START_CMD:
    handleWaiting(depth, now);
    break;

  case INITIAL_BALLAST:
    handleInitialBallast(depth, now);
    break;

  case STAGE1_TO_DEPTH:
    handleStageTracking(depth, now, HOLD_STAGE1, activeConfig_.stage1TargetDepthM,
                        1);
    break;

  case HOLD_STAGE1:
    handleStageHold(depth, now, STAGE1_TO_DEPTH,
                    activeConfig_.stage1TargetDepthM, activeConfig_.stage1HoldMs,
                    1);
    break;

  case STAGE2_TO_DEPTH:
    handleStageTracking(depth, now, HOLD_STAGE2, activeConfig_.stage2TargetDepthM,
                        2);
    break;

  case HOLD_STAGE2:
    handleStageHold(depth, now, STAGE2_TO_DEPTH,
                    activeConfig_.stage2TargetDepthM, activeConfig_.stage2HoldMs,
                    2);
    break;

  case FORCE_SURFACE:
    handleForceSurface(depth, now);
    break;

  case WAIT_UPLOAD_LINK:
    handleWaitingUpload(depth, now);
    break;

  case UPLOADING_HISTORY:
    handleUploading(depth, now);
    break;
  }
}

Mission::Phase Mission::getPhase() const { return phase_; }
