idf_component_register(SRCS "load_control.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log system_state esp_timer)