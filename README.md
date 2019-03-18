# CallConnect_ESP32

ESP32 board communciation via AWS IoT MQTT

## Getting Started

This is a quickly evolving example. It combines several common libraries to control LED animations locally, and send the state information to AWS. Topic subscribers will receive the state information and can act as they need.

Edit Config.h.example and certificates.h.example to meet your specific needs and save as Config.h and certificates.h

Others can ignore 

### Hardware

* [Adafruit Feather HUZZAH ESP32](https://www.adafruit.com/product/3405)
* RGB NeoPixels (I'm using an [Adafruit NeoPixel Ring](https://www.adafruit.com/product/1643))
* [Buttons](https://www.adafruit.com/product/1119)
* MicroUSB cable for flashing board

### Libraries

* WiFIManager - As of January 31, 2019 this _must_ be the [development branch](https://github.com/tzapu/WiFiManager/tree/development) in order for ESP32 boards to work
* [WiFiClientSecure](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure)
* [AceButton](https://github.com/bxparks/AceButton)
* [NeoPixelBus](https://github.com/Makuna/NeoPixelBus/wiki)
* [Arduino-mqtt](https://github.com/256dpi/arduino-mqtt)
* [ArduinoJSON](https://arduinojson.org/)

### Helpful Sites

* Check out the [AWS IOT Workshop](https://github.com/aws-samples/aws-iot-workshop)

## Authors

* **John Damask** (standing on the shoulders of giants (i.e. library makers))


## Acknowledgments

* [Adafruit](https://adafruit.com)
* [Shawn Alverson](https://github.com/tablatronix) from Tabletronix for the awesome WiFiManager
* [Brian Park](https://github.com/bxparks) for AceButton
* [AWS IoT](https://aws.amazon.com/iot/?nc=sn&loc=0)
* [These dudes](https://github.com/bblanchon/ArduinoJson/graphs/contributors) for the ArduinoJSON library
* Michael Miller for NeoPixelBus library
