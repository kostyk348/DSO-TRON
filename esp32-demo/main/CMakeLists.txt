idf_component_register(
    SRCS app_main.c csi_capture.c webserver.c chaos.c
          ../../sysdepend/xtensa/timer.c
          ../../sysdepend/xtensa/serial.c
          ../../kernel/dso_common.c
          ../../kernel/dso_tsk.c
          ../../kernel/dso_dtq.c
          ../../kernel/dso_init.c
          ../../kernel/dso_stubs.c
    INCLUDE_DIRS . ../../include ../../kernel ../../sysdepend/xtensa
    REQUIRES esp_wifi esp_http_server nvs_flash esp_timer driver
)
add_compile_definitions(DSO_ARCH_XTENSA)
