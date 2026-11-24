# NEXUS PS — CPU core

The PlayStation uses a MIPS R3000A-compatible CPU. This directory contains
an early portable interpreter and CP0 foundation.

## Implemented so far

- 32 general-purpose registers with register zero hard-wired to zero.
- HI/LO registers and PC/next-PC state.
- MIPS instruction field decoder and reset-vector initialization.
- Bus interface for the future PS1 memory subsystem.
- 2 MiB main RAM device with little-endian byte/halfword/word access.
- RAM address decoding for the 8 MiB physical mirror window and KSEG0/KSEG1 aliases.
- RAM clear/reset support and separate-device rejection for scratchpad/BIOS addresses.
- Integer shifts, arithmetic/logical operations, comparisons, multiply/divide,
  jumps, immediate operations, and byte/halfword/word memory access.
- Conditional branches including BEQ, BNE, BLEZ, BGTZ, BLTZ, and BGEZ.
- Branch-delay sequencing for ordinary branches and jumps.
- R3000A one-instruction load delay using a pending-load writeback slot; loads
  (including LWL/LWR) become visible after the following instruction reads its operands.
- Little-endian LWL, LWR, SWL, and SWR word-merge instructions.
- CP0 register file with Status, Cause, EPC, and BadVaddr access.
- MFC0, MTC0, and RFE instruction handling.
- Initial exception entry for syscall, breakpoint, reserved instruction,
  arithmetic overflow, and misaligned instruction/data addresses.
- Initial CMake build target and unit tests for arithmetic, branch delay, load-delay hazards, and RAM mapping/access.
- GitHub Actions workflow to compile the CPU target and run its tests.

## Important accuracy limits

This is not a complete or cycle-accurate R3000A implementation yet.
Load-delay behavior, interrupt delivery and prioritization, all exception
corner cases, precise bus-error handling, coprocessor 2/GTE operations,
hardware timing, and some implementation-specific CPU behavior still need
work. Current cycle counts are rough estimates, not a hardware timing model.
The current unit tests are only a small initial test set.

## Next milestones

1. Expand load-delay tests across every load opcode, including LWL/LWR and load-to-zero.
2. Validate exception and branch-delay edge cases.
3. Add the memory bus, then connect RAM, BIOS, scratchpad, and memory-mapped I/O to the CPU.
4. Add interrupt/timer/DMA interactions and improve timing accuracy.
5. Profile on Android before considering a dynamic recompiler.

No BIOS or game images are included. This CPU foundation cannot boot games
until the rest of the PlayStation hardware is implemented.
