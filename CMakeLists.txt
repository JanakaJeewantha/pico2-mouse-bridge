cmake_minimum_required(VERSION 3.17)

# ── pico-sdk import (must be before project()) ────────────────────────────────
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_mouse_bridge C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ── pio-usb: second USB port (host side) uses Pico PIO bit-bang ──────────────
# pico-sdk ≥ 2.0 ships pico_pio_usb in lib/Pico-PIO-USB.
# We generate the PIO header before compiling.
set(PICO_PIO_USB_PATH "${PICO_SDK_PATH}/lib/Pico-PIO-USB")

# Generate PIO header from .pio files
pico_generate_pio_header(pico2_mouse_bridge
    ${PICO_PIO_USB_PATH}/src/pio_usb_host.pio
)

add_executable(pico2_mouse_bridge
    src/main.c
    src/hid_device.c
    src/hid_app.c
    src/vk_to_hid.c
    src/usb_descriptors.c

    # pio-usb source files (host controller)
    ${PICO_PIO_USB_PATH}/src/pio_usb.c
    ${PICO_PIO_USB_PATH}/src/pio_usb_host.c
    ${PICO_PIO_USB_PATH}/src/usb_crc.c
)

target_include_directories(pico2_mouse_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tusb_config
    ${PICO_PIO_USB_PATH}/src
)

# TinyUSB needs to know about our config header location
target_compile_definitions(pico2_mouse_bridge PRIVATE
    # Tell TinyUSB to use our tusb_config.h
    CFG_TUSB_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tusb_config/tusb_config.h"
    # Use pio-usb as the host controller
    CFG_TUH_RPI_PIO_USB=1
)

target_link_libraries(pico2_mouse_bridge
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_irq
    # TinyUSB device + host — correct pico-sdk 2.x target names
    tinyusb_device
    tinyusb_host
    tinyusb_board
    # pio-usb host controller HAL
    pico_pio_usb
)

# Create UF2 for drag-and-drop flashing
pico_add_extra_outputs(pico2_mouse_bridge)

# No USB serial / UART stdio (stealth, no COM port)
pico_enable_stdio_usb(pico2_mouse_bridge 0)
pico_enable_stdio_uart(pico2_mouse_bridge 0)
