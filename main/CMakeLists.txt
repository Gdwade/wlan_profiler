idf_component_register(SRCS "linked_list.c" "wlan_profiler.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       REQUIRES driver nvs_flash esp_wifi esp_event esp_netif
                       )
