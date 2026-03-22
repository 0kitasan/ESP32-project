#include <HTTPClient.h>
#include <WiFi.h>

const char *ssid = "your-ssid";         // WiFi SSID
const char *password = "your-password"; // WiFi 密码

const char *serverName = "https://192.168.1.2/sensor/"; // 服务器地址

unsigned long lastTime = 0;      // 上一次发送时间
unsigned long timerDelay = 5000; // 发送间隔

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) { // 等待连接
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 "
                 "seconds before publishing the first reading.");
}

void loop() {
  if ((millis() - lastTime) > timerDelay) { // 达到发送间隔
    // 检查 WiFi 连接状态
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      http.begin(client, serverName); // 连接服务器

      http.addHeader("Content-Type", "application/json"); // 设置请求头
      int httpResponseCode = http.POST(
          "{\"name\":\"sensor\",\"value\":\"123\"}"); // 发送 POST 请求
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // 释放资源
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis(); // 更新上一次发送时间
  }
}
