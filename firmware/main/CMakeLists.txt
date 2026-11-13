idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES core game chat ui device_hal freertos esp_timer
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
