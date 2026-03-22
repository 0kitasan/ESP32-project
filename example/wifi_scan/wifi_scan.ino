#include "WiFi.h"

void setup() {
  Serial.begin(115200);

  // 将WiFi设置为站点模式并断开之前连接的AP
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Setup done");
}

void loop() {
  Serial.println("scan start");

  // WiFi.scanNetworks将返回找到的网络数量
  // 注意：这是一个阻塞函数，扫描期间程序会停在这里等待结果
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // 打印找到的每个网络的SSID和RSSI
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i)); // 打印 Wi-Fi 名称
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i)); // 打印信号强度（单位dBm，负数，越接近0信号越好）
      Serial.print(")");
      // 如果是开放网络打印空格，加密网络打印星号
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");

  // 等待一段时间后再进行扫描（5秒）
  delay(5000);
}

