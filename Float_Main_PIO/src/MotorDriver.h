#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "Config.h"

// 电机驱动类：基于 A4950 + ESP32 LEDC
class MotorDriver
{
private:
    int in1_ledc_channel_;
    int in2_ledc_channel_;

public:
    // PWM 配置常量
    static constexpr int PUMP_PWM_FREQ = 5000;    // 5 kHz
    static constexpr int PUMP_PWM_RESOLUTION = 8; // 8-bit
    static constexpr int PUMP_PWM_MAX_DUTY = (1 << PUMP_PWM_RESOLUTION) - 1;

    // 构造函数
    MotorDriver(int ch1 = 0, int ch2 = 1);

    // 初始化
    void init();

    // 设置归一化推力/占空比命令，范围 [-1.0, 1.0]
    void setThrust(float thrust);

    // 停止：IN1=0, IN2=0
    void stop();

    // 刹车：IN1=1, IN2=1（PWM拉满）
    void brake();
};

#endif