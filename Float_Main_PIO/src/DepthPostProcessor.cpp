#include "DepthPostProcessor.h"

#include <math.h>

namespace
{
// Keep defaults as pass-through. If sensor jitter needs taming later,
// adjust only this module instead of scattering logic across mission code.
constexpr float kDefaultDepthJitterBandM = 0.0f;
constexpr float kDefaultDepthSmoothingAlpha = 1.0f;
}

DepthPostProcessor::DepthPostProcessor()
    : config_{kDefaultDepthJitterBandM, kDefaultDepthSmoothingAlpha},
      lastOutputM_(0.0f), ready_(false)
{
}

void DepthPostProcessor::reset()
{
    lastOutputM_ = 0.0f;
    ready_ = false;
}

void DepthPostProcessor::setConfig(const Config &config)
{
    config_ = config;
}

float DepthPostProcessor::apply(float filteredDepthM)
{
    if (!ready_)
    {
        lastOutputM_ = filteredDepthM;
        ready_ = true;
        return lastOutputM_;
    }

    float candidate = filteredDepthM;

    if (config_.jitterBandM > 0.0f &&
        fabsf(candidate - lastOutputM_) <= config_.jitterBandM)
    {
        candidate = lastOutputM_;
    }

    if (config_.smoothingAlpha > 0.0f && config_.smoothingAlpha < 1.0f)
    {
        lastOutputM_ += config_.smoothingAlpha * (candidate - lastOutputM_);
    }
    else
    {
        lastOutputM_ = candidate;
    }

    return lastOutputM_;
}
