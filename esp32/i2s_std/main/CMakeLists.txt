idf_component_register(SRCS "i2s_std_example_main.c"
                    PRIV_REQUIRES esp_driver_i2s esp_driver_gpio esp_adc esp_driver_gptimer esp_timer i2s_examples_common
                    INCLUDE_DIRS ".")
