#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "Config.h" // 确保这里定义了PIN_PUMP_IN1 和 PIN_PUMP_IN2

// 定义PWM通道、频率和分辨率
const int PUMP_PWM_FREQ = 5000;    // 5 kHz 频率
const int PUMP_PWM_RESOLUTION = 8; // 8位分辨率 (0-255)

class MotorDriver {
private:
    // 为每个PWM引脚分配一个LEDC通道
    int in1_ledc_channel; // 可以是0-15,根据LEDC通道可用性选择
    int in2_ledc_channel; // 两个引脚需要不同的通道

public:
    // 构造函数：这里指定默认参数，并在内部实现赋值
    MotorDriver(int ch1 = 0, int ch2 = 1) {
        in1_ledc_channel = ch1;
        in2_ledc_channel = ch2;
    }

    // 初始化方法
    void init() {
        Serial.print("Motor: Initializing DC Motor with A4950... ");

        pinMode(PIN_PUMP_IN1, OUTPUT);
        pinMode(PIN_PUMP_IN2, OUTPUT);

        // 【新版 ESP32 API (v3.x)】合并了 ledcSetup 和 ledcAttachPin
        ledcAttachChannel(PIN_PUMP_IN1, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION, in1_ledc_channel);
        ledcAttachChannel(PIN_PUMP_IN2, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION, in2_ledc_channel);

        Serial.println("Done.");
    }

    // 设置推力方法
    void setThrust(float thrust) {
        int dutyCycle_bits = (1 << PUMP_PWM_RESOLUTION) - 1; // PWM分辨率对应的最大值
        // 确保thrust在-1.0到1.0之间，防止越界
        thrust = constrain(thrust, -1.0, 1.0);

        if (thrust >= 0) { // 正向或停止
            int duty = map(thrust * 100, 0, 100, 0, dutyCycle_bits);
            // 【新版 ESP32 API (v3.x)】按通道写入需使用 ledcWriteChannel
            ledcWriteChannel(in1_ledc_channel, duty); // in1 控制推力
            ledcWriteChannel(in2_ledc_channel, 0);    // in2 保持关闭
        } else { // 反向
            int duty = map(-thrust * 100, 0, 100, 0, dutyCycle_bits);
            ledcWriteChannel(in1_ledc_channel, 0);    // in1 保持关闭
            ledcWriteChannel(in2_ledc_channel, duty); // in2 控制反向推力
        }
    }

    // 停止方法
    void stop() {
        // 让 IN1 和 IN2 都变成 0 (低电平)
        ledcWriteChannel(in1_ledc_channel, 0);
        ledcWriteChannel(in2_ledc_channel, 0);
    }
    // 刹车
    void brake() {
        int max_duty = (1 << PUMP_PWM_RESOLUTION) - 1; 
        // 让 IN1 和 IN2 都输出最大值 (高电平)
        ledcWriteChannel(in1_ledc_channel, max_duty);
        ledcWriteChannel(in2_ledc_channel, max_duty);
    }
};

#endif