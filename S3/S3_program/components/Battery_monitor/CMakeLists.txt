idf_component_register(SRCS "Battery_monitor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 
                    esp_adc 
                    driver)
