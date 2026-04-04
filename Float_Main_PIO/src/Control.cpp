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
      lastUpdateMs_(0), initialized_(false), enabled_(true), holding_(false)
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
}

void Control::setParams(const Params &params)
{
  params_ = params;
  sanitizeParams();
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
    return lastCommand_;
  }

  float rawCommand = params_.kp * lastError_ - params_.kd * filteredDepthRateMps_;
  rawCommand = constrain(rawCommand, -params_.outputLimit, params_.outputLimit);

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
}
