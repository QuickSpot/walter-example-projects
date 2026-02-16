idf_component_register(
    SRCS
    "main.cpp"
    "spi_helper.c"
    "Arducam_Mega.cpp"
    "Arducam/ArducamCamera.c"                 
    
    INCLUDE_DIRS "."
    REQUIRES 
        esp_netif
        esp_wifi
        nvs_flash
        esp_event
        esp_http_server
        lwip
        driver
                    )
