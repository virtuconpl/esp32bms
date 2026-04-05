idf_component_register(
    SRCS "esp32_uart_sniffer.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_driver_uart
        esp_driver_usb_serial_jtag
        freertos
        esp_system
)