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
const char* wifi_ssid                  = WIFI_SSID;
const char* wifi_password              = WIFI_PASSWORD;
const char* wifi_hostname              = WIFI_HOSTNAME;
const char* ota_password               = OTA_PASSWORD;

const char* mqtt_server                = MQTT_SERVER;
const int   mqtt_port                  = MQTT_PORT;
const char* mqtt_username              = MQTT_USERNAME;
const char* mqtt_password              = MQTT_PASSWORD;

const char* mqtt_topic_base            = MQTT_TOPIC_BASE;
const char* mqtt_logtopic              = MQTT_LOGTOPIC;
const char* mqtt_set_temperature_topic = MQTT_SET_TEMPERATURE_TOPIC;

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
int slave_id_growatt = 1;
int slave_id_wp = 4;
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

/******************************************************************************************
 Callback function that is called when a message has been pushed to one of your topics.
*******************************************************************************************/
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  for (int i=0; i < length; i++) {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

  if (strcmp(topic, mqtt_set_temperature_topic) == 0)
  {
    String temperature_string(msg);
    float temperature = temperature_string.toFloat();
    uint16_t temp_converted = (uint16_t)(temperature * 100);
    sprintf(log_msg, "set temperature to %.02f (%d)", temperature, temp_converted); log_message(log_msg);

    // instantiate modbusmaster with slave id wp
    node.begin(slave_id_wp, Serial);
    node.writeSingleRegister(54, temp_converted);
  }
}


/******************************************************************
Setup, only ran on application start
*******************************************************************/
void setup() {  
  Serial.begin(9600);

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
  mqtt_client.setCallback(mqtt_callback);
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

  sprintf(mqtt_topic, "%s/%s", mqtt_topic_base, topic);
  
  mqtt_client.publish(mqtt_topic, value_char); 
}

/******************************************************************
Publish int var to specified sub topic
*******************************************************************/
void publishInt(char * topic, int i) {
  String value_str = String(i);
  char value_char[32] = "";
  value_str.toCharArray(value_char, 40);
  
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
    next_poll = millis() + 10000;

    log_message("Trying to read inverter..");
    
    // instantiate modbusmaster with slave id growatt
    node.begin(slave_id_growatt, Serial);
  
    result = node.readInputRegisters(0, 32);
    // do something with data if read is successful
    if (result == node.ku8MBSuccess){
      log_message("success!");
      publishInt("energy/growatt/status", node.getResponseBuffer(0));
      
      publishFloat("energy/growatt/Ppv", glueFloat(node.getResponseBuffer(1), node.getResponseBuffer(2)));    
      publishFloat("energy/growatt/Vpv1", glueFloat(0, node.getResponseBuffer(3)));    
      publishFloat("energy/growatt/PV1Curr", glueFloat(0, node.getResponseBuffer(4)));    
      publishFloat("energy/growatt/Pac", glueFloat(node.getResponseBuffer(11), node.getResponseBuffer(12)));
      publishFloat("energy/growatt/Fac", glueFloat(0, node.getResponseBuffer(13))/10 );  

      publishFloat("energy/growatt/Vac1", glueFloat(0, node.getResponseBuffer(14)));  
      publishFloat("energy/growatt/Iac1", glueFloat(0, node.getResponseBuffer(15)));
      publishFloat("energy/growatt/Pac1", glueFloat(node.getResponseBuffer(16), node.getResponseBuffer(17)));

      publishFloat("energy/growatt/Etoday", glueFloat(node.getResponseBuffer(26), node.getResponseBuffer(27)));
      publishFloat("energy/growatt/Etotal", glueFloat(node.getResponseBuffer(28), node.getResponseBuffer(29)));
      publishFloat("energy/growatt/ttotal", glueFloat(node.getResponseBuffer(30), node.getResponseBuffer(31)));
      publishFloat("energy/growatt/Tinverter", glueFloat(0, node.getResponseBuffer(32)));
    } else {
      tmp = String(result, HEX);
      tmp.toCharArray(value, 40);
      
      log_message("error!");
      log_message(value);
      publishInt("energy/growatt/status", -1);
    }
    node.clearResponseBuffer();
    log_message("Trying to read wp input registers..");
    
    // instantiate modbusmaster with slave id wp
    node.begin(slave_id_wp, Serial);
    
    result = node.readInputRegisters(1, 120);
    if (result == node.ku8MBSuccess) {
      log_message("success!");
      publishInt("heating/mitsubishi/status", 1);

      publishInt("heating/mitsubishi/defrost", node.getResponseBuffer(26)); // 0 = Normal, 1 = Standby, 2 = Defrost, 3 = Waiting Restart
      publishFloat("heating/mitsubishi/zone1_temperature_setpoint", node.getResponseBuffer(40) / 100.0);
      publishFloat("heating/mitsubishi/zone1_flow_temperature_setpoint", node.getResponseBuffer(44) / 100.0);
      
      publishFloat("heating/mitsubishi/legionella_temperature_setpoint", node.getResponseBuffer(48) / 100.0);

      publishFloat("heating/mitsubishi/dhw_temperature_drop", node.getResponseBuffer(50) / 10.0);
    
      publishFloat("heating/mitsubishi/zone1_room_temperature", node.getResponseBuffer(52) / 100.0);

      publishFloat("heating/mitsubishi/refrigerant_temperature", node.getResponseBuffer(56) / 100.0);
      publishFloat("heating/mitsubishi/outdoor_ambient_temperature", node.getResponseBuffer(58) / 10.0);
      
      publishFloat("heating/mitsubishi/water_outlet_temperature", node.getResponseBuffer(60) / 100.0);
      publishFloat("heating/mitsubishi/water_inlet_temperature", node.getResponseBuffer(62) / 100.0);
      
      
    } else {
      tmp = String(result, HEX);
      tmp.toCharArray(value, 40);
      log_message("error!");
      log_message(value);
      publishInt("heating/mitsubishi/status", -1);
    }

    node.clearResponseBuffer();

    log_message("Trying to read wp holding registers..");
    result = node.readHoldingRegisters(4, 120);
    if (result == node.ku8MBSuccess) {
      log_message("success!");
//      publishInt("heating/mitsubishi/operatingmode", node.getResponseBuffer(26)); // 0 = stop, 1 = hot water, 2 = heat, 3 = Cooling, 5 = Freeze Stat, , 6 = Legionella, 7 = Heating-Eco
    
    } else {
      tmp = String(result, HEX);
      tmp.toCharArray(value, 40);
      log_message("error!");
      log_message(value);
    }
    node.clearResponseBuffer();

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
    if (mqtt_client.connect(wifi_hostname, mqtt_username, mqtt_password)) {
      mqtt_client.subscribe(mqtt_set_temperature_topic);
    } else {
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
