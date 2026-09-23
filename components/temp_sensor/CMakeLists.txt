idf_component_register(SRCS "temp_service.c" "ds18b20.c" "onewire.c" "onewire.c"
                       INCLUDE_DIRS "include"
                       REQUIRES system_state esp_driver_gpio esp_timer)