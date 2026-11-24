#include "Cdrom.hpp"
#include "MemoryBus.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace nexus::ps1;

int main() {
    MemoryBus bus;
    Cdrom& cdrom = bus.cdrom();
    std::vector<std::uint8_t> image(2u * 2048u);
    for (std::size_t i = 0; i < 2048u; ++i) {
        image[i] = static_cast<std::uint8_t>(i & 0xFFu);
        image[2048u + i] = static_cast<std::uint8_t>((i + 0x40u) & 0xFFu);
    }

    assert(cdrom.loadImage(image));
    assert(cdrom.imageLoaded());
    assert(cdrom.sectorCount() == 2u);
    assert(!cdrom.loadImage(std::vector<std::uint8_t>(17u, 0u)));

    // CD-ROM byte registers are mapped through the CPU-visible memory bus.
    assert(bus.write8(0x1F801800u, 1u));
    assert(bus.write8(0x1F801802u, 0x1Fu)); // enable all five CD-ROM IRQ classes
    assert(bus.write8(0x1F801800u, 0u));

    // Setloc 00:02:00 BCD maps to LBA 0 after the 150-sector lead-in.
    assert(bus.write8(0x1F801802u, 0x00u));
    assert(bus.write8(0x1F801802u, 0x02u));
    assert(bus.write8(0x1F801802u, 0x00u));
    assert(bus.write8(0x1F801801u, 0x02u));
    assert(cdrom.currentLba() == 0u);

    // Clear the Setloc command-complete interrupt, then start sector reading.
    assert(bus.write8(0x1F801800u, 1u));
    assert(bus.write8(0x1F801803u, 0x1Fu));
    assert(bus.write8(0x1F801800u, 0u));
    assert(bus.write8(0x1F801801u, 0x06u));
    assert(cdrom.reading());
    assert(cdrom.irqPending());

    // One approximate sector interval produces 2048 user-data bytes.
    cdrom.advance(Cdrom::kCyclesPerSector - 1u);
    std::uint8_t byte = 0xFFu;
    assert(bus.read8(0x1F801800u, byte));
    assert((byte & 0x08u) == 0u);
    cdrom.advance(1u);
    assert((cdrom.interruptFlags() & 0x02u) != 0u);
    assert(cdrom.irqPending());

    for (std::size_t i = 0; i < 2048u; ++i) {
        assert(bus.read8(0x1F801802u, byte));
        assert(byte == static_cast<std::uint8_t>(i & 0xFFu));
    }
    assert(bus.read8(0x1F801800u, byte));
    assert((byte & 0x08u) == 0u);

    // The next sector is delivered only after another sector interval.
    cdrom.advance(Cdrom::kCyclesPerSector);
    assert(bus.read8(0x1F801802u, byte));
    assert(byte == 0x40u);

    // Pause stops further sector production.
    assert(bus.write8(0x1F801801u, 0x09u));
    assert(!cdrom.reading());
    cdrom.advance(Cdrom::kCyclesPerSector * 2u);
    assert(bus.write8(0x1F801800u, 1u));
    assert(bus.write8(0x1F801803u, 0x1Fu)); // acknowledge pending interrupts
    assert(!cdrom.irqPending());

    // Raw 2352-byte images expose the 2048-byte user-data area at offset 24.
    std::vector<std::uint8_t> raw(2352u, 0xEEu);
    for (std::size_t i = 24u; i < 24u + 2048u; ++i)
        raw[i] = 0x5Au;
    Cdrom rawCdrom;
    assert(rawCdrom.loadImage(raw));
    assert(rawCdrom.sectorCount() == 1u);
    rawCdrom.write8(0x1F801800u, 0u);
    rawCdrom.write8(0x1F801802u, 0x00u);
    rawCdrom.write8(0x1F801802u, 0x02u);
    rawCdrom.write8(0x1F801802u, 0x00u);
    rawCdrom.write8(0x1F801801u, 0x02u);
    rawCdrom.write8(0x1F801801u, 0x06u);
    rawCdrom.advance(Cdrom::kCyclesPerSector);
    assert(rawCdrom.read8(0x1F801802u, byte));
    assert(byte == 0x5Au);

    // CUE/BIN binding: two MODE1/2352 tracks in one BIN file.
    std::vector<std::uint8_t> cueBin(154u * 2352u, 0u);
    std::fill(cueBin.begin() + 150u * 2352u + 16u,
              cueBin.begin() + 150u * 2352u + 16u + 2048u, 0xA1u);
    std::fill(cueBin.begin() + 152u * 2352u + 16u,
              cueBin.begin() + 152u * 2352u + 16u + 2048u, 0xB2u);
    const std::string cue =
        "FILE \"disc.bin\" BINARY\n"
        "  TRACK 01 MODE1/2352\n"
        "    INDEX 01 00:02:00\n"
        "  TRACK 02 MODE1/2352\n"
        "    INDEX 01 00:02:02\n";
    Cdrom cueCdrom;
    assert(cueCdrom.loadCue(cue, "disc.bin", cueBin));
    assert(cueCdrom.cueLoaded());
    assert(cueCdrom.trackCount() == 2u);
    assert(cueCdrom.sectorCount() == 4u);
    assert(!cueCdrom.loadCue(cue, "wrong-name.bin", cueBin));

    // First CUE track begins at disc LBA 0 and uses MODE1's 16-byte header.
    cueCdrom.write8(0x1F801800u, 0u);
    cueCdrom.write8(0x1F801802u, 0x00u);
    cueCdrom.write8(0x1F801802u, 0x02u);
    cueCdrom.write8(0x1F801802u, 0x00u);
    cueCdrom.write8(0x1F801801u, 0x02u); // Setloc
    cueCdrom.write8(0x1F801801u, 0x06u); // ReadN
    cueCdrom.advance(Cdrom::kCyclesPerSector);
    assert(cueCdrom.read8(0x1F801802u, byte) && byte == 0xA1u);

    // Finish sector 0, then verify sector 1 still belongs to track 1.
    for (std::size_t i = 1; i < 2048u; ++i)
        cueCdrom.read8(0x1F801802u, byte);
    cueCdrom.advance(Cdrom::kCyclesPerSector);
    assert(cueCdrom.read8(0x1F801802u, byte) && byte == 0x00u);
    for (std::size_t i = 1; i < 2048u; ++i)
        cueCdrom.read8(0x1F801802u, byte);

    // Absolute disc LBA 2 maps to file sector 152, the second CUE track.
    cueCdrom.advance(Cdrom::kCyclesPerSector);
    assert(cueCdrom.read8(0x1F801802u, byte) && byte == 0xB2u);

    // Reject unsupported multi-BIN CUE sheets rather than silently mis-mapping.
    const std::string multiFileCue =
        "FILE \"one.bin\" BINARY\n"
        " TRACK 01 MODE1/2048\n"
        " INDEX 01 00:02:00\n"
        "FILE \"two.bin\" BINARY\n"
        " TRACK 02 MODE1/2048\n"
        " INDEX 01 00:00:00\n";
    assert(!cueCdrom.loadCue(multiFileCue, "one.bin", image));

    // Expanded command set: GetTN, GetTD, Setfilter/Getparam, GetlocP/GetlocL.
    Cdrom commandCdrom;
    assert(commandCdrom.loadCue(cue, "disc.bin", cueBin));
    commandCdrom.write8(0x1F801801u, 0x13u); // GetTN
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0x01u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0x02u);

    commandCdrom.write8(0x1F801802u, 0x02u);
    commandCdrom.write8(0x1F801801u, 0x14u); // GetTD track 2
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0x02u);

    commandCdrom.write8(0x1F801802u, 0x03u);
    commandCdrom.write8(0x1F801802u, 0x04u);
    commandCdrom.write8(0x1F801801u, 0x0Du); // Setfilter
    assert(commandCdrom.read8(0x1F801801u, byte)); // discard Setfilter response
    commandCdrom.write8(0x1F801801u, 0x0Fu); // Getparam
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0x03u);
    assert(commandCdrom.read8(0x1F801801u, byte) && byte == 0x04u);

    commandCdrom.write8(0x1F801802u, 0x00u);
    commandCdrom.write8(0x1F801802u, 0x02u);
    commandCdrom.write8(0x1F801802u, 0x02u);
    commandCdrom.write8(0x1F801801u, 0x02u); // Setloc to disc LBA 2.
    assert(commandCdrom.read8(0x1F801801u, byte)); // Setloc response
    commandCdrom.write8(0x1F801801u, 0x11u); // GetlocP
    const std::uint8_t expectedLocP[] = {0x02u, 0x01u, 0u, 0u, 0u, 0u, 0x02u, 0x02u};
    for (const std::uint8_t expected : expectedLocP) {
        assert(commandCdrom.read8(0x1F801801u, byte));
        assert(byte == expected);
    }
    commandCdrom.write8(0x1F801801u, 0x10u); // GetlocL: eight header bytes
    const std::uint8_t expectedLocL[] = {0u, 0x02u, 0x02u, 0x01u, 0u, 0u, 0u, 0u};
    for (const std::uint8_t expected : expectedLocL) {
        assert(commandCdrom.read8(0x1F801801u, byte));
        assert(byte == expected);
    }

    // Setmode bit 7 selects the approximate double-speed sector interval.
    Cdrom fastCdrom;
    assert(fastCdrom.loadImage(image));
    fastCdrom.write8(0x1F801802u, 0x80u);
    fastCdrom.write8(0x1F801801u, 0x0Eu);
    fastCdrom.read8(0x1F801801u, byte); // discard Setmode response
    fastCdrom.write8(0x1F801802u, 0x00u);
    fastCdrom.write8(0x1F801802u, 0x02u);
    fastCdrom.write8(0x1F801802u, 0x00u);
    fastCdrom.write8(0x1F801801u, 0x02u);
    fastCdrom.read8(0x1F801801u, byte); // discard Setloc response
    fastCdrom.write8(0x1F801801u, 0x06u);
    fastCdrom.read8(0x1F801801u, byte); // discard ReadN response
    fastCdrom.advance(Cdrom::kCyclesPerSector / 2u);
    assert(fastCdrom.dataAvailableBytes() == 2048u);

    // MODE2/2352 uses a 24-byte header before its 2048-byte user-data area.
    std::vector<std::uint8_t> mode2Bin(151u * 2352u, 0u);
    std::fill(mode2Bin.begin() + 150u * 2352u + 24u,
              mode2Bin.begin() + 150u * 2352u + 24u + 2048u, 0x7Cu);
    const std::string mode2Cue =
        "FILE \"mode2.bin\" BINARY\n"
        " TRACK 01 MODE2/2352\n"
        " INDEX 01 00:02:00\n";
    Cdrom mode2Cdrom;
    assert(mode2Cdrom.loadCue(mode2Cue, "mode2.bin", mode2Bin));
    mode2Cdrom.write8(0x1F801802u, 0x00u);
    mode2Cdrom.write8(0x1F801802u, 0x02u);
    mode2Cdrom.write8(0x1F801802u, 0x00u);
    mode2Cdrom.write8(0x1F801801u, 0x02u);
    mode2Cdrom.read8(0x1F801801u, byte);
    mode2Cdrom.write8(0x1F801801u, 0x06u);
    mode2Cdrom.read8(0x1F801801u, byte);
    mode2Cdrom.advance(Cdrom::kCyclesPerSector);
    assert(mode2Cdrom.read8(0x1F801802u, byte) && byte == 0x7Cu);

    return 0;
}
