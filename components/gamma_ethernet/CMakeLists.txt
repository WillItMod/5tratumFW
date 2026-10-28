idf_component_register(
    SRCS "gamma_ethernet.c" "gamma_w5500.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif
    PRIV_REQUIRES esp_driver_spi esp_timer esp_hw_support esp_event
)
