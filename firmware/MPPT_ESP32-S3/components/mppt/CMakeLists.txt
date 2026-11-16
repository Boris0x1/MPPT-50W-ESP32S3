file(GLOB kernel "generated/*.c")
idf_component_register(SRCS ${kernel} "src/mppt.c" "src/po_ajustado.c" "src/meng_vi_mas.c" "src/combinacion_final.c" "src/ina226_port.c" "src/model_output.c" "src/pwm_mapping.c" INCLUDE_DIRS "include" "generated")
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-fast-math -ffp-contract=off)
