idf_component_register(SRCS "ui_interface.c"
                       INCLUDE_DIRS "include"
                       REQUIRES display_engine freertos log system_state esp_driver_pcnt esp_driver_gpio)