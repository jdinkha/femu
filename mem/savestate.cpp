#include "savestate.h"
#include "bus.h"
#include "serialize.h"

#include <cstring>
#include <ctime>
#include <fstream>
#include <iterator>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr char     MAGIC[8]  = { 'F', 'E', 'M', 'U', 'S', 'A', 'V', 'E' };
constexpr uint32_t FORMAT_VERSION = 1;

// Fixed-size header prepended to every .state file. Kept 8-byte aligned so the
// serialized payload that follows starts on a clean boundary.
struct Header {
    char     magic[8];
    uint32_t version;
    uint32_t reserved;    // unused; reserved for future flags
    uint64_t rom_hash;
    int64_t  saved_unix;
};

} // namespace

std::string savestate::DefaultPath(const std::string& rom_path) {
    return fs::path(rom_path).replace_extension(".state").string();
}

savestate::Info savestate::Peek(const std::string& path) {
    Info info;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return info;

    Header h{};
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!f || std::memcmp(h.magic, MAGIC, sizeof(MAGIC)) != 0 || h.version != FORMAT_VERSION)
        return info;

    info.valid = true;
    info.rom_hash = h.rom_hash;
    info.saved_unix = h.saved_unix;
    return info;
}

std::string savestate::Save(const Bus& bus, const std::string& path) {
    if (!bus.HasCartridge()) return "no game loaded";

    StateWriter w;
    bus.SerializeState(w);

    Header h{};
    std::memcpy(h.magic, MAGIC, sizeof(MAGIC));
    h.version = FORMAT_VERSION;
    h.reserved = 0;
    h.rom_hash = bus.RomHash();
    h.saved_unix = static_cast<int64_t>(std::time(nullptr));

    // Write to a sibling temp file, then rename over the target so a crash
    // mid-write can never corrupt an existing good savestate.
    std::error_code ec;
    fs::path tmp = fs::path(path);
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) return "could not open savestate file for writing";
        f.write(reinterpret_cast<const char*>(&h), sizeof(h));
        f.write(reinterpret_cast<const char*>(w.buf.data()),
                static_cast<std::streamsize>(w.buf.size()));
        if (!f) {
            f.close();
            fs::remove(tmp, ec);
            return "failed while writing savestate";
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return "could not finalize savestate file";
    }
    return "";
}

std::string savestate::Load(Bus& bus, const std::string& path) {
    if (!bus.HasCartridge()) return "no game running";

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "could not open savestate file";
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    if (data.size() < sizeof(Header)) return "file is not a savestate";

    Header h{};
    std::memcpy(&h, data.data(), sizeof(h));
    if (std::memcmp(h.magic, MAGIC, sizeof(MAGIC)) != 0) return "file is not a femu savestate";
    if (h.version != FORMAT_VERSION) return "savestate was made by a different femu version";
    if (h.rom_hash != bus.RomHash()) return "savestate was made for a different game";

    // Snapshot the live machine first: if the payload turns out to be
    // truncated or corrupt we can roll straight back, so a failed load never
    // leaves the emulator in a half-updated state.
    StateWriter backup;
    bus.SerializeState(backup);

    StateReader r(data.data() + sizeof(Header), data.size() - sizeof(Header));
    bus.DeserializeState(r);
    if (!r.ok) {
        StateReader restore(backup.buf.data(), backup.buf.size());
        bus.DeserializeState(restore);
        return "savestate is corrupt";
    }
    return "";
}
