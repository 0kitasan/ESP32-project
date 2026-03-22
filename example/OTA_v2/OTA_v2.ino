#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* ssid = "FINS";
const char* password = "fins1896";
const char* host = "esp32-ota";

WebServer server(80);

// 保存最近日志
String logBuffer = "";
const size_t LOG_MAX_LEN = 6000;

// 统一日志输出：同时输出到串口和网页缓冲区
void logPrintln(const String& msg) {
  Serial.println(msg);

  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_MAX_LEN) {
    // 超长时截掉最前面的旧内容
    logBuffer.remove(0, logBuffer.length() - LOG_MAX_LEN);
  }
}

const char* mainPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Monitor + OTA</title>
  <style>
    body { font-family: monospace; margin: 20px; }
    #log {
      width: 100%;
      height: 400px;
      border: 1px solid #ccc;
      padding: 10px;
      overflow-y: auto;
      white-space: pre-wrap;
      background: #111;
      color: #0f0;
    }
  </style>
</head>
<body>
  <h2>ESP32 Web Monitor</h2>

  <div id="log">Waiting for logs...</div>

  <h2>OTA Update</h2>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="update">
    <input type="submit" value="Upload">
  </form>

  <script>
    async function refreshLog() {
      try {
        const response = await fetch('/log');
        const text = await response.text();
        const logDiv = document.getElementById('log');
        logDiv.textContent = text;
        logDiv.scrollTop = logDiv.scrollHeight;
      } catch (e) {
        console.log("log fetch failed", e);
      }
    }

    setInterval(refreshLog, 500);
    refreshLog();
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (MDNS.begin(host)) {
    Serial.printf("mDNS ready: http://%s.local/\n", host);
  }

  logPrintln("WiFi connected");
  logPrintln("IP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", mainPage);
  });

  server.on("/log", HTTP_GET, []() {
    server.send(200, "text/plain; charset=utf-8", logBuffer);
  });

  server.on("/update", HTTP_POST,
    []() {
      server.sendHeader("Connection", "close");
      if (Update.hasError()) {
        server.send(500, "text/plain", "Update Failed");
      } else {
        server.send(200, "text/plain", "Update Success. Rebooting...");
      }
      delay(1000);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        logPrintln("OTA Start: " + upload.filename);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          logPrintln("OTA Success, bytes: " + String(upload.totalSize));
        } else {
          Update.printError(Serial);
        }
      }
    }
  );

  server.begin();
  logPrintln("HTTP server started");
}

unsigned long lastLogTime = 0;
int counter = 0;

void loop() {
  server.handleClient();

  // 演示：每秒打一条日志
  if (millis() - lastLogTime > 1000) {
    lastLogTime = millis();
    logPrintln("Counter = " + String(counter++));
  }
}
