#include "Debug.h"
#include "Config.h"

namespace
{
  MqttLink *g_mqtt = nullptr;
  unsigned long g_logSeq = 0;

  void debugWrite(const char *level, const String &msg)
  {
    g_logSeq++;

    String line;
    line.reserve(msg.length() + 24);
    line += String(g_logSeq);
    line += "|";
    line += level;
    line += "|";
    line += msg;

    // 本地串口始终输出
    Serial.println(line);

    // MQTT 可用时，顺手发到 debug topic
    if (g_mqtt != nullptr && g_mqtt->isMqttConnected())
    {
      g_mqtt->publishRaw(MQTT_TOPIC_DEBUG, line.c_str());
    }
  }
}

void debugBegin(MqttLink *mqtt)
{
  g_mqtt = mqtt;
}

void debugInfo(const String &msg)
{
  debugWrite("INFO", msg);
}

void debugWarn(const String &msg)
{
  debugWrite("WARN", msg);
}

void debugError(const String &msg)
{
  debugWrite("ERROR", msg);
}

void debugMQTT(MqttLink &mqtt, bool &running, unsigned long &counter, unsigned long &lastSendMs)
{
  mqtt.update();

  if (mqtt.hasNewCommand())
  {
    String cmd = mqtt.latestCommand();

    if (cmd == "start")
    {
      running = true;
      Serial.println("cmd=start");
    }
    else if (cmd == "stop")
    {
      running = false;
      Serial.println("cmd=stop");
    }
    else if (cmd == "clear")
    {
      counter = 0;
      Serial.println("cmd=clear");
    }
    else
    {
      Serial.print("unknown cmd: ");
      Serial.println(cmd);
    }

    mqtt.clearCommand();
  }

  unsigned long now = millis();
  if (running && now - lastSendMs >= 1000)
  {
    lastSendMs = now;
    counter++;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", counter);

    if (!mqtt.publishRaw(MQTT_TOPIC_COUNTER, buf))
    {
      Serial.println("publish counter failed");
    }
  }
}

void debugSensor(SensorDriver &sensor)
{
  unsigned long currentMillis = millis();

  // 1. 更新感知
  sensor.update();

  // 2. 读取数据
  float depth = sensor.getDepth();
  float pressure = sensor.getPressure();
  float temp = sensor.getTemp();

  // 3. 拼接日志
  String msg;
  msg.reserve(96);

  msg += "[Time: ";
  msg += String(currentMillis / 1000.0, 2);
  msg += "s] ";

  msg += "Depth: ";
  msg += String(depth, 3);
  msg += " m | ";

  msg += "Pressure: ";
  msg += String(pressure, 1);
  msg += " mbar | ";

  msg += "Temp: ";
  msg += String(temp, 2);
  msg += " C";

  debugInfo(msg);
}

void debugPump(Pump &pump)
{
  static unsigned long lastStepMs = 0;
  static unsigned long lastPrintMs = 0;
  static int step = 0;
  static bool entered = false;

  const unsigned long stepIntervalMs = 5000; // 每阶段持续 5 秒
  const unsigned long printIntervalMs = 500; // 每 0.5 秒打印一次

  unsigned long now = millis();

  if (!entered)
  {
    entered = true;
    lastStepMs = now;
    // 暂时规避死区
    switch (step)
    {
    case 0:
      pump.setCommand(0.84f);
      debugInfo("pump cmd=0.84");
      break;
    case 1:
      pump.setCommand(-0.76f);
      debugInfo("pump cmd=-0.76");
      break;
    case 2:
      pump.setCommand(1.0f);
      debugInfo("pump cmd=1.0");
      break;
    case 3:
      pump.setCommand(-1.0f);
      debugInfo("pump cmd=-1.0");
      break;
    default:
      pump.setCommand(0.0f);
      debugInfo("pump cmd=0.0");
      break;
    }
  }

  pump.update();

  if (now - lastPrintMs >= printIntervalMs)
  {
    lastPrintMs = now;

    String msg;
    msg.reserve(96);
    msg += "pump volume=";
    msg += String(pump.getEstimatedVolumeMl(), 2);
    msg += " ml, upper=";
    msg += String(pump.isAtUpperLimit() ? 1 : 0);
    msg += ", lower=";
    msg += String(pump.isAtLowerLimit() ? 1 : 0);

    debugInfo(msg);
  }

  if (now - lastStepMs >= stepIntervalMs)
  {
    step = (step + 1) % 4;
    entered = false;
  }
}

void debugPumpStartThreshold(MotorDriver &motor)
{
  static const float testCmds[] = {
      0.7f,
      -0.7f,
      0.75f,
      -0.75f,
      0.8f,
      -0.8f,
      0.9f,
      -0.9f,
      1.0f,
      -1.0f,
  };

  static const int kNumCmds = sizeof(testCmds) / sizeof(testCmds[0]);

  static int index = 0;
  static bool entered = false;
  static unsigned long stepStartMs = 0;

  const unsigned long driveMs = 1500; // 每个命令持续 1.5s
  const unsigned long stopMs = 1000;  // 两步之间停 1.0s

  unsigned long now = millis();
  unsigned long phaseMs = driveMs + stopMs;
  unsigned long elapsed = now - stepStartMs;

  if (!entered)
  {
    entered = true;
    stepStartMs = now;
    elapsed = 0;

    String msg = "start threshold test cmd=";
    msg += String(testCmds[index], 2);
    debugInfo(msg);

    motor.setThrust(testCmds[index]);
  }

  // 驱动阶段结束后，进入 stop 阶段
  if (elapsed >= driveMs && elapsed < phaseMs)
  {
    motor.stop();
  }

  // 当前测试项结束，切换到下一项
  if (elapsed >= phaseMs)
  {
    index = (index + 1) % kNumCmds;
    entered = false;
  }
}
