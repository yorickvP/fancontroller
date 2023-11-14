idf_component_register(SRCS
    "app_main.c"
    "adc.c"
    "data.c"
    "events.c"
    "fans.c"
    "http_server.c"
    "i2c_bus.c"
    "led.c"
    "mqtt.c"
    "performance.c"
    "periodic.c"
    "tacho.c"
    "temperature.c"
    "wifi.c"

    "driver/shtc3.c"
        INCLUDE_DIRS ".")
