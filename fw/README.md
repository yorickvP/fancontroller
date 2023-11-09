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
