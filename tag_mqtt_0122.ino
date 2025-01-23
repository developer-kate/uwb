/*
For ESP32S3 UWB AT Demo with MQTT

Libraries Required:
Use 2.0.0   Wire
Use 1.11.7   Adafruit_GFX_Library
Use 1.14.4   Adafruit_BusIO
Use 2.0.0   SPI
Use 2.5.7   Adafruit_SSD1306
Use PubSubClient for MQTT
Use ArduinoJson
*/

// User config          ------------------------------------------
#define UWB_INDEX 0
#define TAG
#define UWB_TAG_COUNT 64

// WiFi Settings
const char* ssid = "JNEWORKS(ORBI10)";
const char* password = "jneworks8661";

// MQTT Broker Settings (Raspberry Pi)
const char* mqtt_server = "192.168.1.83";
const int mqtt_port = 1883;

// User config end       ------------------------------------------

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define SERIAL_LOG Serial
#define SERIAL_AT mySerial2

HardwareSerial SERIAL_AT(2);

#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    delay(10);
    SERIAL_LOG.println();
    SERIAL_LOG.print("Connecting to ");
    SERIAL_LOG.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        SERIAL_LOG.print(".");
    }

    SERIAL_LOG.println("");
    SERIAL_LOG.println("WiFi connected");
    SERIAL_LOG.println("IP address: ");
    SERIAL_LOG.println(WiFi.localIP());
}

void reconnect() {
    while (!client.connected()) {
        SERIAL_LOG.print("Attempting MQTT connection...");
        String clientId = "ESP32Tag-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            SERIAL_LOG.println("connected");
        } else {
            SERIAL_LOG.print("failed, rc=");
            SERIAL_LOG.print(client.state());
            SERIAL_LOG.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);

    SERIAL_LOG.begin(115200);
    SERIAL_LOG.print(F("Hello! ESP32-S3 AT command V1.0 Test"));
    
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);

    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
    SERIAL_AT.println("AT");
    
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(1000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        SERIAL_LOG.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();

    logoshow();

    sendData("AT?", 2000, 1);
    sendData("AT+RESTORE", 5000, 1);
    sendData(config_cmd(), 2000, 1);
    sendData(cap_cmd(), 2000, 1);
    sendData("AT+SETRPT=1", 2000, 1);
    sendData("AT+SAVE", 2000, 1);
    sendData("AT+RESTART", 2000, 1);
}

String response = "";
String rec_head = "AT+RANGE";

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    while (SERIAL_LOG.available() > 0) {
        SERIAL_AT.write(SERIAL_LOG.read());
        yield();
    }

    while (SERIAL_AT.available() > 0) {
        char c = SERIAL_AT.read();

        if (c == '\r') {
            continue;
        }
        else if (c == '\n' || c == '\r') {
            // Create JSON document
            StaticJsonDocument<200> doc;
            doc["device_type"] = "tag";
            doc["tag_id"] = UWB_INDEX;
            doc["data"] = response;
            
            char jsonBuffer[512];
            serializeJson(doc, jsonBuffer);
            
            // Publish to MQTT
            client.publish("uwb/tag/data", jsonBuffer);
            
            SERIAL_LOG.println(response);
            response = "";
        }
        else {
            response += c;
        }
    }
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
    temp = temp + "T" + UWB_INDEX;
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
    temp = temp + ",0";       // Set device role (0 for tag)
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