# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow_wifi_scan LANGUAGES C CXX ASM)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
add_subdirectory(no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI)

add_executable(picow_wifi_scan_poll
        picow_wifi_scan.c
        modules/display.c
        modules/mpu6050_i2c.cpp
        web/page.c
        )

target_include_directories(picow_wifi_scan_poll PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

pico_enable_stdio_usb(picow_wifi_scan_poll 1)
pico_enable_stdio_uart(picow_wifi_scan_poll 0)

target_link_libraries(picow_wifi_scan_poll
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pwm
    pico_multicore
    pico_stdlib
    pico_aon_timer
    FatFs_SPI
)

target_compile_definitions(picow_wifi_scan_poll PRIVATE
    PICO_LWIP_TIMEVAL_PRIVATE=1
)
pico_add_extra_outputs(picow_wifi_scan_poll)

