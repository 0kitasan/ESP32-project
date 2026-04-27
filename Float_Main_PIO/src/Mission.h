#ifndef MISSION_H
#define MISSION_H

#include <Arduino.h>

#include "Control.h"
#include "MqttLink.h"
#include "Pump.h"
#include "SensorDriver.h"

class Mission
{
public:
  enum Phase
  {
    WAIT_START_CMD,
    INITIAL_BALLAST,
    STAGE1_TO_DEPTH,
    HOLD_STAGE1,
    STAGE2_TO_DEPTH,
    HOLD_STAGE2,
    FORCE_SURFACE,
    WAIT_UPLOAD_LINK,
    UPLOADING_HISTORY
  };

  struct TaskConfig
  {
    float stage1TargetDepthM = MISSION_STAGE1_TARGET_DEPTH_M_DEFAULT;
    unsigned long stage1HoldMs = MISSION_STAGE1_HOLD_MS_DEFAULT;
    float stage2TargetDepthM = MISSION_STAGE2_TARGET_DEPTH_M_DEFAULT;
    unsigned long stage2HoldMs = MISSION_STAGE2_HOLD_MS_DEFAULT;
    unsigned long ballastFillDurationMs =
        MISSION_BALLAST_FILL_DURATION_MS_DEFAULT;
    bool trackingTimeoutEnabled =
        MISSION_TRACKING_TIMEOUT_ENABLE_DEFAULT != 0;
    unsigned long trackingTimeoutMs = MISSION_TRACKING_TIMEOUT_MS_DEFAULT;
    unsigned long surfaceDrainDurationMs =
        MISSION_SURFACE_DRAIN_DURATION_MS_DEFAULT;
    Control::Params controlParams;
  };

  Mission(SensorDriver &sensor, Pump &pump, MqttLink &mqtt, Control &control);

  void init();
  void update();

  Phase getPhase() const;

private:
  struct RuntimeSample
  {
    unsigned long nowMs;
    float depthM;
    float pressureKPa;
  };

  struct HistorySample
  {
    char utcTime[16];
    int stage;
    float depthM;
    float pressureKPa;
  };

  struct DebugHistorySample
  {
    unsigned long timeMs;
    float depthM;
    float controlOutput;
  };

  static constexpr size_t kHistoryCapacity =
      MISSION_REPORT_PACKET_COUNT * 2U;
  static constexpr size_t kDebugHistoryCapacity =
      MISSION_DEBUG_HISTORY_CAPACITY;

  SensorDriver &sensor_;
  Pump &pump_;
  MqttLink &mqtt_;
  Control &control_;

  Phase phase_;
  unsigned long missionStartMs_;
  unsigned long phaseStartMs_;
  unsigned long surfaceStartMs_;
  unsigned long lastReadyStatusMs_;
  unsigned long lastUploadMs_;
  unsigned long lastDebugLogMs_;
  size_t sampleCount_;
  size_t uploadIndex_;
  size_t debugSampleCount_;
  size_t debugUploadIndex_;
  size_t holdSampleStartIndex_;
  unsigned int holdPacketIndex_;
  bool warnedHistoryFull_;
  bool warnedDebugHistoryFull_;
  bool readyAnnounced_;

  TaskConfig activeConfig_;
  HistorySample history_[kHistoryCapacity];
  DebugHistorySample debugHistory_[kDebugHistoryCapacity];

  RuntimeSample acquireRuntimeSample();
  bool handleCommand(float depthM, unsigned long nowMs);
  void publishCurrentStatus(float depthM, unsigned long nowMs);
  void resetMissionRuntime();
  void enterPhase(Phase newPhase, unsigned long nowMs);
  unsigned long missionElapsedMs(unsigned long nowMs) const;
  void resetHoldReportCollection();
  void rollbackHoldReportCollection();
  bool reachedHoldEntryThreshold(float depthM, float targetDepthM,
                                 int stage) const;
  void collectMissionReportData(const RuntimeSample &sample, int stage);
  String buildHistoryLine(size_t index) const;
  void collectDebugHistorySample(unsigned long nowMs, float depthM,
                                 float controlOutput, bool force);
  String buildDebugHistoryLine(size_t index) const;
  void publishReadyStatus(float depthM, bool force, unsigned long nowMs);
  void publishStatus(const char *state, float depthM, float targetDepthM,
                     unsigned long nowMs, int stage);
  void publishParams();
  void startMission(const TaskConfig &config, float depthM, unsigned long nowMs);
  void startStage1Tracking(float depthM, unsigned long nowMs);
  void startStage2(float depthM, unsigned long nowMs);
  void startForceSurface(float depthM, unsigned long nowMs,
                         const String &reason);
  void handleWaiting(float depthM, unsigned long nowMs);
  void handleInitialBallast(float depthM, unsigned long nowMs);
  void handleStageTracking(float depthM, unsigned long nowMs, Phase holdPhase,
                           float targetDepthM, int stage);
  void handleStageHold(float depthM, unsigned long nowMs, Phase fallbackPhase,
                       float targetDepthM, unsigned long holdMs, int stage);
  void handleForceSurface(float depthM, unsigned long nowMs);
  void handleWaitingUpload(float depthM, unsigned long nowMs);
  void handleUploading(float depthM, unsigned long nowMs);
};

#endif
