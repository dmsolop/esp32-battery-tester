idf_component_register(SRCS "adc_driver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log esp_driver_i2c)