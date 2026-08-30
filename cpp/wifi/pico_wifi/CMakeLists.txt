cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

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

set(PICO_BOARD pico2_w)
set(PICO_CYW43_SUPPORTED 1)

include(pico_sdk_import.cmake)

project(pico2_ble LANGUAGES C CXX ASM)

pico_sdk_init()

add_executable(pico2_ble
    main.cpp
)

pico_enable_stdio_usb(pico2_ble 1)
pico_enable_stdio_uart(pico2_ble 0)

target_link_libraries(pico2_ble
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_sockets
)

target_compile_definitions(pico2_ble PRIVATE
    CYW43_ENABLE_BLUETOOTH=1
)


target_include_directories(pico2_ble PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(pico2_ble)
