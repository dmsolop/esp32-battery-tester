idf_component_register(SRCS "safety_monitor.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos log system_state load_control)
