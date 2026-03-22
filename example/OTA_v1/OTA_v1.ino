#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* ssid = "FINS";//你的WiFi名
const char* password = "fins1896";//你的WiFi密码
const char* host = "esp32-ota";

WebServer server(80);

const char* updatePage = R"rawliteral(
<!DOCTYPE html>
<html>
  <body>
    <h2>ESP32 Web OTA</h2>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update">
      <input type="submit" value="Upload">
    </form>
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
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(host)) {
    Serial.printf("mDNS ready: http://%s.local/\n", host);
  } else {
    Serial.println("mDNS failed, use IP instead.");
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", updatePage);
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
        Serial.printf("Update start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("Update success: %u bytes\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    }
  );

  server.begin();
  Serial.println("HTTP OTA server started");
}

void loop() {
  server.handleClient();
}

