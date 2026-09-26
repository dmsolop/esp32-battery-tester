idf_component_register(SRCS "ui_interface.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log system_state esp_driver_i2c esp_lcd)