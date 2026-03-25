#ifndef CONFIG_H
#define CONFIG_H

// 硬件引脚定义
#define PIN_SDA 6
#define PIN_SCL 7
#define PIN_PUMP_IN1 2
#define PIN_PUMP_IN2 3
#define PIN_BATTERY_ADC 4
#define PIN_LED_STATUS 10  //指示灯（可能有）

// 任务参数
#define TARGET_DEPTH_DEEP 2.5f    // 目标深度1: 2.5米
#define TARGET_DEPTH_SHALLOW 0.4f // 目标深度2: 0.4米
#define HOVER_DURATION 30000      // 悬停时间: 30秒 (毫秒)
#define LOG_INTERVAL 1000         // 数据记录间隔: 1秒

// 参数常量
#define FLUID_DENSITY 1025
#define PWM_FREQ 20000     
#define PWM_RES  8         // 8位分辨率 (0-255)

// 身份信息
#define COMPANY_ID "PIONEER_01"   // 你的队伍编号 未定

// 通信参数

// WiFi 参数
#define WIFI_SSID      "esp32_sjtu"
#define WIFI_PASSWORD  "esp32_sjtu"

// MQTT 参数
#define MQTT_HOST           "192.168.137.1"
#define MQTT_PORT           1883
#define MQTT_USER           "admin"
#define MQTT_PASSWORD       "Ciallo~114"
#define MQTT_TOPIC_CMD      "/cmd"
#define MQTT_TOPIC_COUNTER  "/counter" //MQTT_TEST
#define MQTT_TOPIC_DEBUG    "/debug" //DEBUG


#define MQTT_CLIENT_ID     "esp32-counter"

#endif