#ifndef PUMP_H
#define PUMP_H

#include "Config.h"
#include "MotorDriver.h"
#include <Arduino.h>

class Pump
{
private:
  MotorDriver &motor_;

  // 极性：false=正常，true=反向
  bool inverted_;

  // 当前归一化命令 [-1, 1]
  float cmd_;

  // 体积估计（mL）
  float volume_est_ml_;

  // 上下限（mL）
  float volume_min_ml_;
  float volume_max_ml_;

  // 上一次 update 的时间
  unsigned long last_update_ms_;

public:
  Pump(MotorDriver &motor, bool inverted = false, float volume_min_ml = 0.0f,
       float volume_max_ml = PUMP_MAX_VOLUME);

  void init();

  // 设置归一化泵命令，范围 [-1, 1]
  void setCommand(float cmd);

  // 周期调用：执行积分、限幅、再下发给 MotorDriver
  void update();

  // 立即停止
  void stop();

  // 容积估计值读写
  float getEstimatedVolumeMl() const;
  void setEstimatedVolumeMl(float volume_ml);
  void setVolumeLimitEnabled(bool enabled);

  // 是否已经碰到上下限
  bool isAtUpperLimit() const;
  bool isAtLowerLimit() const;

private:
  bool volume_limit_enabled_;
};

#endif
