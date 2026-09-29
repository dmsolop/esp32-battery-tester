idf_component_register(SRCS "input_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_driver_pcnt esp_driver_gpio)
                       