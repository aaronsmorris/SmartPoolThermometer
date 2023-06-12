# Smart Pool Thermometer

![PoolPhoto](/img/photo_pool.jpeg)

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
* [6x 5V 60mA Epoxy Solar Panel](https://www.amazon.com/dp/B0736W4HK1)
* [18650 Battery Clip Holder](https://www.amazon.com/dp/B0721Y3NDQ)
* [JST Connectors](https://www.amazon.com/dp/B071XN7C43)
* [TP4056 Battery Charger Module](https://www.amazon.com/dp/B098989NRZ)
* [Prototype board (optional)](https://www.amazon.com/dp/B00FXHXT80)
* [M3 3D Printing Brass Nuts, 5mm x 6mm](https://www.amazon.com/dp/B09KZSJS88)
* [M3 6mm Button](https://www.amazon.com/dp/B083HCLFM1)

### Schematic 

![Schematic](/img/schematic.png)
![Board](/img/board.jpg)

### Enclosure 

![Case](/img/case.png)

All parts print without support in the default orientation.

* [Box](stl/box.stl) - White PETG or something UV and heat resistant. 4 walls and top/bottom layers. 10-30% infill should be enough.
* [Lid](stl/lid.stl) - White PETG or something UV and heat resistant. 100% Infill
* [Gasket](stl/gasket.stl) - TPU or something flesible to act as a gasket. 100% Infill
* [AnchorLoop](stl/anchor.stl) - TPU, optional, can be used to tie the thermometer in the sunny side of the pool if needed.

### Build & Assembly

First - drop a brass knurled insert in each of the box bolt holes. The inserts can be driven into the plastic easily with a soldering iron. Make sure they are fairly flush with the surface. Do not lower too much as to compromise the outer wall.
Close the threads with throw-away set of bolts during the next phase of waterproofing.

#### Waterproofing

Below was my process for the several prototypes I made and the final version. It might be overkill, but I have not experienced a single water leak after weeks in the pool.

1. Coat the box with ... epoxy like coating. Heavier on the bottom and sides of the box. Very light on the top and the lid as to not compromise dimensional fit. Wipe any excess around the top and lid if concerned, when this stuff hardens it is very difficult to correct (sanding and headaches). Mask off the inside of the box where the board and battery will go.
2. Insert the thermometer sensor in the box and mask it off with tape about 10mm away from the box. Glue without any gaps and apply some silicone on the top side when the glue is dry. Wait for the silicone to dry/cure.
3. Glue the solar panes, ensure no gaps on the back side. Mask off with tape the effecive area of the solar panel.
4. Spray (rubber coating), thicker on the bottom and around the unmasked part of the sensor, thinner on top and on the lid to ensure the parts fit snuggly but still fit.

#### Circuit board layout 

* Ensure the output of the TP4056 board goingto the ESP8266 is 5v with the little adjuster provided

#### Assembly



Battery and case go on the bottom. The temp sensor in the designate hole pushed all the way down and sealed with appropriate waterproof sealer / adhesive.
The solar panels should be glued and then water proof sealed too.
The linked prototype board fits inside well and can be screwed with some 1mm screws or glued or taped down.



## Data visualization

Useful articles and guides:
* https://frederic-hemberger.de/notes/prometheus/monitoring-temperature-and-humidity-with-mqtt/
* https://grafana.com/docs/grafana/latest/setup-grafana/configure-docker/
* https://grafana.com/docs/grafana-cloud/quickstart/docker-compose-linux/

The following instructions are better suited for a "local" install - desktop, raspberry pi, server on LAN. Deployments expected to communicate over public internet should use appropriate encryption and authentication configuration. Large-scale deployments might also want to leverage a docker-compose, Terraform, etc. 

### MQTT broker

```
docker run -dit \
    --name=mqtt-mac \
    --restart=unless-stopped \
    -p 1883:1883 \
    -v "$APPROPRIATE_VAR_PATH/mosquitto:/mosquitto" \
    eclipse-mosquitto:latest
```

### MQTT JSON exporter

Subscribes to a topic, parses JSON received from that topic and populates Prometheus with the compatible JSON values.
https://github.com/tg44/mqtt-prometheus-message-exporter

```
docker run -dit --restart unless-stopped -p 9324:9000 --name mqtt_json \
    -v "$APPROPRIATE_VAR_PATH/mqtt_json:/data" \
    -e CONF_PATH=/data/exporter.conf \
    ghcr.io/tg44/mqtt-prometheus-message-exporter:latest
```

### MQTT REST API for Alexa

```
docker run -dit --restart unless-stopped -p 3443:3443 \
  -e MQTT_SERVER="$MQTT_SERVER" \
  -e MQTT_TOPIC="$MQTT_TOPIC" \
  -e HTTPS_PRIVATE_KEY_PATH="$PATH_TO_KEY" \
  -e HTTPS_CERTIFICATE_PATH="$PATH_TO_CERT" \
  -e API_KEY="$API_KEY" \
  jaisor/mqtt-rest-api:latest
```

### Prometehus

`prometheus/config.yml`

```
scrape_configs:

# Prometehus itself
  - job_name: 'prometheus'
    scrape_interval: 30s
    static_configs:
      - targets: ['localhost:9090']

# MQTT exporter
  - job_name: 'mqtt'
    scrape_interval: 30s
    static_configs:
    - targets: ['<HOSTNAME>.local:9344']
```

```
docker run -dit --restart unless-stopped -p 9090:9090 \
    --name prometheus-mac \
    -v "$APPROPRIATE_VAR_PATH/prometheus:/data" \
    prom/prometheus:latest \
    --config.file="/data/config.yml" \
    --storage.tsdb.path="/data/prometheus"
```

### Grafana

```
docker run -dit --restart unless-stopped -p "3000:3000" --name grafana-mac grafana/grafana:latest
```

### Dashboard

![Dash](/img/dash.png)
