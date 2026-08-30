cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(display_anim)

pico_sdk_init()

add_executable(display_anim main.cpp)
target_link_libraries(display_anim pico_stdlib hardware_spi fatfs)
pico_add_extra_outputs(display_anim)