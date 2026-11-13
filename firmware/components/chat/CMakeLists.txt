idf_component_register(
    SRCS "src/NeuralChat.cpp"
        "src/SpeechToText.cpp"
        "src/TextToSpeech.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core game esp_http_client freertos
    PRIV_REQUIRES json mbedtls esp_event
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
