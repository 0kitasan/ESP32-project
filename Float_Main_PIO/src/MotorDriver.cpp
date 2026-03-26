#include "MotorDriver.h"

MotorDriver::MotorDriver(int ch1, int ch2)
    : in1_ledc_channel_(ch1), in2_ledc_channel_(ch2) {}

void MotorDriver::init()
{
    Serial.print("Motor: Initializing DC Motor with A4950... ");

    pinMode(PIN_PUMP_IN1, OUTPUT);
    pinMode(PIN_PUMP_IN2, OUTPUT);

    // ESP32 Arduino Core v3.x API
    ledcAttachChannel(PIN_PUMP_IN1, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION, in1_ledc_channel_);
    ledcAttachChannel(PIN_PUMP_IN2, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION, in2_ledc_channel_);

    Serial.println("Done.");
}

void MotorDriver::setThrust(float thrust)
{
    thrust = constrain(thrust, -1.0f, 1.0f);

    int duty = (int)(fabs(thrust) * PUMP_PWM_MAX_DUTY);

    if (thrust >= 0.0f)
    {
        ledcWriteChannel(in1_ledc_channel_, duty);
        ledcWriteChannel(in2_ledc_channel_, 0);
    }
    else
    {
        ledcWriteChannel(in1_ledc_channel_, 0);
        ledcWriteChannel(in2_ledc_channel_, duty);
    }
}

void MotorDriver::stop()
{
    ledcWriteChannel(in1_ledc_channel_, 0);
    ledcWriteChannel(in2_ledc_channel_, 0);
}

void MotorDriver::brake()
{
    ledcWriteChannel(in1_ledc_channel_, PUMP_PWM_MAX_DUTY);
    ledcWriteChannel(in2_ledc_channel_, PUMP_PWM_MAX_DUTY);
}