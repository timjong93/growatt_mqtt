# Growatt MQTT

_This is still a work in progress 14-09-2019._

This application is build for an ESP8266, tested on NodeMCU.
It reads values from an growatt solar power inverter using modbus. and publishes these values using MQTT for use with for example Home Assistant or any other application of your preference.

## Wiring

You will need to use an RS485 -> TTL adapter, when using and ESP8266 make sure to use an 3.3V compatibale one. If your adapter does not have automatic TX/RX switching you will need additional wiring to pull down/up the correct pins.

ESP8266 --> RS485 to TTL --> Growatt Inverter.

## References
Code structure based on: https://github.com/LukasdeBoer/esp8266-whr930-mqtt