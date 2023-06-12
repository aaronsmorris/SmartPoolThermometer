# Smart Pool Thermometer - ESPHome Version

![PoolPhoto](/img/pool_photo.jpeg)

## Overview
This is a version of the Smart Pool Thermometer by jaisor: https://github.com/jaisor/SmartPoolThermometer that uses ESPHome for the code instead of stand-alone arduino code. This makes it easy to setup and maintain in Home Assistant.

## Requirements

* Floating water-tight pool sensor
* Integrates with Home Assistant via ESPHome
* Temperature and other telemetry (internal battery voltage) readings submitted over Wifi to ESPHome every 15min)
* Low power consumption, battery powered with solar recharge during the day

## Notes

Code is compatible with both ESP8266 and ESP32 boards, but ESP8266 draws significantly less power (4mA) during deep sleep. For my project, I used a Wemos D1 Mini ESP8266 clone.



### Bill of materials (BOM)

* [ESP8266 ESP-12](https://www.amazon.com/gp/product/B081PX9YFV)
* [DS18B20 Temperature Sensor Waterproof](https://www.amazon.com/dp/B012C597T0)
* [JST 3 Pin Connector](https://www.amazon.com/dp/B01DUC1PW6)
* [2x 10K Resistors; 1x 1K Resistor](https://www.amazon.com/dp/B08FD1XVL6)
* [5V 60mA Epoxy Solar Panel](https://www.amazon.com/dp/B0736W4HK1) (You only need 2)
* [18650 Battery Clip Holder](https://www.amazon.com/dp/B0721Y3NDQ)
* [JST Connectors](https://www.amazon.com/dp/B071XN7C43)
* [TP4056 Battery Charger Module](https://www.amazon.com/dp/B098989NRZ)
* [Prototype board (optional)](https://www.amazon.com/dp/B00FXHXT80)
* [M3 3D Printing Brass Nuts, 5mm x 6mm](https://www.amazon.com/dp/B09KZSJS88)
* [M3 6mm Button screws](https://www.amazon.com/dp/B083HCLFM1)

### Schematic 
Below is the schematic from @jaisor version. I used the same design, except omitted the pushbutton for reset and the 1K resistor for the reset line.

![Schematic](/img/schematic.png)

### Enclosure 

![Case](/img/case.png)

All parts print without support in the default orientation.

* [Box](stl/box.stl) - White PETG or something UV and heat resistant. 4 walls and top/bottom layers. 10-30% infill should be enough.
* [Lid](stl/lid-no-button.stl) - Lid with no hole for reset button. White PETG or something UV and heat resistant. 100% Infill
* [Lid](stl/lid.stl) - Alternative Lid with hole for reset button White PETG or something UV and heat resistant. 100% Infill
* [Gasket](stl/gasket.stl) - TPU or something flesible to act as a gasket. 100% Infill
* [AnchorLoop](stl/anchor.stl) - TPU, optional, can be used to tie the thermometer in the sunny side of the pool if needed.

### Build & Assembly

First - drop a brass knurled insert in each of the box bolt holes. The inserts can be driven into the plastic easily with a soldering iron. Make sure they are fairly flush with the surface. Do not lower too much as to compromise the outer wall.
Close the threads with throw-away set of bolts during the next phase of waterproofing.

#### Waterproofing

For waterproofing the box, I didn't go as extreme as jaisor did, but this is what I did. If it starts to leak then I'll consider flexseal as well.

1. Coat the box with epoxy resin. Heavier on the bottom and sides of the box. Wipe any excess around the top and lid if concerned, when this stuff hardens it is very difficult to correct (sanding and headaches).
2. Insert the thermometer sensor in the box, let it stick out about a 1/2 inch. Epoxy the probe in place from the top and bottom, working the Epoxy into the hole. 
3. Epoxy the solar panels, ensure no gaps on the back side. Mask off the top of the solar panels with tape.
4. Coat the lid with epoxy resin.

#### Circuit board layout 

* Ensure the output of the TP4056 board going to the ESP8266 is 5v by adjusting the variable potentiometer on the board using a voltmeter measuring the Vout terminals.

#### Assembly


Battery and case go on the bottom. The temp sensor in the designate hole pushed all the way down and sealed with appropriate waterproof sealer / adhesive.
The solar panels should be glued and then water proof sealed too.
The linked prototype board fits inside well and can be screwed with some 1mm screws or glued or taped down.


## ESPHome YAML File

Here's an example of how I set up my ESPHome YAML for this sensor. It's a Dallas 18B20 temp sensor, and I use deep sleep to conserve power. Every 15m I report the temp and battery voltage. I use a deep sleep toggle entity on Home Assistant to tell the device when not to use deep sleep so that I can do OTA updates.

![ESPHome YAML](/esphome-pooltemp.yaml)

## Create a Deep Sleep toggle in Home Assistant to enable/disable deep sleep on ESPHome Devices
Follow the instructions on this blog to make a deep sleep toggle so you can turn deep sleepn on and off of your devices for testing or code updates.

https://www.randomdiyprojects.it/esphome-and-deep-sleep/

