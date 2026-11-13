idf_component_register(
    SRCS
        "src/DisplayDriver.cpp"
        "src/VaiwuUi.cpp"
        "src/sprites.c"
        "src/icons.c"
        "src/fonts/vaiwu_font_12.c"
        "src/fonts/vaiwu_font_14.c"
        "src/fonts/vaiwu_font_16.c"
        "src/fonts/vaiwu_font_20.c"
        "src/fonts/vaiwu_font_24.c"
    INCLUDE_DIRS "include"
    REQUIRES core game chat lvgl esp_lvgl_port
    PRIV_REQUIRES esp_lcd esp_lcd_ili9341 driver
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
