# STM32F0 Secure Bootloader

A lightweight, bare-metal **secure bootloader** for ARM Cortex-M0 MCUs, targeting the **STM32F030CCTx** (256 KB flash).  
Firmware is transferred over UART using the **XMODEM-CRC** protocol into a dedicated staging area, optionally verified with an **Ed25519 digital signature** (via [Monocypher](https://monocypher.org/)), and only then copied into the application area before execution.

Built with **CMake** and the **STM32 HAL**, the project is designed to be readable, portable to other STM32F0 variants, and easy to extend.

---

## Table of Contents

1. [Features](#features)
2. [Project Structure](#project-structure)
3. [Security — Ed25519 Signature Verification](#security--ed25519-signature-verification)
4. [Bootloader Entry Logic](#bootloader-entry-logic)
5. [CLI Commands](#cli-commands)
6. [User Application Design](#user-application-design)
7. [Signing the Application Binary](#signing-the-application-binary)
8. [Build](#build)
9. [Flashing the Bootloader](#flashing-the-bootloader)
10. [Uploading a User Application](#uploading-a-user-application)

---

## Features

- Targets **STM32F030CCTx** (Cortex-M0, 256 KB flash, 32 KB SRAM)
- Fully **bare-metal** — no OS, no dynamic allocation
- **XMODEM-CRC** firmware receive over UART
- **Staging area** — incoming binary is buffered in flash before being promoted to the application area
- Optional **Ed25519 signature verification** (Monocypher) enabled at compile time via `-DSECURITY_ENABLED=ON`
- Signed-binary **footer** carries the file length and 64-byte Ed25519 signature
- Simple UART **CLI** (info / help / upload / jump)
- **Vector-table remapping** to SRAM for correct interrupt delivery to the user application on Cortex-M0 (no VTOR)
- **CMake** build system with optional security flag
- Clean **serial abstraction layer** (function-pointer interface) — easy to swap UART implementation

---

## Project Structure

```
stm32f0-bootloader/
├── Core/
│   ├── Inc/
│   │   ├── bootloader.h            # Bootloader API, memory-map macros
│   │   └── bootloader_signature.h  # Footer structure, auth status codes,
│   │                               #   Ed25519 public key (SECURITY_ENABLED)
│   ├── Src/
│   │   ├── main.c                  # System init → bootloader_entry()
│   │   ├── bootloader.c            # CLI, XMODEM callbacks, signature verify,
│   │   │                           #   staging-to-application move, jump
│   │   └── system.c                # Clock / peripheral init (HAL)
│   └── Startup/
│       └── startup_stm32f030cctx.s # Reset handler, vector table
│
├── Middleware/
│   ├── Inc/
│   │   ├── flash.h                 # Flash erase/write/move API
│   │   ├── xmodem.h                # XMODEM-CRC receiver API
│   │   └── serial.h                # Serial abstraction (function-pointer interface)
│   └── Src/
│       ├── flash.c                 # HAL_FLASHEx_Erase + HAL_FLASH_Program wrappers
│       ├── xmodem.c                # XMODEM-CRC state machine
│       └── serial.c                # USART1 ISR-driven RX, HAL TX
│
├── Crypto/
│   ├── Inc/
│   │   └── crypto_dsa.h            # Ed25519 verify wrapper
│   ├── Src/
│   │   └── crypto_dsa.c            # Calls Monocypher crypto_eddsa_check()
│   └── Libraries/
│       └── Src/
│           └── monocypher.c        # Vendored Monocypher library
│
├── STM32F030CCTX_FLASH.ld          # Linker script
└── CMakeLists.txt
```

---

## Security — Ed25519 Signature Verification

When built with `-DSECURITY_ENABLED=ON`, the bootloader **refuses to install any unsigned or invalidly signed binary**.

### How it works

1. The host signing tool appends a **footer** to the firmware binary before transfer:

```
┌─────────────────────────────────────────────────────────┐
│                  Firmware binary (N bytes)              │
├──────────┬──────────────┬───────────────────────────────┤
│  magic   │ file_length  │       Ed25519 signature       │
│ 4 bytes  │   4 bytes    │         64 bytes              │
│0x424F4F54│      N       │  over binary[0..N-1]          │
└──────────┴──────────────┴───────────────────────────────┘
         ▲
   "BOOT" in ASCII — identifies the footer
```

2. XMODEM-CRC delivers the signed binary (firmware + footer) to the staging area in flash.
3. On transfer completion, the bootloader:
   - Strips XMODEM's trailing `0x1A` padding bytes.
   - Scans backward to locate the footer by its magic number (`0x424F4F54`).
   - Checks that the declared `file_length` matches the found binary size.
   - Calls `crypto_eddsa_check()` (Monocypher Ed25519) against the 32-byte public key embedded in `bootloader_signature.h`.
4. Only on `authentication_status_success` is the binary moved from staging to the application area.

### Authentication status codes

| Status | Meaning |
|---|---|
| `authentication_status_success` | Signature valid — binary promoted |
| `authentication_status_footer_not_found` | Footer magic absent |
| `authentication_status_file_length_mismatch` | Declared length ≠ actual length |
| `authentication_status_invalid_signature` | Ed25519 verification failed |

### Configuring the public key

Replace the placeholder bytes in `Core/Inc/bootloader_signature.h`:

```c
#ifdef SECURITY_ENABLED
static const uint8_t dsa_public_key[32] = {
    0x96, 0xf3, /* ... 30 more bytes from your key pair ... */
};
#endif
```

The matching key pair and the signing tool are available at the **[Embedded-signing-tool](https://github.com/ALICHOUCHENE/Embedded-signing-tool)** repository.

---

## Bootloader Entry Logic

Currently, `main.c` calls `bootloader_entry()` unconditionally on every reset:

```c
int main(void) {
    system_init();
    HAL_Delay(500);
    bootloader_entry();   // always enters bootloader
}
```

For a production deployment, add a **pin-based or flag-based entry check** before the `bootloader_entry()` call.  
Two common patterns:

**Option A — GPIO pin held at reset:**
```c
// Enter bootloader only if BOOT pin is held low at reset
if (HAL_GPIO_ReadPin(BOOT_GPIO_Port, BOOT_Pin) == GPIO_PIN_RESET) {
    bootloader_entry();
}
bootloader_exit();   // otherwise jump straight to the application
```

**Option B — Persistent flag in a backup register:**
```c
if (RTC->BKP0R == BOOTLOADER_MAGIC) {
    RTC->BKP0R = 0;
    bootloader_entry();
}
bootloader_exit();
```

---

## CLI Commands

After reset the bootloader prints a welcome banner over UART (115200 baud, 8N1) and waits for commands at the `->` prompt.

| Command | Description |
|---|---|
| `info` | Print MCU architecture, core, device, and flash size |
| `help` | List available commands |
| `upload` | Erase staging area and start XMODEM-CRC receive. Sends `C` to signal CRC mode. Aborts automatically after ~2 minutes of inactivity. |
| `jump` | Validate the application vector table, copy it to SRAM, and branch to the application reset handler |

Example session:
```
================================================
        STM32 Bootloader v1.0.0
================================================
-> help
=================== Bootloader Command Menu ===================
Available commands:
  info     : Display system information
  help     : Display bootloader commands
  upload   : Upload a new application binary to flash using XMODEM
  jump     : Jumping to the user application
-> upload
Starting binary upload via XMODEM...
CCCC...
[transfer completes]
Signed binary authenticated!
-> jump
Jump to application...
```

---

## User Application Design

### Memory layout

The user application must be linked to start at `0x0800A000` and must leave the first `0xC0` bytes of SRAM free for the bootloader to copy the application vector table into.

Example linker script for the user application:

```ld
/* Vector table: 48 entries × 4 bytes = 0xC0 */
_Vector_Table_Size = 0xC0;

MEMORY
{
  RAM         (xrw) : ORIGIN = 0x200000C0, LENGTH = 32K - _Vector_Table_Size
  FLASH       (rx)  : ORIGIN = 0x0800A000, LENGTH = 216K
  BOOTLOADER  (rx)  : ORIGIN = 0x08000000, LENGTH = 40K
}
```

> The `BOOTLOADER` region is declared read-only — the linker uses it only to prevent the application from accidentally overlapping the bootloader.

### Vector table remapping on Cortex-M0

Cortex-M0 does **not** have a `VTOR` (Vector Table Offset Register), so the CPU always fetches interrupt vectors from address `0x00000000`, which is aliased to SRAM via the boot-mode pin.

The bootloader handles this transparently before jumping:

```
1. memcpy(0x20000000, 0x0800A000, 0xC0)   ← copy app vectors into reserved SRAM
2. Disable all interrupts
3. Set MSP ← app_stack_pointer  (first word at 0x0800A000)
4. Branch to app_reset_handler  (second word at 0x0800A000)
```

Your application's startup code should **not** attempt to relocate the vector table — the bootloader has already done it.

---

## Signing the Application Binary

The **[Embedded-signing-tool](https://github.com/ALICHOUCHENE/Embedded-signing-tool)** is a host-side C utility that appends the 72-byte Ed25519 footer (magic + file length + 64-byte signature) that the bootloader expects.

### 1. Build the signing tool

```sh
cd signing_tool
mkdir build && cd build
cmake ..
make
# produces: build/signing_tool
```

### 2. Sign the application binary

On the **first run**, a fresh Ed25519 key pair is generated automatically from a `getrandom(2)` seed and saved to `keys/keys.txt`. Subsequent runs reuse the same keys.

```sh
./build/signing_tool sign app.bin
```

```
binary file: app.bin
Keys file found, read keys...
Generating signed binary file
```

Output: `app.bin.sign` — the original binary with the 72-byte footer appended. This is the file to transfer via XMODEM.

### 3. (Optional) Verify the signed binary on the host

```sh
./build/signing_tool verify app.bin.sign
```

```
binary file: app.bin.sign
Keys file found, read keys...
Signature is valid
```

### 4. Extract the public key and update the bootloader

The public key is stored as the second line of `keys/keys.txt` (64 lowercase hex characters = 32 bytes). Convert it to a C byte array and paste it into `Core/Inc/bootloader_signature.h`:

```c
#ifdef SECURITY_ENABLED
static const uint8_t dsa_public_key[32] = {
    0xAB, 0xCD, /* ... all 32 bytes from the second line of keys/keys.txt ... */
};
#endif
```

Then rebuild the bootloader with `-DSECURITY_ENABLED=ON`.

> **Key security:** `keys/keys.txt` contains the **private key in plaintext**. Restrict its permissions and never commit it to version control:
> ```sh
> chmod 600 keys/keys.txt
> echo "keys/keys.txt" >> .gitignore
> ```
> Regenerating the key pair (by deleting `keys/keys.txt`) invalidates all previously signed binaries.

The original `app.bin` will be **rejected** by a bootloader built with `SECURITY_ENABLED=ON`.

---

## Build

### Requirements

| Tool | Version |
|---|---|
| `arm-none-eabi-gcc` | ≥ 10 |
| `cmake` | ≥ 3.15 |
| `make` | any recent |
| `st-flash` or STM32CubeProgrammer | for flashing |

### Build commands

**Without signature verification (development/debug):**
```sh
cmake -B build -DSECURITY_ENABLED=OFF
cmake --build build
```

**With signature verification (production):**
```sh
cmake -B build -DSECURITY_ENABLED=ON
cmake --build build
```

The build produces `build/bootloader.bin` and `build/bootloader.elf`.

Compiler flags used: `-mcpu=cortex-m0 -mthumb -Os -ffreestanding -ffunction-sections -fdata-sections`  
No standard C library is linked (`-lgcc` only).

---

## Flashing the Bootloader

Using `st-flash` with an ST-Link programmer:

```sh
st-flash write build/bootloader.bin 0x08000000
```

Using STM32CubeProgrammer CLI:

```sh
STM32_Programmer_CLI -c port=SWD -w build/bootloader.bin 0x08000000 -v -rst
```

---

## Uploading a User Application

1. Connect the board's UART TX/RX pins to a USB-UART adapter.

2. Open a serial terminal at **115200 baud, 8N1** with local echo enabled:
   ```sh
   sudo minicom -b 115200 -D /dev/ttyUSB0
   # or on Windows: Tera Term → Setup → Serial port → 115200 8N1, local echo ON
   ```

3. Reset the board. The bootloader banner appears. Press **Enter** to see the `->` prompt.

4. Type `upload` and press **Enter**. The bootloader sends `C` characters to indicate it is ready for XMODEM-CRC.

5. In your terminal, start an XMODEM-CRC send of the (signed) binary:
   - **minicom:** `Ctrl-A S` → `xmodem` → select file
   - **lrzsz:** `sx --xmodem app_signed.bin > /dev/ttyUSB0 < /dev/ttyUSB0`
   - **Tera Term:** `File → Transfer → XMODEM → Send` → select file, choose CRC

6. Wait for the transfer to complete. If security is enabled, the bootloader prints the authentication result.

7. Type `jump` and press **Enter** to run the uploaded application.

---

## License

Copyright © 2026 Ali Chouchene. See `LICENSE` for details.
