# STM32F0 Secure Bootloader

A bare-metal secure UART bootloader for the STM32F030CCTx (Cortex-M0).
Firmware is transferred using XMODEM-CRC. When security is enabled, the bootloader verifies the firmware's Ed25519 signature before installing it.

## Features

* STM32F030CCTx support (256 KB flash, 32 KB SRAM)
* UART command-line interface
* XMODEM-CRC firmware transfer
* Optional Ed25519 signature verification
* CMake build system

## Security — Ed25519 Signature Verification

Build with `-DSECURITY_ENABLED=ON` to enable signature verification.

When enabled, the bootloader rejects firmware with a missing or invalid signature. The signature is checked against the public key configured in `Core/Inc/bootloader_signature.h`.

The signing tool is available here: [Embedded-signing-tool](https://github.com/ALICHOUCHENE/Embedded-signing-tool)

Use the signing tool to generate keys and sign the firmware binary before uploading it. The matching public key must be configured in the bootloader.

**Keep the private key secure.** Do not commit it to the repository or include it in the device firmware. Only the public key should be stored in the bootloader. Anyone with the private key can create firmware that passes the signature check.

Signature verification checks firmware authenticity and integrity. It does not encrypt the firmware or guarantee that the signed firmware is safe.

## CLI Commands

The bootloader uses UART at 115200 baud, 8N1.

Command  | Description
--- | ---
`info`  | Display MCU information
`help`  | List available commands
`upload`  | Start an XMODEM-CRC firmware upload
`jump`  | Start the user application

## User Application Design

### Memory layout

The user application must be linked to start at `0x0800A000` and must leave the first `0xC0` bytes of SRAM free for the bootloader to copy the application vector table into.

Use the application linker script with the following memory layout:

    /* Vector table: 48 entries × 4 bytes = 0xC0 */
    _Vector_Table_Size = 0xC0;

    MEMORY
    {
      RAM         (xrw) : ORIGIN = 0x200000C0, LENGTH = 32K - _Vector_Table_Size
      FLASH       (rx)  : ORIGIN = 0x0800A000, LENGTH = 216K
      BOOTLOADER  (rx)  : ORIGIN = 0x08000000, LENGTH = 40K
    }

The `BOOTLOADER` region is declared read-only. It prevents the application from being linked over the bootloader area.

### Vector table

The STM32F030 uses a Cortex-M0 core and does not have a `VTOR` register. The bootloader copies the application vector table to the reserved SRAM area before jumping to the application.

The application's startup code should not relocate the vector table again.

## Build

### Requirements

Tool  | Version
--- | ---
`arm-none-eabi-gcc`  | ≥ 10
`cmake`  | ≥ 3.15
`make`  | any recent
`st-flash` or STM32CubeProgrammer  | for flashing

### Build commands

Without signature verification:

    cmake -B build -DSECURITY_ENABLED=OFF
    cmake --build build

With signature verification:

    cmake -B build -DSECURITY_ENABLED=ON
    cmake --build build

The build produces `build/bootloader.bin` and `build/bootloader.elf`.

## Flashing the Bootloader

Using `st-flash` with an ST-Link programmer:

    st-flash write build/bootloader.bin 0x08000000

Using STM32CubeProgrammer CLI:

    STM32_Programmer_CLI -c port=SWD -w build/bootloader.bin 0x08000000 -v -rst

## Uploading a User Application

1. Connect the board's UART TX/RX pins to a USB-UART adapter.

2. Open Tera Term or Minicom at 115200 baud, 8N1, and enable **local echo**.

       sudo minicom -b 115200 -D /dev/ttyUSB0

   In Tera Term, configure this under `Setup → Serial port → Local echo`.

3. Reset the board. The bootloader banner appears. Press Enter to see the `->` prompt.

4. Type `upload` and press Enter. The bootloader sends `C` characters when it is ready for XMODEM-CRC.

5. Start an XMODEM-CRC send from the terminal and select the firmware binary.

       * minicom: `Ctrl-A S` → `xmodem` → select the file
       * Tera Term: `File → Transfer → XMODEM → Send` → select the file

   If security is enabled, send the binary produced by the signing tool.

6. Wait for the transfer to finish. The bootloader reports the authentication result when signature verification is enabled.

7. Type `jump` and press Enter to run the application.

## License

MIT. See `LICENSE` for details.
