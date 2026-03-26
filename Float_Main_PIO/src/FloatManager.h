#ifndef FLOAT_MANAGER_H
#define FLOAT_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "SensorDriver.h"
#include "MotorDriver.h"
#include "MqttLink.h"
#include "StorageDriver.h"

class FloatManager
{
public:
    enum State
    {
        IDLE,
        DIVE_1,
        HOVER_1,
        ASCEND_1,
        HOVER_SHALLOW_1,
        DIVE_2,
        HOVER_2,
        ASCEND_2,
        HOVER_SHALLOW_2,
        RECOVERY
    };

    FloatManager(SensorDriver *s, MotorDriver *m, MqttLink *mq, StorageDriver *st);

    void init();
    void update();

    State getState() const;
    void setState(State newState);

private:
    SensorDriver *sensor;
    MotorDriver *motor;
    MqttLink *mqtt;
    StorageDriver *storage;

    State currentState;

    unsigned long lastLogTime;
    unsigned long stateStartTime;
    bool idleInitDone;
    bool recoveryUploadDone;

    void handleIdle(unsigned long now, float currentDepth);
    void handleMissionState(unsigned long now, float currentDepth);
    void handleRecovery(unsigned long now, float currentDepth);

    void enterState(State newState);
};

#endif