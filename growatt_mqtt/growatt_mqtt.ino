#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include "secrets.h"
#include "mqtt_topics.h"
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
const char* mqtt_topic_whr_base = MQTT_TOPIC_WHR_BASE;
const char* mqtt_logtopic = MQTT_LOGTOPIC;
const char* mqtt_set_wp_temperature_topic = MQTT_SET_WP_TEMPERATURE_TOPIC;
const char* mqtt_set_ventilation_topic = MQTT_SET_VENTILATION_TOPIC;
const char* mqtt_set_temperature_topic = MQTT_SET_TEMPERATURE_TOPIC;
const char* mqtt_get_update_topic = MQTT_GET_UPDATE_TOPIC;

/******************************************************************
Useful for debugging, outputs info to a separate mqtt topic
*******************************************************************/
const bool outputMqttLog = true;

#define MAXDATASIZE 256
char data[MAXDATASIZE];
int data_length = 0;

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

SoftwareSerial swSer(13, 15, false, 128);
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
  log_message("read mqtt input");
  char msg[length + 1];
  for (int i=0; i < length; i++) {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

  if (strcmp(topic, mqtt_set_wp_temperature_topic) == 0)
  {
    String temperature_string(msg);
    float temperature = temperature_string.toFloat();
    uint16_t temp_converted = (uint16_t)(temperature * 100);
    sprintf(log_msg, "set temperature to %.02f (%d)", temperature, temp_converted); log_message(log_msg);

    // instantiate modbusmaster with slave id wp
    node.begin(slave_id_wp, Serial);
    node.writeSingleRegister(54, temp_converted);
  }
  if (strcmp(topic, mqtt_set_ventilation_topic) == 0)
  {
    String ventilation_string(msg);
    int ventilation = ventilation_string.toInt() + 1;
    int checksum = (0 + 153 + 1 + ventilation + 173) % 256;

    sprintf(log_msg, "set ventilation to %d", ventilation - 1); log_message(log_msg);
    byte command[] = {0x07, 0xF0, 0x00, 0x99, 0x01, ventilation, checksum, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));
  }
  if (strcmp(topic, mqtt_set_temperature_topic) == 0)
  {
    String temperature_string(msg);
    int temperature = temperature_string.toInt();

    temperature = (temperature + 20) * 2;
    int checksum = (0 + 211 + 1 + temperature + 173) % 256;
    
    sprintf(log_msg, "set temperature to %d", temperature); log_message(log_msg);
    byte command[] = {0x07, 0xF0, 0x00, 0xD3, 0x01, temperature, checksum, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));
  }
  
}

/******************************************************************
Setup, only ran on application start
*******************************************************************/
void setup() {  
  Serial.begin(9600);
  swSer.begin(9600);
  
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

  String topic_str = "inverter/" + String(topic);
  char topic_char[128] = "";
  topic_str.toCharArray(topic_char, 128);
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
Reconnect to mqtt broker if connection is lost
*******************************************************************/
void mqtt_reconnect()
{
  // Loop until we're reconnected
  while (!mqtt_client.connected()) 
  {
    if (mqtt_client.connect(wifi_hostname, mqtt_username, mqtt_password)) {
      mqtt_client.subscribe(mqtt_set_wp_temperature_topic);
      mqtt_client.subscribe(mqtt_set_ventilation_topic);
      mqtt_client.subscribe(mqtt_set_temperature_topic);
      mqtt_client.subscribe(mqtt_get_update_topic);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }

  }
}

/******************************************************************
Growatt
*******************************************************************/

void update_growatt() {
  static uint32_t i;
  uint8_t j, result;
  uint16_t data[6];
  
  i++;

  String tmp;
  char topic[40] = "";
  char value[40] = "";
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
}

/******************************************************************
WP
*******************************************************************/
void update_wp() {
  log_message("Trying to read wp input registers..");

  static uint32_t i;
  uint8_t j, result;
  uint16_t data[6];
  
  i++;

  String tmp;
  char topic[40] = "";
  char value[40] = "";
  
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

/******************************************************************
WHR 930
*******************************************************************/
void send_whr_command(byte* command, int length)
{  
  log_message("sending command");
  sprintf(log_msg, "Data length   : %d", length); log_message(log_msg);
  sprintf(log_msg, "Ack           : %02X %02X", command[0], command[1]); log_message(log_msg);
  sprintf(log_msg, "Start         : %02X %02X", command[2], command[3]); log_message(log_msg);
  sprintf(log_msg, "Command       : %02X %02X", command[4], command[5]); log_message(log_msg);
  sprintf(log_msg, "Nr data bytes : %02X (integer %d)", command[6], command[6]); log_message(log_msg);

  int bytesSent = swSer.write(command, length);
  sprintf(log_msg, "sent bytes    : %d", bytesSent); log_message(log_msg);

  // wait until the serial buffer is filled with the replies
  delay(1000);

  // read the serial
  readSerial();
  
  sprintf(log_msg, "received size : %d", data_length); log_message(log_msg);
}

void readSerial()
{

  if (swSer.available() > 0) {
    log_message("serial available");
    
    int index = 0;
    while (swSer.available()){
      byte iByte = swSer.read();
        
      sprintf(log_msg, "%02X", iByte); log_message(log_msg);

      // reset the internal counter to zero when we encounter a start of a message
      if (index > 0 && iByte == 0xF0 && data[index-1] == 0x07){
          log_message("start of msg");
          index = 0;
      } else {
          index += 1;
      }
  
      data[index] = iByte;
      
      if (iByte == 0xF3 && data[index-1] == 0x07)
      {
        log_message("Got an ACK!");
        index = 0;
        data_length = 0;
      }
      if (iByte == 0x0F && data[index-1] == 0x07)
      {
          // call process function
          sprintf(log_msg, "end of msg of length %d", index+1); log_message(log_msg);
          data_length = index + 1;          
      }
    }
  }
}

void get_filter_status() {  
    byte command[] = {0x07, 0xF0, 0x00, 0xD9, 0x00, 0x86, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));

    int filter_state = (int)(data[18]);
    
    char* filter_state_string;
    if (filter_state == 0) {
      filter_state_string = "Ok";
    } else if (filter_state == 1) {
      filter_state_string = "Full";
    } else {
      filter_state_string = "Unknown"; 
    }
    sprintf(log_msg, "received filter state : %d (%s)", filter_state, filter_state_string); log_message(log_msg);
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "filter_state"); mqtt_client.publish(mqtt_topic, filter_state_string);
}

void get_temperatures() {  
    byte command[] = {0x07, 0xF0, 0x00, 0xD1, 0x00, 0x7E, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));
    
    float ComfortTemp = (float)data[6] / 2.0 - 20;
    float OutsideAirTemp = (float)data[7] / 2.0 - 20;
    float SupplyAirTemp = (float)data[8] / 2.0 - 20;
    float ReturnAirTemp = (float)data[9] / 2.0 - 20;
    float ExhaustAirTemp = (float)data[10] / 2.0 - 20;
    
    sprintf(log_msg, "received temperatures (comfort, outside, supply, return, exhaust): %.2f, %.2f, %.2f, %.2f, %.2f", ComfortTemp, OutsideAirTemp, SupplyAirTemp, ReturnAirTemp, ExhaustAirTemp); log_message(log_msg);

    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "comfort_temp"); mqtt_client.publish(mqtt_topic, String(ComfortTemp).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "outside_air_temp"); mqtt_client.publish(mqtt_topic, String(OutsideAirTemp).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "supply_air_temp"); mqtt_client.publish(mqtt_topic, String(SupplyAirTemp).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "return_air_temp"); mqtt_client.publish(mqtt_topic, String(ReturnAirTemp).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "exhaust_air_temp"); mqtt_client.publish(mqtt_topic, String(ExhaustAirTemp).c_str());
}

void get_ventilation_status() {  
    byte command[] = {0x07, 0xF0, 0x00, 0xCD, 0x00, 0x7A, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));

    
    float ReturnAirLevel = (float)data[12];
    float SupplyAirLevel = (float)data[13];
    int FanLevel = (int)data[14] - 1;
    int IntakeFanActive = (int)data[15];
     
    sprintf(log_msg, "received ventilation status (return air level, supply air level, fan level, intake fan active): %.2f, %.2f, %d, %d", ReturnAirLevel, SupplyAirLevel, FanLevel, IntakeFanActive); log_message(log_msg);
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "return_air_level"); mqtt_client.publish(mqtt_topic, String(ReturnAirLevel).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "supply_air_level"); mqtt_client.publish(mqtt_topic, String(SupplyAirLevel).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "ventilation_level"); mqtt_client.publish(mqtt_topic, String(FanLevel).c_str());

    char* IntakeFanActive_string;
    if (IntakeFanActive == 1) {
      IntakeFanActive_string = "Yes";
    } else if (IntakeFanActive == 0) {
      IntakeFanActive_string = "No";
    } else {
      IntakeFanActive_string = "Unknown"; 
    }
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_base, "intake_fan_active");  mqtt_client.publish(mqtt_topic, IntakeFanActive_string);
}

void get_fan_status() {  
    byte command[] = {0x07, 0xF0, 0x00, 0x0B, 0x00, 0xB8, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));

    float IntakeFanSpeed = (int)data[6];
    float ExhaustFanSpeed = (int)data[7];

    sprintf(log_msg, "received fan speeds (intake, exhaust): %.2f, %.2f", IntakeFanSpeed, ExhaustFanSpeed); log_message(log_msg);
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "intake_fan_speed"); mqtt_client.publish(mqtt_topic, String(IntakeFanSpeed).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "exhaust_fan_speed"); mqtt_client.publish(mqtt_topic, String(ExhaustFanSpeed).c_str());
}

void get_valve_status() {  
    byte command[] = {0x07, 0xF0, 0x00, 0x0D, 0x00, 0xBA, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));

    int ByPass = (int)data[7];
    int PreHeating = (int)data[8];
    int ByPassMotorCurrent = (int)data[9];
    int PreHeatingMotorCurrent = (int)data[10];
 
    sprintf(log_msg, "received fan status (bypass, preheating, bypassmotorcurrent, preheatingmotorcurrent): %d, %d, %d, %d", ByPass, PreHeating, ByPassMotorCurrent, PreHeatingMotorCurrent); log_message(log_msg);
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "valve_bypass_percentage"); mqtt_client.publish(mqtt_topic, String(ByPass).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "valve_preheating"); mqtt_client.publish(mqtt_topic, String(PreHeating).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "bypass_motor_current"); mqtt_client.publish(mqtt_topic, String(ByPassMotorCurrent).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "preheating_motor_current"); mqtt_client.publish(mqtt_topic, String(PreHeatingMotorCurrent).c_str());
}

void get_bypass_control() {
    byte command[] = {0x07, 0xF0, 0x00, 0xDF, 0x00, 0x8C, 0x07, 0x0F};
    send_whr_command(command, sizeof(command));

    int ByPassFactor  = (int)data[9];
    int ByPassStep  = (int)data[10];
    int ByPassCorrection  = (int)data[11];

    char* summerModeString;
    if ((int)data[13] == 1) {
      summerModeString = "Yes";
    } else {
      summerModeString = "No";
    }
    sprintf(log_msg, "received bypass control (bypassfactor, bypass step, bypass correction, summer mode): %d, %d, %d, %s", ByPassFactor, ByPassStep, ByPassCorrection, summerModeString); log_message(log_msg);
    
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "bypass_factor"); mqtt_client.publish(mqtt_topic, String(ByPassFactor).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "bypass_step"); mqtt_client.publish(mqtt_topic, String(ByPassStep).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "bypass_correction"); mqtt_client.publish(mqtt_topic, String(ByPassCorrection).c_str());
    sprintf(mqtt_topic, "%s/%s", mqtt_topic_whr_base, "summermode"); mqtt_client.publish(mqtt_topic, String(summerModeString).c_str());
}


/******************************************************************
Application keeps doing this after running setup
*******************************************************************/

typedef void (* GenericFP)();
GenericFP fnArr[8] = {&update_growatt, &update_wp, &get_filter_status, &get_temperatures, &get_fan_status, &get_valve_status, &get_bypass_control, &get_ventilation_status};
unsigned long next_poll = 0;

void loop() {
  // Handle OTA first.
  ArduinoOTA.handle();

  if (!mqtt_client.connected())
  {
    mqtt_reconnect();
  }
  mqtt_client.loop();
  if(millis() > next_poll){
    for (int i = 0; i < 8; i++)
    {
      fnArr[i]();
      mqtt_client.loop();
    }
    next_poll = millis() + 10000;
  }else{
    mqtt_client.loop();
  }

}
