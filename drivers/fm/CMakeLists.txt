add_library(fm INTERFACE)

target_sources(fm INTERFACE ${CMAKE_CURRENT_LIST_DIR}/fm.c)

target_link_libraries(fm INTERFACE hardware_pio hardware_clocks hardware_dma)

target_include_directories(fm INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

