cmake_minimum_required(VERSION 3.17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_mouse_bridge C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Pico-PIO-USB path (set via -D in cmake or env var, default /opt/pico-pio-usb)
if(DEFINED ENV{PICO_PIO_USB_PATH} AND NOT PICO_PIO_USB_PATH)
    set(PICO_PIO_USB_PATH $ENV{PICO_PIO_USB_PATH})
endif()
if(NOT PICO_PIO_USB_PATH)
    set(PICO_PIO_USB_PATH "/opt/pico-pio-usb")
endif()
message(STATUS "Pico-PIO-USB: ${PICO_PIO_USB_PATH}")

add_executable(pico2_mouse_bridge
    src/main.c
    src/hid_device.c
    src/hid_app.c
    src/vk_to_hid.c
    src/usb_descriptors.c
    # Pico-PIO-USB host controller (provides hcd_* symbols)
    ${PICO_PIO_USB_PATH}/src/pio_usb.c
    ${PICO_PIO_USB_PATH}/src/pio_usb_host.c
    ${PICO_PIO_USB_PATH}/src/usb_crc.c
)

# Pre-built .pio.h headers already exist in the repo - just include the dir
target_include_directories(pico2_mouse_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tusb_config
    ${PICO_PIO_USB_PATH}/src
)

target_link_libraries(pico2_mouse_bridge
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_irq
    tinyusb_device
    tinyusb_host
    tinyusb_board
)

pico_add_extra_outputs(pico2_mouse_bridge)
pico_enable_stdio_usb(pico2_mouse_bridge  0)
pico_enable_stdio_uart(pico2_mouse_bridge 0)
