# Pico2 Mouse Bridge – Custom C Firmware for RP2350A

## What this does

| Feature | Detail |
|---|---|
| USB **Device** (to PC) | Composite HID: Mouse (ID 1) + Boot Keyboard (ID 3) + Vendor cmd channel (ID 2) |
| USB **Host** (from mouse) | Reads any standard HID mouse plugged into the board, forwards it as ID 1 |
| Polling rate | **1000 Hz** – `bInterval = 1` on IN endpoint |
| Key injection | 0xAB packets from PixelAssist app → hardware keystrokes (NeoStrafe / BHop) |
| Stealth | Appears to Windows as a Logitech G502 (VID 046D / PID C08B) |
| Update | 0xFF packet → board reboots into BOOTSEL automatically |

## File layout

```
pico2_mouse_bridge/
├── CMakeLists.txt
├── tusb_config/
│   └── tusb_config.h        ← TinyUSB device + host config
├── src/
│   ├── main.c               ← entry point, dual-core setup
│   ├── hid_device.c/h       ← PC-facing HID reports & 0xAA/0xAB/0xFF parser
│   ├── hid_app.c/h          ← Host-side mouse callback (reads G502)
│   ├── usb_descriptors.c    ← ALL USB descriptors (bInterval=1 here!)
│   └── vk_to_hid.c/h        ← Windows VK → HID keycode table
└── .github/workflows/
    └── build.yml            ← GitHub Actions CI (produces .uf2)
```

## How to build (GitHub Actions – recommended)

1. Push this folder to a GitHub repository.
2. Go to **Actions** → the `Build Pico2 Mouse Bridge Firmware` workflow fires automatically.
3. Download `pico2_mouse_bridge.uf2` from the workflow artifacts.

## How to flash

1. Hold the **BOOTSEL** button on the RP2350A and plug USB into the PC.
   - The board appears as a USB mass-storage drive (`RPI-RP2`).
2. Drag & drop `pico2_mouse_bridge.uf2` onto the drive.
3. The board reboots immediately.

If CIRCUITPY firmware is blocking BOOTSEL:
- Unplug the board.
- Hold BOOTSEL.
- Plug back in **while still holding**.
- Release after the RPI-RP2 drive appears.

## Wiring the second USB port (mouse connector)

The host port runs over **pio-usb** on GPIO 2 & 3.

| Signal | RP2350A GPIO |
|---|---|
| D+ | GP2 |
| D- | GP3 |
| VBUS | 5 V |
| GND | GND |

Connect these to a USB-A female connector (or a micro-B adapter if your board has one).

## Packet protocol (Report ID 2, OUT, 7 bytes)

| Byte | Meaning |
|---|---|
| [0] = 0xAA | Aim packet |
| [1..2] | DX signed 16-bit LE |
| [3..4] | DY signed 16-bit LE |
| [5] | flags (bit 0 = tracking active) |
| [6] | checksum = [1]^[2]^[3]^[4]^[5] |

| Byte | Meaning |
|---|---|
| [0] = 0xAB | Key inject |
| [1] | Windows VK code |
| [2] | 1 = key down, 0 = key up |
| [6] | checksum = [1]^[2] |

| Byte | Meaning |
|---|---|
| [0] = 0xFF | Reboot to BOOTSEL |
