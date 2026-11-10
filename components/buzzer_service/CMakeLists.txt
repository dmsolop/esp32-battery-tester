idf_component_register(SRCS "buzzer_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log nvs_flash esp_driver_mcpwm driver)
