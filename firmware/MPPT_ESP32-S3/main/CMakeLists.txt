idf_component_register(SRCS "main.c" "mppt_control.c" "acquisition_pipeline.c"
                            "acquisition_gate.c" "acquisition_worker.c" "ina226_bus.c" "serial_link.c"
                            "quiet_window.c" "environment_sensors.c" "environment_state.c" "mppt_loads.c"
                            "load_trial.c" "load_trial_worker.c"
                       INCLUDE_DIRS "."
                       LDFRAGMENTS "mppt_hot.lf"
                       PRIV_REQUIRES driver esp_driver_gpio esp_driver_ledc esp_timer
                                     esp_driver_uart esp_driver_usb_serial_jtag espressif__cjson mppt
                                     espressif__onewire_bus espressif__ds18b20)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-fast-math -ffp-contract=off)
# Forzar la extraccion del objeto: libmain puede aparecer antes que libmppt.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=quietWindow" "-Wl,--undefined=__wrap_quietWindow")
