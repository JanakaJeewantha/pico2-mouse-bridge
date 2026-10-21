cmake_minimum_required(VERSION 3.17)

# ── pico-sdk import (before project()) ───────────────────────────────────────
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_mouse_bridge C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico2_mouse_bridge
    src/main.c
    src/hid_device.c
    src/hid_app.c
    src/vk_to_hid.c
    src/usb_descriptors.c
)

target_include_directories(pico2_mouse_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tusb_config
)

# RP2350A has two native USB PHYs:
#   USB0 (native) -> tinyusb_device  (PC-facing, bInterval=1, 1000 Hz)
#   USB1 (native) -> tinyusb_host    (mouse-facing, reads G502)
# No PIO bit-bang needed. pico_pio_usb is NOT required.
target_link_libraries(pico2_mouse_bridge
    pico_stdlib
    pico_multicore
    pico_unique_id
    tinyusb_device
    tinyusb_host
    tinyusb_board
)

# UF2 for drag-and-drop flashing
pico_add_extra_outputs(pico2_mouse_bridge)

# No USB CDC/serial stdio – stealth mode
pico_enable_stdio_usb(pico2_mouse_bridge  0)
pico_enable_stdio_uart(pico2_mouse_bridge 0)
