#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <ArduinoOTA.h>
#include "secrets.h"
/******************************************************************
Secrets, please change these in the secrets.h file
*******************************************************************/
const char* wifi_ssid     = WIFI_SSID;
const char* wifi_password = WIFI_PASSWORD;
const char* wifi_hostname = WIFI_HOSTNAME;
const char* ota_password  = OTA_PASSWORD;

const char* mqtt_server   = MQTT_SERVER;
const int   mqtt_port     = MQTT_PORT;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const char* mqtt_topic_base = MQTT_TOPIC_BASE;
const char* mqtt_logtopic = MQTT_LOGTOPIC;

/******************************************************************
Useful for debugging, outputs info to a separate mqtt topic
*******************************************************************/
const bool outputMqttLog = true;

char log_msg[256];
char mqtt_topic[256];

/******************************************************************
Instantiate modbus and mqtt libraries
*******************************************************************/
ModbusMaster node;
WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);

/******************************************************************
Log debug to mqtt
*******************************************************************/
void log_message(char* string)
{
  if (outputMqttLog)
  {
    mqtt_client.publish(mqtt_logtopic, string); 
  } 
}

/******************************************************************
Setup, only ran on application start
*******************************************************************/
void setup() {  
  Serial.begin(9600);
  
  // instantiate modbusmaster with slave id 1 (growatt)
  node.begin(1, Serial);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.hostname(wifi_hostname);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(wifi_hostname);

  // Set authentication
  ArduinoOTA.setPassword(ota_password);

  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

  });
  ArduinoOTA.onError([](ota_error_t error) {

  });
  ArduinoOTA.begin();

  mqtt_client.setServer(mqtt_server, mqtt_port);
}

/******************************************************************
Create float using values from multiple regsiters
*******************************************************************/
float glueFloat(unsigned int d1, unsigned int d0) {
  unsigned long t;
  t = d1 << 16;
  t += d0;

  float f;
  f = t;
  f = f / 10;
  return f;
}

/******************************************************************
Publish float var to specified sub topic
*******************************************************************/
void publishFloat(char * topic, float f) {
  String value_str = String(f, 1);
  char value_char[32] = "";
  value_str.toCharArray(value_char, 40);

  String topic_str = "inverter/" + String(topic);
  char topic_char[128] = "";
  topic_str.toCharArray(topic_char, 128);
  sprintf(mqtt_topic, "%s/%s", mqtt_topic_base, topic);
  
  mqtt_client.publish(mqtt_topic, value_char); 
}

/******************************************************************
Read values from modbus registers and output to seperate mqtt topics
*******************************************************************/
unsigned long next_poll = 0;
void update_growatt() {
  static uint32_t i;
  uint8_t j, result;
  uint16_t data[6];
  
  i++;

  String tmp;
  char topic[40] = "";
  char value[40] = "";

  if(millis() > next_poll){
    log_message("Trying to read..");
    
    result = node.readInputRegisters(0, 32);
    // do something with data if read is successful
    if (result == node.ku8MBSuccess){
      log_message("success!");
      publishFloat("status",float(node.getResponseBuffer(0)));
      
      publishFloat("Ppv", glueFloat(node.getResponseBuffer(1), node.getResponseBuffer(2)));    
      publishFloat("Vpv1", glueFloat(0, node.getResponseBuffer(3)));    
      publishFloat("PV1Curr", glueFloat(0, node.getResponseBuffer(4)));    
      publishFloat("Pac", glueFloat(node.getResponseBuffer(11), node.getResponseBuffer(12)));
      publishFloat("Fac", glueFloat(0, node.getResponseBuffer(13))/10 );  

      publishFloat("Vac1", glueFloat(0, node.getResponseBuffer(14)));  
      publishFloat("Iac1", glueFloat(0, node.getResponseBuffer(15)));
      publishFloat("Pac1", glueFloat(node.getResponseBuffer(16), node.getResponseBuffer(17)));

      publishFloat("Etoday", glueFloat(node.getResponseBuffer(26), node.getResponseBuffer(27)));
      publishFloat("Etotal", glueFloat(node.getResponseBuffer(28), node.getResponseBuffer(29)));
      publishFloat("ttotal", glueFloat(node.getResponseBuffer(30), node.getResponseBuffer(31)));
      publishFloat("Tinverter", glueFloat(0, node.getResponseBuffer(32)));
    } else {
      tmp = String(result, HEX);
      tmp.toCharArray(value, 40);
      
      log_message("error!");
      log_message(value);
    }
    next_poll = millis() + 10000;
  }
}

/******************************************************************
Reconnect to mqtt broker if connection is lost
*******************************************************************/
void mqtt_reconnect()
{
  // Loop until we're reconnected
  while (!mqtt_client.connected()) 
  {
    if (!mqtt_client.connect(wifi_hostname, mqtt_username, mqtt_password)) {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/******************************************************************
Application keeps doing this after running setup
*******************************************************************/
void loop() {
  // Handle OTA first.
  ArduinoOTA.handle();

  if (!mqtt_client.connected())
  {
    mqtt_reconnect();
  }
  mqtt_client.loop();

  update_growatt();
}
