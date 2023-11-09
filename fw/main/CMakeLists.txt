idf_component_register(SRCS
    "app_main.c"
    "adc.c"
    "data.c"
    "events.c"
    "fans.c"
    "http_server.c"
    "mqtt.c"
    "performance.c"
    "periodic.c"
    "tacho.c"
    "temperature.c"
    "wifi.c"
    "i2c_bus.c"

    "driver/shtc3.c"
        INCLUDE_DIRS ".")
