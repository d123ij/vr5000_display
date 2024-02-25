add_executable(vr5000_disp
        vr5000_disp.c
        disp_func.c
)
pico_generate_pio_header(vr5000_disp ${CMAKE_CURRENT_LIST_DIR}/vr5000_disp.pio)
target_link_libraries(vr5000_disp pico_stdlib freertos hardware_pio hardware_spi hardware_dma)
pico_add_extra_outputs(vr5000_disp)
