# NEXUS PS — CPU core

The PlayStation uses a MIPS R3000A-compatible CPU. This directory contains
an expanding portable interpreter foundation.

## Implemented foundation

- 32 GPRs with hard-wired register zero, HI/LO, PC, and next-PC state.
- Instruction-field decoder and reset vector initialization.
- Bus abstraction so the machine layer can provide RAM, BIOS, scratchpad,
  address mirrors, and memory-mapped I/O.
- Integer shifts, register arithmetic/logical operations, comparisons,
  multiply/divide, jumps, branches, immediate operations, and basic byte,
  halfword, and word loads/stores.
- Branch delay-slot sequencing for normal branch and jump flow.
- Alignment and bus-fault reporting.
- No BIOS or game images are included.

## Accuracy and performance roadmap

1. Add CP0 registers, exception vectors, EPC/Cause/Status behavior, and
   precise delay-slot exception handling.
2. Implement R3000A load-delay behavior and validate all instruction edge cases.
3. Add instruction/cycle timing appropriate to the PS1 and hardware stalls.
4. Add automated CPU tests and compare behavior against trusted test suites.
5. Profile on Android before choosing interpreter optimizations or a dynarec.

This is not yet a complete or cycle-accurate CPU, and it cannot boot games
until the memory bus, GPU, DMA, timers, interrupts, BIOS loading, and other
machine components are implemented.
