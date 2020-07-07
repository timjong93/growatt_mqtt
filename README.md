# Growatt MQTT

_This is still a work in progress 14-09-2019._

This application is build for an ESP8266, tested on NodeMCU.
It reads values from an growatt solar power inverter, mitsubishi heat exchanger and whr930 mechanical ventilation using modbus. These value are published using MQTT for use with for example Home Assistant or any other application of your preference.

## Wiring

You will need to use an RS485 -> TTL adapter, when using and ESP8266 make sure to use an 3.3V compatibale one. If your adapter does not have automatic TX/RX switching you will need additional wiring to pull down/up the correct pins.

ESP8266 --> RS485 to TTL --> Growatt Inverter.

## Home Assistant
Use the following sensors to integrate with Home Assistant
``` YAML
- platform: mqtt
  state_topic: "house/energy/growatt/status"
  name: "Solar inverter state"
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Ppv"
  name: "Solar inverter input power"
  unit_of_measurement: W
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Vpv1"
  name: "Solar inverter PV1 voltage"
  unit_of_measurement: V
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/PV1Curr"
  name: "Solar inverter PV1 input current"
  unit_of_measurement: A
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Pac"
  name: "Solar inverter output power"
  unit_of_measurement: W
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Fac"
  name: "Solar inverter grid frequency"
  unit_of_measurement: Hz
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Vac1"
  name: "Solar inverter three/single phase grid voltage"
  unit_of_measurement: v
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Iac1"
  name: "Solar inverter three/single phase grid output current"
  unit_of_measurement: A
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Pac1"
  name: "Solar inverter three/single phase grid output Watt"
  unit_of_measurement: W
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Etoday"
  name: "Solar inverter total energy today"
  unit_of_measurement: KWH
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Etotal"
  name: "Solar inverter total energy"
  unit_of_measurement: KWH
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/ttotal"
  name: "Solar inverter operating timet"
  unit_of_measurement: S
  qos: 0
- platform: mqtt
  state_topic: "house/energy/growatt/Tinverter"
  name: "Solar inverter inverter temperature"
  unit_of_measurement: C
  qos: 0
```

## References
Code structure based on: https://github.com/LukasdeBoer/esp8266-whr930-mqtt
