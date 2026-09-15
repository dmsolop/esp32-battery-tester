idf_component_register(SRCS "cli.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log system_state adc_driver)
                       