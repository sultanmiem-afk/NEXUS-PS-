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
  - GP0(02h) VRAM fill rectangle.\n  - GP0(20h) flat-shaded triangle and GP0(28h) flat-shaded quad rasterization.\n  - Basic signed vertex coordinates, draw offset, and drawing-area clipping.
  - GP0(A0h) CPU-to-VRAM image transfer.
  - GP0(C0h) VRAM-to-CPU image readback.
  - GP0(E1h–E6h) draw-environment register storage.
  - GP1 reset, command-buffer reset, IRQ acknowledge, display enable,
    DMA direction, display start, display ranges, and display mode registers.
- CMake tests for CPU, RAM, memory bus, BIOS/VRAM, and initial GPU operations.

## GPU limitations

The GPU is only an early command-processing subset. Textured primitives, blending, dithering,
texture-window behavior, mask semantics, display scanout, GPU timing/IRQ generation,
and DMA integration are not implemented. The current flat polygon rasterizer is
an initial pixel-level implementation and still needs edge-rule/hardware validation. GP0 command FIFO behavior and status
bits still need hardware-level validation. Passing the current tests does not
mean commercial games are compatible.

## Next milestones

1. Implement untextured triangle/quad and line rasterization.
2. Implement drawing-area clipping, mask behavior, and blending.
3. Add GPU MMIO ports and DMA integration.
4. Add display scanout and connect the output to the Android renderer.
5. Continue CPU timing, CD-ROM, SPU, timers, and interrupt-controller work.

No BIOS or game images are included.


### Additional GPU rasterization

The current experimental GPU subset also accepts GP0(40h) flat-line packets and rasterizes them into VRAM with drawing-area clipping and drawing offsets. This remains a small command subset: it is not yet cycle-accurate, does not implement Gouraud shading or textured polygons, and has no GPU DMA/display scanout integration.


### Gouraud shading milestone

Added experimental GP0(30h) Gouraud-shaded triangle and GP0(38h) Gouraud-shaded quad support. Vertex RGB colors are interpolated across covered pixels and converted to the PS1 15-bit VRAM format. This is an initial software rasterizer; exact console edge rules, color precision, and hardware timing still require validation.


### Initial textured polygon support

Added experimental GP0(24h) textured triangle and GP0(2Ch) textured quad handling for 15-bit direct-color texture pages, including affine UV interpolation, texture-window coordinate masking, transparent texel skip, and draw-area clipping. Indexed 4-bit/8-bit CLUT texture modes, modulation, semi-transparency, dithering, and exact hardware rasterization rules remain unsupported and need dedicated tests.


### Indexed texture/CLUT support (experimental)

Extended textured triangle/quad sampling to indexed 4-bit and 8-bit texture pages. The sampler extracts packed indices from VRAM words and resolves them through the packet CLUT coordinates; zero texels remain transparent. Added focused 4-bit and 8-bit CLUT tests. Perspective correction, texture modulation, semi-transparency, dithering, exact hardware edge rules, and timing remain unimplemented.


### Texture modulation and raw texture mode (experimental)

Added textured triangle/quad opcode variants GP0(24h/25h) and GP0(2Ch/2Dh). Modulated variants multiply sampled 5-bit RGB texture channels by the packet vertex RGB color using a neutral 128 scale; raw variants bypass modulation. CLUT test fixtures were corrected to seed the actual page/CLUT coordinates used by their packets. Semi-transparency, dithering, exact fixed-point behavior, and hardware rasterization rules remain incomplete.
