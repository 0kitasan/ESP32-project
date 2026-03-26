#include "Pump.h"

Pump::Pump(MotorDriver &motor, bool inverted, float volume_min_ml,
           float volume_max_ml)
    : motor_(motor), inverted_(inverted), cmd_(0.0f), volume_est_ml_(0.0f),
      volume_min_ml_(volume_min_ml), volume_max_ml_(volume_max_ml),
      last_update_ms_(0) {}

void Pump::init() {
  motor_.init();
  last_update_ms_ = millis();
}

void Pump::setCommand(float cmd) { cmd_ = constrain(cmd, -1.0f, 1.0f); }

void Pump::update() {
  unsigned long now = millis();
  float dt_s = 0.0f;

  if (last_update_ms_ != 0) {
    dt_s = (now - last_update_ms_) / 1000.0f;
  }
  last_update_ms_ = now;

  // 1. 极性修正后的命令
  float actual_cmd = inverted_ ? -cmd_ : cmd_;

  // 2. 低于启动阈值则认为泵不转
  float effective_cmd = 0.0f;
  if (fabs(actual_cmd) >= PUMP_START_THRESHOLD) {
    effective_cmd = actual_cmd;
  }

  // 3. 只有真正能转时，才认为有流量、才做体积积分
  float flow_ml_per_s = effective_cmd * PUMP_MAX_FLOW;
  float next_volume_ml = volume_est_ml_ + flow_ml_per_s * dt_s;

  // 4. 上限硬保护：防止继续注水
  if (next_volume_ml >= volume_max_ml_ && effective_cmd > 0.0f) {
    effective_cmd = 0.0f;
    next_volume_ml = volume_max_ml_;
  }

  // 5. 下限只做夹紧，不强制阻止继续排水
  if (next_volume_ml <= volume_min_ml_) {
    next_volume_ml = volume_min_ml_;
  }

  // 6. 更新体积估计
  volume_est_ml_ = constrain(next_volume_ml, volume_min_ml_, volume_max_ml_);

  // 7. 下发到底层驱动
  if (effective_cmd == 0.0f) {
    motor_.stop();
  } else {
    motor_.setThrust(effective_cmd);
  }
}

void Pump::stop() {
  cmd_ = 0.0f;
  motor_.stop();
}

float Pump::getEstimatedVolumeMl() const { return volume_est_ml_; }

void Pump::setEstimatedVolumeMl(float volume_ml) {
  volume_est_ml_ = constrain(volume_ml, volume_min_ml_, volume_max_ml_);
}

void Pump::setInverted(bool inverted) { inverted_ = inverted; }

bool Pump::isInverted() const { return inverted_; }

bool Pump::isAtUpperLimit() const { return volume_est_ml_ >= volume_max_ml_; }

bool Pump::isAtLowerLimit() const { return volume_est_ml_ <= volume_min_ml_; }