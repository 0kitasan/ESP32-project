#include "Control.h"

#include "Pump.h"

namespace
{
  float signOrZero(float value)
  {
    if (value > 0.0f)
    {
      return 1.0f;
    }
    if (value < 0.0f)
    {
      return -1.0f;
    }
    return 0.0f;
  }
}

Control::Control() : Control(Params{}) {}

Control::Control(const Params &params)
    : params_(params), targetDepthM_(0.0f), lastDepthM_(0.0f),
      filteredDepthRateMps_(0.0f), lastCommand_(0.0f), lastError_(0.0f),
      lastLeadInput_(0.0f), lastLeadOutput_(0.0f), pulseCommand_(0.0f),
      lastUpdateMs_(0), pulseActiveUntilMs_(0), pulseCooldownUntilMs_(0),
      initialized_(false), enabled_(true), holding_(false),
      leadInitialized_(false)
{
  sanitizeParams();
}

void Control::reset(float currentDepthM)
{
  reset(currentDepthM, millis());
}

void Control::reset(float currentDepthM, unsigned long nowMs)
{
  lastDepthM_ = currentDepthM;
  filteredDepthRateMps_ = 0.0f;
  lastCommand_ = 0.0f;
  lastError_ = targetDepthM_ - currentDepthM;
  lastUpdateMs_ = nowMs;
  initialized_ = true;
  holding_ = fabs(lastError_) <= params_.holdEnterBandM;
  resetLeadCompensator();
  resetPulseController();
}

void Control::setParams(const Params &params)
{
  params_ = params;
  sanitizeParams();
  resetPulseController();
}

const Control::Params &Control::getParams() const { return params_; }

void Control::setTargetDepth(float targetDepthM)
{
  targetDepthM_ = targetDepthM;
}

float Control::getTargetDepth() const { return targetDepthM_; }

void Control::setEnabled(bool enabled)
{
  enabled_ = enabled;
  if (!enabled_)
  {
    lastCommand_ = 0.0f;
    resetLeadCompensator();
    resetPulseController();
  }
}

bool Control::isEnabled() const { return enabled_; }

float Control::update(float currentDepthM)
{
  return update(currentDepthM, millis());
}

float Control::update(float currentDepthM, unsigned long nowMs)
{
  if (!initialized_)
  {
    reset(currentDepthM, nowMs);
  }

  float dtS = 0.0f;
  if (nowMs > lastUpdateMs_)
  {
    dtS = (nowMs - lastUpdateMs_) / 1000.0f;
  }

  if (dtS > 0.0f)
  {
    float rawDepthRate = (currentDepthM - lastDepthM_) / dtS;
    filteredDepthRateMps_ +=
        params_.derivativeFilterAlpha * (rawDepthRate - filteredDepthRateMps_);
  }

  lastDepthM_ = currentDepthM;
  lastUpdateMs_ = nowMs;
  lastError_ = targetDepthM_ - currentDepthM;

  float absError = fabs(lastError_);
  if (holding_)
  {
    if (absError > params_.holdExitBandM)
    {
      holding_ = false;
    }
  }
  else if (absError <= params_.holdEnterBandM)
  {
    holding_ = true;
  }

  if (!enabled_ || holding_)
  {
    lastCommand_ = 0.0f;
    resetLeadCompensator();
    resetPulseController();
    return lastCommand_;
  }

  float rawCommand =
      params_.kp * lastError_ - params_.kd * filteredDepthRateMps_;
  rawCommand = leadCompensator(rawCommand, dtS);
  rawCommand = constrain(rawCommand, -params_.outputLimit, params_.outputLimit);

  if (params_.pulseEnabled && params_.pulseWindowM > 0.0f &&
      fabs(lastError_) <= params_.pulseWindowM)
  {
    // Near the target, use short pulses instead of continuously driving the
    // pump at its start threshold. This matches the pump's large dead zone
    // better and leaves time for the float to respond between corrections.
    if (signOrZero(pulseCommand_) != 0.0f &&
        signOrZero(lastError_) != signOrZero(pulseCommand_))
    {
      resetPulseController();
    }

    if (pulseActiveUntilMs_ > nowMs && pulseCommand_ != 0.0f)
    {
      lastCommand_ = pulseCommand_;
      return lastCommand_;
    }

    pulseCommand_ = 0.0f;

    bool movingTowardTarget =
        signOrZero(lastError_) == signOrZero(filteredDepthRateMps_) &&
        fabs(filteredDepthRateMps_) >= params_.pulseCoastRateMps;
    if (movingTowardTarget || pulseCooldownUntilMs_ > nowMs)
    {
      lastCommand_ = 0.0f;
      return lastCommand_;
    }

    float pulseSign = signOrZero(lastError_);
    if (pulseSign != 0.0f)
    {
      float pulseAlpha = constrain(fabs(lastError_) / params_.pulseWindowM,
                                   0.0f, 1.0f);
      unsigned long pulseOnMs = params_.pulseMinOnMs;
      if (params_.pulseMaxOnMs > params_.pulseMinOnMs)
      {
        pulseOnMs += (unsigned long)((params_.pulseMaxOnMs -
                                      params_.pulseMinOnMs) *
                                     pulseAlpha);
      }

      pulseCommand_ = constrain(pulseSign * params_.pulseCmd,
                                -params_.outputLimit, params_.outputLimit);
      pulseActiveUntilMs_ = nowMs + pulseOnMs;
      pulseCooldownUntilMs_ = pulseActiveUntilMs_ + params_.pulseOffMs;
      lastCommand_ = pulseCommand_;
      return lastCommand_;
    }
  }
  else
  {
    resetPulseController();
  }

  if (params_.minActuationCmd > 0.0f && fabs(rawCommand) < params_.minActuationCmd)
  {
    if (signOrZero(rawCommand) == signOrZero(lastError_))
    {
      rawCommand = signOrZero(lastError_) * params_.minActuationCmd;
    }
  }

  lastCommand_ = constrain(rawCommand, -params_.outputLimit, params_.outputLimit);
  return lastCommand_;
}

void Control::apply(Pump &pump, float currentDepthM)
{
  apply(pump, currentDepthM, millis());
}

void Control::apply(Pump &pump, float currentDepthM, unsigned long nowMs)
{
  pump.setCommand(update(currentDepthM, nowMs));
}

float Control::getLastCommand() const { return lastCommand_; }

float Control::getLastError() const { return lastError_; }

float Control::getDepthRateMps() const { return filteredDepthRateMps_; }

bool Control::isHolding() const { return holding_; }

void Control::sanitizeParams()
{
  params_.kp = max(0.0f, params_.kp);
  params_.kd = max(0.0f, params_.kd);
  params_.outputLimit = constrain(params_.outputLimit, 0.0f, 1.0f);
  params_.minActuationCmd =
      constrain(params_.minActuationCmd, 0.0f, params_.outputLimit);
  params_.holdEnterBandM = max(0.0f, params_.holdEnterBandM);
  params_.holdExitBandM = max(params_.holdEnterBandM, params_.holdExitBandM);
  params_.derivativeFilterAlpha =
      constrain(params_.derivativeFilterAlpha, 0.0f, 1.0f);
  params_.pulseWindowM = max(0.0f, params_.pulseWindowM);
  params_.pulseMinOnMs = max(1UL, params_.pulseMinOnMs);
  params_.pulseMaxOnMs = max(params_.pulseMinOnMs, params_.pulseMaxOnMs);
  params_.pulseOffMs = max(0UL, params_.pulseOffMs);
  params_.pulseCoastRateMps = max(0.0f, params_.pulseCoastRateMps);
  params_.pulseCmd = constrain(params_.pulseCmd, 0.0f, params_.outputLimit);
  params_.leadGain = max(0.0f, params_.leadGain);
  params_.leadTauS = max(0.0f, params_.leadTauS);
  params_.leadAlpha = constrain(params_.leadAlpha, 0.0f, 1.0f);
}

void Control::resetLeadCompensator()
{
  lastLeadInput_ = 0.0f;
  lastLeadOutput_ = 0.0f;
  leadInitialized_ = false;
}

void Control::resetPulseController()
{
  pulseCommand_ = 0.0f;
  pulseActiveUntilMs_ = 0;
  pulseCooldownUntilMs_ = 0;
}

float Control::leadCompensator(float input, float dtS)
{
  if (!params_.leadEnabled || params_.leadGain <= 0.0f ||
      params_.leadTauS <= 0.0f || params_.leadAlpha <= 0.0f)
  {
    lastLeadInput_ = input;
    lastLeadOutput_ = input;
    leadInitialized_ = true;
    return input;
  }

  if (!leadInitialized_ || dtS <= 0.0f)
  {
    lastLeadInput_ = input;
    lastLeadOutput_ = params_.leadGain * input;
    leadInitialized_ = true;
    return lastLeadOutput_;
  }

  float denom = 2.0f * params_.leadAlpha * params_.leadTauS + dtS;
  if (denom <= 0.0f)
  {
    lastLeadInput_ = input;
    lastLeadOutput_ = params_.leadGain * input;
    return lastLeadOutput_;
  }

  float b0 =
      params_.leadGain * (2.0f * params_.leadTauS + dtS) / denom;
  float b1 =
      params_.leadGain * (dtS - 2.0f * params_.leadTauS) / denom;
  float a1 = (dtS - 2.0f * params_.leadAlpha * params_.leadTauS) / denom;

  float output =
      b0 * input + b1 * lastLeadInput_ - a1 * lastLeadOutput_;

  lastLeadInput_ = input;
  lastLeadOutput_ = output;
  return output;
}
