# STM32F0 Serial Bootloader

A lightweight, customizable **serial bootloader** for ARM Cortex-M0 based MCUs such as **STM32F0**, designed for firmware upgrades, application jumping, and low-level system initialization on bare-metal devices.  

This bootloader transfers firmware over **UART** using the **XMODEM-CRC** protocol, ensuring reliable and error-checked updates.  
Built with **CMake** and **STM32 HAL**, this project provides a clean foundation for implementing your own flashing/update protocol.


---

## Features

- Supports STM32F0 series (tested on **STM32F030CCTx**)  
- Fully bare-metal
- CMake build system  
- Application jump support  
- Startup assembly file included  
- Easy to extend with other serial comunication protocols.
---

## Bootloader Commands

The bootloader provides a simple UART command interface.  
Available commands:

- **info**: Display system information.

- **help**: Display all available bootloader commands.

- **upload**: Upload a new application binary to flash using the **XMODEM-CRC** protocol.

- **jump**: Jump to the user application.


## User Application Design
Before jumping to the user application, the memory layout of the device must accommodate both the bootloader and the application.  
The bootloader is placed at the beginning of flash memory and a fixed region is reserved for it:

- **Bootloader start address:** `0x08000000`  
- **Bootloader reserved size:** **20 KB**  
  - Actual size: ~9 KB in Release and ~20 KB in Debug  
  - Extra space is intentionally reserved to support future custom features (protocol extensions, fail-safe logic, additional peripherals, etc.)
- **User application start address:** `0x08005000` (immediately after the 20 KB bootloader region)

This structure ensures that both the bootloader and application coexist safely without overlap.

### Cortex-M0 Vector Table Remapping

On Cortex-M0 devices (including STM32F0), the vector table cannot be relocated using a register like `VTOR` because this feature does **not** exist on Cortex-M0 cores.  
After initialization, the MCU always expects the vector table at address **0x00000000**, which becomes an alias of **SRAM** once the system is running.

To allow the bootloader to correctly start a user application, the following approach is used:

1. **Reserve a portion of SRAM** to hold the application's vector table.  
2. **Copy the application's vector table into this reserved SRAM region** before jumping.  
3. **Link the user application** so its RAM starts *after* the reserved vector table space.

This ensures interrupt handlers and system exceptions map correctly for the user application.

### User Application Linker Script

Example linker script configuration for a Cortex-M0 user application:

```ld
/* Vector table size: 48 entries * 4 bytes = 0xC0 */
_Vector_Table_Size = 0xC0;

/* Memories definition */
MEMORY
{
  RAM           (xrw) : ORIGIN = 0x200000C0, LENGTH = 32K - _Vector_Table_Size
  FLASH         (rx)  : ORIGIN = 0x08005000, LENGTH = 236K
  BOOTLOADER    (rx)  : ORIGIN = 0x08000000, LENGTH = 20K
}
```

## Build the Project

### Requirements
Make sure the following tools are installed:

- **ARM GCC Toolchain** (`gcc-arm-none-eabi`)
- **CMake ≥ 3.15**
- **Make**
- (Optional) **OpenOCD** or **STM32CubeProgrammer** for flashing


### Build Steps
```sh
mkdir build && cd build
cmake ..
make
```
## Flashing the Bootloader

After building the project, the bootloader binary (`bootloader.bin`) is generated inside the **build** directory.

You can flash it to the MCU using an ST-Link programmer:

```sh
st-flash write bootloader.bin 0x08000000
```

## Flashing the First User Application Using the Bootloader

Once the bootloader is flashed and the board has started, follow these steps to flash the user application:

1. Open a serial terminal using `minicom` (or `Tera Term` with local echo enabled): `sudo minicom -b 115200 -D /dev/ttyUSBx`

2. Press Enter to wake the board and see the `->` prompt.

3. Type `upload` and press Enter; `C` characters will appear indicating XMODEM is ready. The transfer will be aborted if no binary is sent within 2 minutes.

4. Transfer the firmware binary using XMODEM and wait until the transfer completes.

5. Type `jump` and press Enter to run the flashed user application.

