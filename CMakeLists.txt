cmake_minimum_required(VERSION 3.22)
set(PICO_PLATFORM "host" CACHE STRING "Target platform")
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards )
set(PICO_BOARD "murmulator" CACHE STRING "Board type")

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(master-gear C CXX ASM)
enable_language(CXX)
SET(BUILD_NAME "${PROJECT_NAME}")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${PICO_PLATFORM}/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC "src/z80/*.c" "src/emu2413.c" "src/sn76489.c" "src/vdp.c")

message(STATUS "Add source files:")
foreach(SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach()
message(STATUS "")

if (PICO_PLATFORM STREQUAL "host")
    add_compile_options(-funroll-loops -fms-extensions  -O3)
    add_executable(${PROJECT_NAME} ${SRC} src/win32-main.c src/win32/WinMiniFB.c)
target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
else()
    pico_sdk_init()
    add_compile_options(-funroll-loops -ffast-math -ffunction-sections -fdata-sections -fms-extensions -O3)
    add_executable(${PROJECT_NAME} ${SRC} src/pico-main.cpp)

    pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
    target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
    pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)



    add_subdirectory(drivers/ps2kbd)
    add_subdirectory(drivers/fatfs)
    add_subdirectory(drivers/sdcard)
    add_subdirectory(drivers/vga-nextgen)

    add_subdirectory(drivers/graphics)
    add_subdirectory(drivers/audio)
    add_subdirectory(drivers/nespad)

    target_compile_definitions(${PROJECT_NAME} PRIVATE
            VGA
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE vga-nextgen)
    SET(BUILD_NAME "${BUILD_NAME}-VGA")


    target_link_libraries(${PROJECT_NAME} PRIVATE
            graphics
            audio

            nespad
            sdcard
            ps2kbd
            fatfs

            pico_runtime
            pico_stdlib
            pico_multicore
            hardware_dma
            hardware_pio
            hardware_i2c
            hardware_exception
            hardware_interp
            hardware_timer
            hardware_clocks
            hardware_pwm
            hardware_flash

            tinyusb_board
            tinyusb_device

    )
    pico_add_extra_outputs(${PROJECT_NAME})
    target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE src/)

target_compile_definitions(${PROJECT_NAME} PRIVATE EXECZ80)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
