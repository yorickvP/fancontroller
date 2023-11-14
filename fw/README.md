## Dependencies
This project uses the Espressif [ESP-IDF version 5.1.1 for the ESP32S3](https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32s3/get-started/index.html).

## How to build
```bash
rm ./sdkconfig
export SDKCONFIG_DEFAULTS="./sdkconfig.default"
idf.py clean
rm -rf build
idf.py set-target esp32s3
# set Fancontroller -> Wifi
idf.py menuconfig
idf.py build
```

## TODO
* PID
* CO2 sensor
* Light sensor
* Temperature sensors
* LED
* USB-PD
* Current supervisor
