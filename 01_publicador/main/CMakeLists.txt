idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        esp_netif
        esp_event
        protocol_examples_common
        esp_driver_tsens
        mqtt
)
