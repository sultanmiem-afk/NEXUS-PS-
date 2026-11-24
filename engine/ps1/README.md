# NEXUS PS — PS1 CPU, memory, and GPU foundation

This repository contains an early portable PlayStation emulation foundation.
It is not yet a complete emulator and cannot boot commercial games at this stage.

## Implemented foundations

- Partial MIPS R3000A CPU interpreter with branch/load-delay foundations.
- Initial CP0 registers and exception handling.
- 2 MiB main RAM with physical mirrors and KSEG aliases.
- Separate 1 KiB scratchpad.
- Read-only 512 KiB BIOS ROM device; users supply their own BIOS image.
- 1 MiB VRAM (1024 × 512, 16-bit pixels).
- Initial GP0/GP1 GPU command processor:
  - GP0(02h) VRAM fill rectangle.
  - GP0(A0h) CPU-to-VRAM image transfer.
  - GP0(C0h) VRAM-to-CPU image readback.
  - GP0(E1h–E6h) draw-environment register storage.
  - GP1 reset, command-buffer reset, IRQ acknowledge, display enable,
    DMA direction, display start, display ranges, and display mode registers.
- CMake tests for CPU, RAM, memory bus, BIOS/VRAM, and initial GPU operations.

## GPU limitations

The GPU is only an early command-processing subset. Polygon and line rasterizers,
textured and shaded primitives, blending, dithering, texture-window behavior,
drawing-area clipping, mask semantics, display scanout, GPU timing/IRQ generation,
and DMA integration are not implemented. GP0 command FIFO behavior and status
bits still need hardware-level validation. Passing the current tests does not
mean commercial games are compatible.

## Next milestones

1. Implement untextured triangle/quad and line rasterization.
2. Implement drawing-area clipping, mask behavior, and blending.
3. Add GPU MMIO ports and DMA integration.
4. Add display scanout and connect the output to the Android renderer.
5. Continue CPU timing, CD-ROM, SPU, timers, and interrupt-controller work.

No BIOS or game images are included.
