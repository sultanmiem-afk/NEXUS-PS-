# NEXUS PS — PS1 CPU and memory foundation

The PlayStation uses a MIPS R3000A-compatible CPU. This directory contains
an early portable interpreter and the initial memory-device foundation.

## Implemented so far

- 32 general-purpose registers, HI/LO, PC state, and instruction decoding.
- Partial R3000A integer interpreter, branch-delay handling, and load-delay handling.
- Initial CP0 register and exception foundation.
- 2 MiB main RAM with the 8 MiB physical mirror window and KSEG aliases.
- Separate 1 KiB CPU scratchpad at physical address `0x1F800000`.
- Read-only BIOS ROM device with the 512 KiB address window at `0x1FC00000`.
  The user must supply a BIOS image; no BIOS image is included.
- 1 MiB VRAM storage (1024 × 512 × 16-bit) with pixel and rectangular transfer
  helpers. VRAM is intentionally separate from the CPU memory bus because the
  original console accesses it through the GPU command/transfer path.
- MemoryBus routes CPU reads/writes to RAM and scratchpad, and routes BIOS reads
  to the read-only BIOS device. Unimplemented I/O regions fail explicitly.
- CMake test targets for CPU, RAM, memory bus, BIOS, and VRAM foundations.

## Important accuracy limits

This is not a complete or cycle-accurate PlayStation emulator. The GPU command
processor and its GP0/GP1 registers, CD-ROM controller, DMA, timers, interrupts,
SPU/audio, controller ports, and full hardware timing are not implemented.
VRAM storage alone does not render graphics or execute GPU commands. The CPU
interpreter and memory map also need broader hardware validation.

## Next milestones

1. Implement GPU command ports and connect them to VRAM.
2. Add MMIO register decoding, DMA, timers, and interrupt controller.
3. Add CD-ROM image handling and SPU foundations.
4. Connect the BIOS boot path to the CPU and validate with legally obtained
   BIOS files and user-provided game images.
5. Expand automated tests and profile on Android.

No BIOS or game images are included.
