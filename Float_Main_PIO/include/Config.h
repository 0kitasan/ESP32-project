#ifndef CONFIG_H
#define CONFIG_H

// 硬件引脚定义
#define PIN_SDA 6
#define PIN_SCL 7
#define PIN_PUMP_IN1 2
#define PIN_PUMP_IN2 3
#define PIN_BATTERY_ADC 4
#define PIN_LED_STATUS 10 // 指示灯（可能有）

// 任务参数
#define TARGET_DEPTH_DEEP 2.5f    // 目标深度1: 2.5米
#define TARGET_DEPTH_SHALLOW 0.4f // 目标深度2: 0.4米
#define HOVER_DURATION 30000      // 悬停时间: 30秒 (毫秒)
#define LOG_INTERVAL 1000         // 数据记录间隔: 1秒

// 参数常量
#define FLUID_DENSITY 1025.0f // 似乎暂时没用，可能要看更底层的库
#define PWM_FREQ 20000
#define PWM_RES 8 // 8位分辨率 (0-255)

#define PUMP_MAX_VOLUME 400.0f // 泵的最大体积估计 (mL)，根据实际情况调整
#define PUMP_START_THRESHOLD 0.75f // 泵命令死区（实测）
#define PUMP_MAX_FLOW                                                          \
  10.0f // 泵的最大体积流量 (10 mL/s=600 mL/min)，根据实际情况调整

// 控制默认参数
#define CTRL_KP_DEFAULT 0.90f
#define CTRL_KD_DEFAULT 0.35f
#define CTRL_OUTPUT_LIMIT_DEFAULT 1.0f
#define CTRL_MIN_ACTUATION_CMD_DEFAULT PUMP_START_THRESHOLD
#define CTRL_HOLD_ENTER_BAND_M_DEFAULT 0.05f
#define CTRL_HOLD_EXIT_BAND_M_DEFAULT 0.12f
#define CTRL_DERIVATIVE_FILTER_ALPHA_DEFAULT 0.35f
#define CTRL_LEAD_ENABLE_DEFAULT 0
#define CTRL_LEAD_GAIN_DEFAULT 1.0f
#define CTRL_LEAD_TAU_S_DEFAULT 0.15f
#define CTRL_LEAD_ALPHA_DEFAULT 0.35f

// 通信参数

// WiFi 参数
// #define WIFI_SSID           "FINS"
// #define WIFI_PASSWORD       "fins1896"
#define WIFI_SSID           "esp32_sjtu"
#define WIFI_PASSWORD       "esp32_sjtu"

// MQTT 参数
// #define MQTT_HOST           "192.168.8.76"
#define MQTT_HOST           "192.168.137.1"
#define MQTT_PORT           1883
#define MQTT_USER           ""//"admin"
#define MQTT_PASSWORD       ""//"Ciallo~114"
#define MQTT_TOPIC_PREFIX   "/float_sjtu"
#define MQTT_TOPIC_CMD_BASE MQTT_TOPIC_PREFIX "/cmd"
#define MQTT_TOPIC_CMD_MISSION MQTT_TOPIC_CMD_BASE "/mission"
#define MQTT_TOPIC_CMD_MOTOR   MQTT_TOPIC_CMD_BASE "/motor"
#define MQTT_TOPIC_CMD_PUMP    MQTT_TOPIC_CMD_BASE "/pump"
#define MQTT_TOPIC_CMD_COUNTER MQTT_TOPIC_CMD_BASE "/counter"
#define MQTT_TOPIC_COUNTER  MQTT_TOPIC_PREFIX "/counter"
#define MQTT_TOPIC_DEBUG    MQTT_TOPIC_PREFIX "/debug"
#define MQTT_TOPIC_HISTORY  MQTT_TOPIC_PREFIX "/history"
#define MQTT_TOPIC_PARAM    MQTT_TOPIC_PREFIX "/param"
#define MQTT_TOPIC_REALTIME MQTT_TOPIC_PREFIX "/telemetry"
#define MQTT_TOPIC_STATUS   MQTT_TOPIC_PREFIX "/status"

#define MQTT_CLIENT_ID      "esp32_float_sjtu_mqtt"

#endif
