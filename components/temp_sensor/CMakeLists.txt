idf_component_register(SRCS "ds18b20.c" "onewire.c" "onewire.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_timer)