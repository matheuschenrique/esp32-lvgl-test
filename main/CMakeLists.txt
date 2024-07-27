idf_component_register(SRCS main.c lvgl_app.c
                        REQUIRES driver soc esp_timer lvgl_esp32_drivers lvgl nvs_flash esp_wifi lwip dht)
