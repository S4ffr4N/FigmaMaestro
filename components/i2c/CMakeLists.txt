idf_component_register(SRCS "i2c.c" 
                        INCLUDE_DIRS "."
                        REQUIRES driver gpio esp_driver_i2c
                    )
