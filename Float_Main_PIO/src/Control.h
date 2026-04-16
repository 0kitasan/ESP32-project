#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>
#include "Config.h"

class Pump;

class Control
{
public:
  struct Params
  {
    float kp = CTRL_KP_DEFAULT;
    float kd = CTRL_KD_DEFAULT;
    float outputLimit = CTRL_OUTPUT_LIMIT_DEFAULT;
    float minActuationCmd = CTRL_MIN_ACTUATION_CMD_DEFAULT;
    float holdEnterBandM = CTRL_HOLD_ENTER_BAND_M_DEFAULT;
    float holdExitBandM = CTRL_HOLD_EXIT_BAND_M_DEFAULT;
    float derivativeFilterAlpha = CTRL_DERIVATIVE_FILTER_ALPHA_DEFAULT;
    bool leadEnabled = CTRL_LEAD_ENABLE_DEFAULT != 0;
    float leadGain = CTRL_LEAD_GAIN_DEFAULT;
    float leadTauS = CTRL_LEAD_TAU_S_DEFAULT;
    float leadAlpha = CTRL_LEAD_ALPHA_DEFAULT;
  };

  Control();
  explicit Control(const Params &params);

  void reset(float currentDepthM);
  void reset(float currentDepthM, unsigned long nowMs);

  void setParams(const Params &params);
  const Params &getParams() const;

  void setTargetDepth(float targetDepthM);
  float getTargetDepth() const;

  void setEnabled(bool enabled);
  bool isEnabled() const;

  float update(float currentDepthM);
  float update(float currentDepthM, unsigned long nowMs);
  void apply(Pump &pump, float currentDepthM);
  void apply(Pump &pump, float currentDepthM, unsigned long nowMs);

  float getLastCommand() const;
  float getLastError() const;
  float getDepthRateMps() const;
  bool isHolding() const;

private:
  Params params_;
  float targetDepthM_;
  float lastDepthM_;
  float filteredDepthRateMps_;
  float lastCommand_;
  float lastError_;
  float lastLeadInput_;
  float lastLeadOutput_;
  unsigned long lastUpdateMs_;
  bool initialized_;
  bool enabled_;
  bool holding_;
  bool leadInitialized_;

  void sanitizeParams();
  void resetLeadCompensator();
  float leadCompensator(float input, float dtS);
};

#endif
