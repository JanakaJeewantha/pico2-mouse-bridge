cmake_minimum_required(VERSION 3.17)

# Pull in pico-sdk (must be done before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_mouse_bridge C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ── pio-usb (needed for the host-side second USB port) ───────────────────────
# pico-sdk bundles tinyusb; we just need to add the pio-usb driver.
# If your pico-sdk version ships pico_pio_usb as a separate target, link it.
# Otherwise tinyusb host mode handles it automatically when CFG_TUH_ENABLED=1.

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

# Link pico-sdk libraries
target_link_libraries(pico2_mouse_bridge
    pico_stdlib
    pico_multicore
    tinyusb_device
    tinyusb_host
    tinyusb_board
)

# Create UF2 for drag-and-drop flashing
pico_add_extra_outputs(pico2_mouse_bridge)

# Optional: disable USB serial / stdio on the device port (stealth)
pico_enable_stdio_usb(pico2_mouse_bridge 0)
pico_enable_stdio_uart(pico2_mouse_bridge 0)
