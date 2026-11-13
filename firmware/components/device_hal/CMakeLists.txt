idf_component_register(
    SRCS
        "esp/EspWifiNetwork.cpp"
        "esp/Es8311Audio.cpp"
        "esp/SharedI2cBus.cpp"
        "esp/Ft6336Touch.cpp"
        "esp/NvsStorage.cpp"
        "esp/EspSntpClock.cpp"
        "esp/WakeWord.cpp"
        "esp/OpenMeteoWeatherProvider.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core esp_event freertos esp_codec_dev driver nvs_flash esp_http_client
    PRIV_REQUIRES esp_wifi esp_netif esp_timer lwip json mbedtls
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
