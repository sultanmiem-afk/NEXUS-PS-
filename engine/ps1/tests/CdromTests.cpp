#include "Cdrom.hpp"
#include "MemoryBus.hpp"

#include <cassert>
#include <cstdint>
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

    return 0;
}
