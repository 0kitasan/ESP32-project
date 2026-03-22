// 测试程序：让 D2 引脚输出持续的高电平 (3.3V)
const int testPin = 4; // XIAO ESP32-C3 的 D2 对应 GPIO 4

void setup() {
  // 必须先将引脚设置为输出模式
  pinMode(testPin, OUTPUT);
}

void loop() {
  digitalWrite(testPin, HIGH);
  delay(1000); // 停1秒
  digitalWrite(testPin, LOW);
  delay(1000); // 停1秒
}
