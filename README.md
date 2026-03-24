## ESP32

XIAO ESP32C3

### 官方文档

https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

Interfaces：I2C/UART/SPI

PWM/Analog Pins：11/4

### 编程环境配置

添加额外配置（开发板管理器地址）

```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.ison
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
```

开发板管理器中安装esp32扩展

水压计：https://github.com/bluerobotics/BlueRobotics_MS5837_Library

MissionManager

如果使用windows开启热点，似乎需要配置入站规则

![入站规则](./docs/入站规则.png)

传输数据包格式：队伍编号 时间 压力/深度（如：PN01 1:51:42 UTC 9.8 kpa 1.00 meters ）

### 无线烧写配置

- 方案 A：Web OTA —— 最稳，浏览器上传 .bin
- 方案 B：ArduinoOTA —— 更像“无线烧写”，适合后续自动化上传

使用方案 A，步骤：

- 默认的 Partition Scheme 带有OTA分区，不用动
- 用 USB 第一次烧入“OTA 底包”。
- 打开串口监视器，记下它打印出来的 IP。
- 之后可以使用：`Sketch -> Export Compiled Binary`，导出编译好的 `.bin`文件

方案 B 暂时没试
