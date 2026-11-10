idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES buzzer_service safety_monitor system_state load_control ui_interface telemetry adc_driver temp_sensor cli nvs_flash)