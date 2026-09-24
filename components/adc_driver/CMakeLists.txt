idf_component_register(SRCS "adc_driver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES load_control esp_driver_gpio freertos log esp_driver_i2c)