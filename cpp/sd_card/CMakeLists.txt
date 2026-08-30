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
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(sd_card C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
# Pull in the FatFS SPI library and the FatFS core
add_subdirectory(no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI)

# Build your main app
add_executable(sd_card sd_card.cpp modules/display.cpp modules/mpu6050_i2c.cpp)

pico_set_program_name(sd_card "sd_card")
pico_set_program_version(sd_card "0.1")

# Optional: enable or disable UART/USB I/O
pico_enable_stdio_uart(sd_card 0)
pico_enable_stdio_usb(sd_card 0)

# Link everything needed for Pico 2 + SD + FatFS
target_link_libraries(sd_card
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pwm
    pico_multicore       
    pico_aon_timer       # RP2350 replacement for RTC
    FatFs_SPI            # SD-SPI driver + glue + sd_get_by_num()
)

# Include your current directory for headers
target_include_directories(sd_card PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Generate .uf2/.bin outputs
pico_add_extra_outputs(sd_card)
