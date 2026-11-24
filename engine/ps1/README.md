# NEXUS PS — CPU core

The PlayStation uses the MIPS R3000A-compatible CPU. This directory starts the
CPU implementation with its architectural state and instruction-field decoder.

## Current foundation

- 32 general-purpose registers (register zero is fixed to zero by the core).
- HI and LO special registers.
- Program counter and next-PC state for later branch-delay-slot handling.
- Helpers for decoding the fields of a 32-bit MIPS instruction.
- Reset state initialized to the PS1 BIOS reset vector.

## Next implementation steps

1. Implement the R3000A integer instruction set and branch-delay behavior.
2. Add load-delay behavior and precise CPU exceptions.
3. Add a memory bus for RAM, scratchpad, BIOS, and memory-mapped devices.
4. Build instruction-level tests before connecting the CPU to the full system.

This is an early CPU foundation, not yet a playable emulator. BIOS and game
images are not included.
