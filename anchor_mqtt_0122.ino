/*
For ESP32S3 UWB AT Demo with WiFi and MQTT

Libraries Required:
- Wire
- Adafruit_GFX_Library
- Adafruit_BusIO
- SPI
- Adafruit_SSD1306
- WiFi
- PubSubClient
- ArduinoJson
*/

// User config          ------------------------------------------
#define UWB_INDEX 4
#define ANCHOR
#define UWB_TAG_COUNT 64
#define MQTT_MAX_PACKET_SIZE 512
// WiFi 설정
const char* ssid = "JNEWORKS(ORBI10)";
const char* password = "jneworks8661";     // WiFi 비밀번호로 변경하세요


// MQTT Broker Settings (Raspberry Pi)
const char* mqtt_server = "test.mosquitto.org";
//const char* mqtt_server = "192.168.1.100"; // 라즈베리파이 192.168.1.x 주소
const int mqtt_port = 1883;
/*
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
*/
// User config end       ------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi 클라이언트 객체 생성
WiFiClient wifiClient;
PubSubClient client(wifiClient);
// PubSubClient.h 파일을 수정하거나 코드 상단에 추가
#define MQTT_MAX_PACKET_SIZE 512
#define SERIAL_LOG Serial
#define SERIAL_AT Serial2

// ESP32S3
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void callback(char* topic, byte* payload, unsigned int length) {
  SERIAL_LOG.print("메시지 수신 [");
  SERIAL_LOG.print(topic);
  SERIAL_LOG.print("]: ");
  for (int i = 0; i < length; i++) {
    SERIAL_LOG.print((char)payload[i]);
  }
  SERIAL_LOG.println();
}

void setup_wifi() {
  delay(10);
  SERIAL_LOG.println();
  SERIAL_LOG.print("Connecting to ");
  SERIAL_LOG.println(ssid);

  // DHCP 사용
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    SERIAL_LOG.print(".");
    
    if (attempts % 10 == 0) {
      SERIAL_LOG.println();
      SERIAL_LOG.print("Still trying to connect, status: ");
      SERIAL_LOG.println(WiFi.status());
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    SERIAL_LOG.println("");
    SERIAL_LOG.println("WiFi connected");
    SERIAL_LOG.print("IP address: ");
    SERIAL_LOG.println(WiFi.localIP());
    SERIAL_LOG.print("MAC address: ");
    SERIAL_LOG.println(WiFi.macAddress());
    SERIAL_LOG.print("Gateway IP: ");
    SERIAL_LOG.println(WiFi.gatewayIP());
    SERIAL_LOG.print("Subnet mask: ");
    SERIAL_LOG.println(WiFi.subnetMask());
  } else {
    SERIAL_LOG.println("");
    SERIAL_LOG.println("WiFi connection failed");
  }
}

void testMQTTConnection() {
  SERIAL_LOG.println("MQTT 연결 테스트 시작...");
  
  // TCP 연결 테스트
  SERIAL_LOG.print("TCP 연결 테스트 (IP: ");
  SERIAL_LOG.print(mqtt_server);
  SERIAL_LOG.print(", 포트: ");
  SERIAL_LOG.print(mqtt_port);
  SERIAL_LOG.print(")... ");
  
  WiFiClient testClient;
  if (testClient.connect(mqtt_server, mqtt_port)) {
    SERIAL_LOG.println("성공!");
    testClient.stop();
    
    // MQTT 연결 테스트
    SERIAL_LOG.print("MQTT 프로토콜 연결 테스트... ");
    String clientId = "TestClient-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      SERIAL_LOG.println("성공!");
      
      // 테스트 메시지 발행
      SERIAL_LOG.print("테스트 메시지 발행... ");
      if (client.publish("test/topic", "Hello from ESP32 Anchor")) {
        SERIAL_LOG.println("성공!");
      } else {
        SERIAL_LOG.println("실패!");
      }
      
      client.disconnect();
    } else {
      SERIAL_LOG.print("실패! 오류 코드: ");
      SERIAL_LOG.println(client.state());
    }
  } else {
    SERIAL_LOG.println("실패!");
  }
}

bool reconnect() {
  SERIAL_LOG.println("MQTT 재연결 시도 시작...");
  int attempts = 0;
  int max_attempts = 5;
  
  while (!client.connected() && attempts < max_attempts) {
    attempts++;
    SERIAL_LOG.print("MQTT 연결 시도 #");
    SERIAL_LOG.print(attempts);
    SERIAL_LOG.print(", 서버: ");
    SERIAL_LOG.print(mqtt_server);
    SERIAL_LOG.print(", 포트: ");
    SERIAL_LOG.println(mqtt_port);
    
    String clientId = "ESP32Anchor-";
    clientId += String(UWB_INDEX) + "-" + String(random(0xffff), HEX);
    SERIAL_LOG.print("클라이언트 ID: ");
    SERIAL_LOG.println(clientId);
    
    // WiFi 연결 상태 확인
    SERIAL_LOG.print("WiFi 상태: ");
    SERIAL_LOG.println(WiFi.status() == WL_CONNECTED ? "연결됨" : "연결 안됨");
    
    if (client.connect(clientId.c_str())) {
      SERIAL_LOG.println("MQTT 브로커에 연결 성공!");
      return true;
    } else {
      SERIAL_LOG.print("연결 실패, rc=");
      int state = client.state();
      SERIAL_LOG.print(state);
      
      // MQTT 오류 코드 해석
      switch(state) {
        case -4: SERIAL_LOG.println(" (MQTT_CONNECTION_TIMEOUT)"); break;
        case -3: SERIAL_LOG.println(" (MQTT_CONNECTION_LOST)"); break;
        case -2: SERIAL_LOG.println(" (MQTT_CONNECT_FAILED)"); break;
        case -1: SERIAL_LOG.println(" (MQTT_DISCONNECTED)"); break;
        case 1: SERIAL_LOG.println(" (MQTT_CONNECT_BAD_PROTOCOL)"); break;
        case 2: SERIAL_LOG.println(" (MQTT_CONNECT_BAD_CLIENT_ID)"); break;
        case 3: SERIAL_LOG.println(" (MQTT_CONNECT_UNAVAILABLE)"); break;
        case 4: SERIAL_LOG.println(" (MQTT_CONNECT_BAD_CREDENTIALS)"); break;
        case 5: SERIAL_LOG.println(" (MQTT_CONNECT_UNAUTHORIZED)"); break;
        default: SERIAL_LOG.println(" (알 수 없는 오류)");
      }
      
      // TCP 연결 직접 테스트
      SERIAL_LOG.print("TCP 연결 직접 테스트... ");
      WiFiClient testClient;
      if (testClient.connect(mqtt_server, mqtt_port)) {
        SERIAL_LOG.println("성공! MQTT 프로토콜 문제일 수 있습니다.");
        testClient.stop();
      } else {
        SERIAL_LOG.println("실패! 네트워크 연결 문제일 수 있습니다.");
      }
      
      SERIAL_LOG.println(" 5초 후 재시도...");
      delay(5000);
    }
  }
  
  SERIAL_LOG.println("MQTT 재연결 시도 완료, 결과: 실패");
  return false;
}

void setup() {
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);

  SERIAL_LOG.begin(115200);
  SERIAL_LOG.println(F("Hello! ESP32-S3 AT command V1.0 Test with WiFi"));
  
  // WiFi 초기화 - 꼭 호출되는지 확인
  setup_wifi();
  
  // MQTT 클라이언트 설정
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(60);
  client.setSocketTimeout(15); // 소켓 타임아웃 설정

  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  SERIAL_AT.println("AT");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(1000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    SERIAL_LOG.println(F("SSD1306 allocation failed"));
    // 계속 진행
  } else {
    display.clearDisplay();
    logoshow();
  }

  sendData("AT?", 2000, 1);
  sendData("AT+RESTORE", 5000, 1);
  sendData(config_cmd(), 2000, 1);
  sendData(cap_cmd(), 2000, 1);
  sendData("AT+SETRPT=1", 2000, 1);
  sendData("AT+SAVE", 2000, 1);
  sendData("AT+RESTART", 2000, 1);
  
  SERIAL_LOG.println("Setup completed");
  
  // MQTT 연결 테스트
  testMQTTConnection();
}

String response = "";
String rec_head = "AT+RANGE";
unsigned long lastWifiCheck = 0;

void loop() {
  // 현재 시간 확인
  unsigned long currentMillis = millis();
  
  // WiFi 연결 상태 주기적 확인 (10초마다)
  if (currentMillis - lastWifiCheck > 10000) {
    lastWifiCheck = currentMillis;
    
    // WiFi 상태 확인
    if (WiFi.status() != WL_CONNECTED) {
      SERIAL_LOG.println("WiFi connection lost, reconnecting...");
      setup_wifi();
    }
  }
  
  // WiFi 연결되지 않았으면 리턴
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    return;
  }
  
  // MQTT 연결 확인
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 시리얼 통신 처리
  while (SERIAL_LOG.available() > 0) {
    SERIAL_AT.write(SERIAL_LOG.read());
    yield();
  }

  // AT 명령/응답 처리
  while (SERIAL_AT.available() > 0) {
    char c = SERIAL_AT.read();

    if (c == '\r') {
      continue;
    }
    else if (c == '\n') {
      if (response.length() > 0) {
        SERIAL_LOG.println(response);
        
        if (response.startsWith("AT+RANGE=")) {
          // Parse the UWB data
          StaticJsonDocument<512> doc;
          
          // Extract tag ID
          int tidStart = response.indexOf("tid:") + 4;
          int tidEnd = response.indexOf(",", tidStart);
          String tagId = response.substring(tidStart, tidEnd);
          
          // Extract mask
          int maskStart = response.indexOf("mask:") + 5;
          int maskEnd = response.indexOf(",", maskStart);
          String mask = response.substring(maskStart, maskEnd);
          
          // Extract sequence
          int seqStart = response.indexOf("seq:") + 4;
          int seqEnd = response.indexOf(",", seqStart);
          String seq = response.substring(seqStart, seqEnd);
          
          // Extract range values
          int rangeStart = response.indexOf("range:(") + 7;
          int rangeEnd = response.indexOf(")", rangeStart);
          String rangeStr = response.substring(rangeStart, rangeEnd);
          
          // Extract RSSI values
          int rssiStart = response.indexOf("rssi:(") + 6;
          int rssiEnd = response.indexOf(")", rssiStart);
          String rssiStr = response.substring(rssiStart, rssiEnd);

          // Create JSON structure
          doc["device_type"] = "anchor";
          doc["anchor_id"] = UWB_INDEX;
          doc["tag_id"] = tagId.toInt();
          doc["mask"] = mask;
          doc["sequence"] = seq.toInt();
          
          // 배열을 객체로 변경
          JsonObject rangeObj = doc.createNestedObject("range");
          int rStart = 0;
          int rIndex = 0;
          
          // 거리 값을 파싱하고 앵커 ID를 키로 사용하여 객체에 저장
          while (rStart < rangeStr.length()) {
            int rEnd = rangeStr.indexOf(",", rStart);
            if (rEnd == -1) rEnd = rangeStr.length();
            String rVal = rangeStr.substring(rStart, rEnd);
            
            // 현재 앵커의 데이터만 저장
            if (rIndex == UWB_INDEX || rVal.toInt() > 0) {
              rangeObj[String(UWB_INDEX)] = rVal.toInt();
            }
            
            rStart = rEnd + 1;
            rIndex++;
          }
          
          // RSSI 값을 파싱하고 앵커 ID를 키로 사용하여 객체에 저장
          JsonObject rssiObj = doc.createNestedObject("rssi");
          int rsStart = 0;
          int rsIndex = 0;
          
          while (rsStart < rssiStr.length()) {
            int rsEnd = rssiStr.indexOf(",", rsStart);
            if (rsEnd == -1) rsEnd = rssiStr.length();
            String rsVal = rssiStr.substring(rsStart, rsEnd);
            
            // 현재 앵커의 데이터만 저장
            if (rsIndex == UWB_INDEX || rsVal.toFloat() < 0) {
              rssiObj[String(UWB_INDEX)] = rsVal.toFloat();
            }
            
            rsStart = rsEnd + 1;
            rsIndex++;
          }
          
          // Add timestamp
          doc["timestamp"] = millis();

          // Serialize and publish
          char jsonBuffer[512];
          serializeJson(doc, jsonBuffer);
          
          // 디버깅용 출력
          SERIAL_LOG.print("Publishing: ");
          SERIAL_LOG.println(jsonBuffer);
          
          bool success = client.publish("uwb/anchor/data", jsonBuffer);
          if (success) {
            SERIAL_LOG.println("Publish successful");
          } else {
            SERIAL_LOG.print("Publish failed, reason: ");
            SERIAL_LOG.println(client.state());
          }
        }
      }
      response = "";
    }
    else {
      response += c;
    }
  }
  
  // CPU 과부하 방지 딜레이
  delay(10);
}

void logoshow(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("MaUWB DW3000"));

  display.setCursor(0, 20);
  display.setTextSize(2);

  String temp = "";
  temp = temp + "A" + UWB_INDEX;
  temp = temp + "   6.8M";
  display.println(temp);

  display.setCursor(0, 40);
  temp = "Total: ";
  temp = temp + UWB_TAG_COUNT;
  display.println(temp);

  display.display();
  delay(2000);
}

String sendData(String command, const int timeout, boolean debug) {
  String response = "";
  SERIAL_LOG.println(command);
  SERIAL_AT.println(command);

  long int time = millis();
  while ((time + timeout) > millis()) {
    while (SERIAL_AT.available()) {
      char c = SERIAL_AT.read();
      response += c;
    }
  }

  if (debug) {
    SERIAL_LOG.println(response);
  }
  return response;
}

String config_cmd() {
  String temp = "AT+SETCFG=";
  temp = temp + UWB_INDEX;  // Set device id
  temp = temp + ",1";       // Set device role (1 for anchor)
  temp = temp + ",1";       // Set frequency 6.8M
  temp = temp + ",1";       // Set range filter
  return temp;
}

String cap_cmd() {
  String temp = "AT+SETCAP=";
  temp = temp + UWB_TAG_COUNT;     // Set Tag capacity
  temp = temp + ",10";             // Time of a single time slot
  temp = temp + ",0";
  return temp;
}