idf_component_register(
    SRCS
        "main.c"
        "ui/ui.c"
        "ui/img_tab1.c"
        "ui/img_tab2.c"
        "ui/img_tab3.c"
        "ui/audio_player.c"
        "ui/cn_font.c"
    INCLUDE_DIRS
        "."
        "ui"
    REQUIRES esp32_p4_wifi6_touch_lcd_7b lvgl__lvgl esp_codec_dev fatfs sdmmc driver
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(${LVGL_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)
