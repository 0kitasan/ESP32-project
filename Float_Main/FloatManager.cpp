#include "FloatManager.h"

FloatManager::FloatManager(SensorDriver* s, MotorDriver* m, MqttLink* mq, StorageDriver* st)
    : sensor(s),
      motor(m),
      mqtt(mq),
      storage(st),
      currentState(IDLE),
      lastLogTime(0),
      stateStartTime(0),
      idleInitDone(false),
      recoveryUploadDone(false) {}

void FloatManager::init() {
    currentState = IDLE;
    lastLogTime = 0;
    stateStartTime = millis();
    idleInitDone = false;
    recoveryUploadDone = false;

    Serial.println("Manager: Init Complete. State = IDLE");
}

void FloatManager::update() {
    unsigned long now = millis();
    float currentDepth = sensor->getDepth();

    switch (currentState) {
        case IDLE:
            handleIdle(now, currentDepth);
            break;

        case DIVE_1:
        case HOVER_1:
        case ASCEND_1:
        case HOVER_SHALLOW_1:
        case DIVE_2:
        case HOVER_2:
        case ASCEND_2:
        case HOVER_SHALLOW_2:
            handleMissionState(now, currentDepth);
            break;

        case RECOVERY:
            handleRecovery(now, currentDepth);
            break;
    }
}

FloatManager::State FloatManager::getState() const {
    return currentState;
}

void FloatManager::setState(State newState) {
    enterState(newState);
}

void FloatManager::enterState(State newState) {
    currentState = newState;
    stateStartTime = millis();
    lastLogTime = 0;

    Serial.print("Manager: Enter state = ");
    Serial.println((int)currentState);

    if (newState == RECOVERY) {
        recoveryUploadDone = false;
    }
}

void FloatManager::handleIdle(unsigned long now, float currentDepth) {
    if (!idleInitDone) {
        idleInitDone = true;
        Serial.println("Manager: IDLE init");
        // 如果你希望每次上电先清空旧日志，可以打开这句
        // storage->clearLogs();
    }

    // 每5秒发一次心跳/实时数据
    if (now - lastLogTime >= 5000) {
        lastLogTime = now;

        if (mqtt->isMqttConnected()) {
            // 这里建议你后面在 MqttLink 里补一个更通用的 publishDepth/publishStatus
            // 目前先沿用你之前的语义
            mqtt->sendRealtimeData(currentDepth);
        }

        Serial.print("Manager: IDLE heartbeat depth = ");
        Serial.println(currentDepth);
    }

    // 后续可在这里放“收到命令后开始任务”的逻辑
    // 比如：
    // if (mqtt->hasNewCommand()) {
    //     String cmd = mqtt->latestCommand();
    //     if (cmd == "start_mission") {
    //         storage->clearLogs();
    //         enterState(DIVE_1);
    //     }
    //     mqtt->clearCommand();
    // }
}

void FloatManager::handleMissionState(unsigned long now, float currentDepth) {
    // 每1秒记录一次日志
    if (now - lastLogTime >= 1000) {
        lastLogTime = now;

        storage->logData(now, currentDepth);

        Serial.print("Manager: log depth = ");
        Serial.println(currentDepth);
    }

    // 这里不放 PID，只预留控制接口
    // 例如未来你可以写：
    // float thrust = depthController->update(targetDepth, currentDepth);
    // motor->setThrust(thrust);

    // 当前先只保留状态逻辑框架
    // 后续你根据任务条件切状态，例如：
    //
    // if (currentState == DIVE_1 && currentDepth >= TARGET_DEPTH_DEEP) {
    //     enterState(HOVER_1);
    // }
    //
    // if (currentState == HOVER_1 && now - stateStartTime >= HOVER_DURATION) {
    //     enterState(ASCEND_1);
    // }
    //
    // if (任务全部完成) {
    //     enterState(RECOVERY);
    // }
}

void FloatManager::handleRecovery(unsigned long now, float currentDepth) {
    motor->stop();

    // 这里也可以顺便发实时状态
    if (mqtt->isMqttConnected() && now - lastLogTime >= 5000) {
        lastLogTime = now;
        mqtt->sendRealtimeData(currentDepth);
    }

    // 历史数据上传一次即可，避免无限重发
    if (recoveryUploadDone) {
        return;
    }

    if (!mqtt->isMqttConnected()) {
        return;
    }

    Serial.println("Manager: Uploading history data...");

    File file = storage->openFileForRead();
    if (!file) {
        Serial.println("Manager: No logs found!");
        recoveryUploadDone = true;
        return;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() == 0) {
            continue;
        }

        mqtt->sendHistoryLine(line);
        delay(50);
    }

    file.close();

    Serial.println("Manager: Upload complete!");
    recoveryUploadDone = true;
}