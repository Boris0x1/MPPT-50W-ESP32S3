file(GLOB kernel "generated/*.c")
idf_component_register(SRCS ${kernel}
    "src/mppt.c" "src/local_article.c"
    "src/po_mppt.c" "src/inc_mppt.c" "src/meng_vi_mppt.c"
    "src/adaptive_meng_mppt.c" "src/po_with_probing_mppt.c"
    "src/adaptive_meng_with_probing_mppt.c" "src/adaptive_meng_without_probing_mppt.c"
    "src/ina226_port.c" "src/model_output.c" "src/pwm_mapping.c"
    INCLUDE_DIRS "include" "generated")
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-fast-math -ffp-contract=off)
