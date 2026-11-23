idf_component_register(SRCS "movingaverage.c" "main.c"
                    REQUIRES esp_driver_i2s esp_driver_gpio esp_adc esp_driver_gptimer esp_timer i2s_examples_common
                    EMBED_FILES "ChurchOrgan.wav" "PanFlute.wav" "Brass.wav"
                       )
