idf_component_register(SRCS "src/uc.cpp"
                            "src/driver/sequansGM02S/gm02s.cpp"
                            "src/driver/espWifi/espWifi.cpp"
                    INCLUDE_DIRS "src"
                    REQUIRES esp_netif esp_wifi nvs_flash esp_modem driver esp_timer)