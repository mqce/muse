idf_component_register(
    SRCS "voicevox_tts.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client espressif__cjson freertos heap
)
