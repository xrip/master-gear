cmake_minimum_required(VERSION 3.22)

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
file(GLOB_RECURSE SRC "src/*.c" "src/*.h")

message(STATUS "Add source files:")
foreach(SRC_FILE IN LISTS SRC)
    message(STATUS "${SRC_FILE}")
endforeach()
message(STATUS "")

add_compile_options(-funroll-loops -fms-extensions -O3)
add_executable(${PROJECT_NAME} ${SRC})
add_compile_options(-fms-extensions)
target_compile_definitions(${PROJECT_NAME} PRIVATE
        EXECZ80
)
target_link_libraries(${PROJECT_NAME} PRIVATE winmm)

#target_include_directories(${PROJECT_NAME} PRIVATE src src/win32)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)