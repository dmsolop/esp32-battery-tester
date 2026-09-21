idf_component_register(SRCS "pid_service.c" "dcir_service.c" "pwm_driver.c" "load_control.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log system_state esp_timer adc_driver esp_driver_ledc)