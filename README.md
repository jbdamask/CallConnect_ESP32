# CallConnect_ESP32

ESP32 board communciation via AWS IoT MQTT. States show as NeoPixel animations.

## Description

This code accompanies my Instructables.com post called Web-Connected Glow Pillows. 

## Components

### Hardware

* [Adafruit Feather HUZZAH ESP32](https://www.adafruit.com/product/3405)
* RGB NeoPixels
* MicroUSB cable for flashing board
* USB powerbank (I use RavPower 6700mAh)

### Libraries

* WiFIManager - As of January 31, 2019 this _must_ be the [development branch](https://github.com/tzapu/WiFiManager/tree/development) in order for ESP32 boards to work
* [WiFiClientSecure](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure)
* [AceButton](https://github.com/bxparks/AceButton)
* [NeoPixelBus](https://github.com/Makuna/NeoPixelBus/wiki)
* [Arduino-mqtt](https://github.com/256dpi/arduino-mqtt)
* [ArduinoJSON](https://arduinojson.org/)


## Authors

* **John Damask** 


## Acknowledgments

* [Adafruit](https://adafruit.com)
* [Shawn Alverson](https://github.com/tablatronix) from Tabletronix for the awesome WiFiManager
* [Brian Park](https://github.com/bxparks) for AceButton
* [AWS IoT](https://aws.amazon.com/iot/?nc=sn&loc=0)
* [These dudes](https://github.com/bblanchon/ArduinoJson/graphs/contributors) for the ArduinoJSON library
* Michael Miller for NeoPixelBus library
