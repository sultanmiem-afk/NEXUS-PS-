# NEXUS PS — CPU core

The PlayStation uses a MIPS R3000A-compatible CPU. This directory contains
an early portable interpreter and CP0 foundation.

## Implemented so far

- 32 general-purpose registers with register zero hard-wired to zero.
- HI/LO registers and PC/next-PC state.
- MIPS instruction field decoder.
- Bus interface for the future memory subsystem.
- Initial integer shifts, arithmetic/logical operations, comparisons,
  multiply/divide, branches, jumps, immediates, and basic byte/halfword/word
  loads and stores.
- Branch-delay sequencing for ordinary branches and jumps.
- CP0 register file with Status, Cause, EPC, and BadVaddr access.
- MFC0, MTC0, and RFE instruction handling.
- Initial exception entry for syscall, breakpoint, reserved instruction,
  arithmetic overflow, and misaligned memory accesses.
- Exception mode-stack handling and BEV-dependent exception vector selection.

## Important accuracy limits

This is not a complete or cycle-accurate R3000A implementation yet.
Load-delay behavior, interrupt delivery, all exception corner cases,
unaligned merge instructions (LWL/LWR/SWL/SWR), coprocessor 2/GTE operations,
precise bus-error handling, and hardware timing still need implementation.
The current cycle counts are rough estimates, not a hardware timing model.

## Next milestones

1. Implement load-delay behavior and remaining base CPU instructions.
2. Add automated instruction and exception tests, then compile with the
   Android NDK toolchain.
3. Connect the CPU to PS1 RAM, BIOS, scratchpad, and memory-mapped I/O.
4. Add interrupt/timer/DMA interactions and improve timing accuracy.
5. Profile on Android before considering a dynamic recompiler.

No BIOS or game images are included. This CPU foundation cannot boot games
until the rest of the PlayStation hardware is implemented.
