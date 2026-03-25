## ESP32

XIAO ESP32C3

### 官方文档/硬件信息

https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

Interfaces：I2C/UART/SPI

PWM/Analog Pins：11/4

- 左侧按钮：BOOT
- 右侧按钮：RESET（重启）

### 基本编程环境配置

添加额外配置（开发板管理器地址）

```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.ison
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
```

开发板管理器中安装 esp32 扩展：

- 有两个版本，不要安装错，要安装 `espressif` 的
- https://github.com/espressif/arduino-esp32

无线配置：如果使用 windows 开启热点，似乎需要配置入站规则

![入站规则](./docs/入站规则.png)

传输数据包格式：队伍编号 时间 压力/深度（如：PN01 1:51:42 UTC 9.8 kpa 1.00 meters ）

### 程序架构与依赖

使用 MQTT 通信，我认为方便很多。

当前代码中使用的库：

- WiFi.h
- PubSubClient.h
- ArduinoOTA.h
- 水压计：https://github.com/bluerobotics/BlueRobotics_MS5837_Library
- BlueRobotics_MS5837_Library
- 其他项目内自定义头文件：
  - Config.h
  - Debug.h（应当在顶层 `main` 中调用，底层直接有线调试，在串口打印）
  - MqttLink.h
  - FloatManager.h（改个名字吧，Mission 之类的）

### 无线烧写配置

- 方案 A：Web OTA —— 浏览器上传 .bin
  - 默认的 Partition Scheme 带有 OTA 分区，不用动
  - 用 USB 第一次烧入“OTA 底包”。
  - 打开串口监视器，记下它打印出来的 IP。
  - 之后可以使用：`Sketch -> Export Compiled Binary`，导出编译好的 `.bin` 文件
- 方案 B：ArduinoOTA —— 更像“无线烧写”，适合后续自动化上传
  - 无线连接后直接使用 Arduino IDE 选择 ip 烧写即可
  - 需要先填写密码
  - 不支持仅上传，总是喜欢编译一遍再上传

### 工具依赖

- Mosquitto
- MQTTX / MQTT Box

```powershell
.\mosquitto -c mosquitto.conf -v

netstat -ano | findstr :1883

arp -a
```

或者 espota 上传（还没试）

```powershell
espota.exe -i 192.168.137.42 -p 3232 -f firmware.bin
espota.exe -i 192.168.137.42 -p 3232 -a your_password -f firmware.bin
```

```powershell
C:\Users\[user]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.7\tools
```

linux 好像只能用 py 吧：

```bash
python3 ~/.arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py \
  -i 192.168.1.100 \
  -p 3232 \
  -f ./build/your_firmware.bin
```
