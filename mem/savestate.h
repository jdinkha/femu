#pragma once
#include <cstdint>
#include <string>

class Bus;

// Whole-machine savestates. A savestate captures the complete emulated state
// (CPU, PPU, APU, RAM, plus the cartridge's PRG-RAM / CHR-RAM / mapper
// registers) so gameplay can be restored bit-for-bit. It does NOT capture the
// ROM itself - a state only makes sense reloaded against the same game, which
// is enforced by a hash of the ROM's PRG+CHR stored in the file header.
//
// Files live next to the ROM with a ".state" extension, mirroring how
// battery-backed saves use ".sav".
namespace savestate {

// Path of the default ("quick") savestate for a ROM: same directory and stem,
// ".state" extension.
std::string DefaultPath(const std::string& rom_path);

// Cheap metadata read from a .state file's header without deserializing it.
struct Info {
    bool     valid = false;   // magic + format version recognised
    uint64_t rom_hash = 0;    // identifies the game this state belongs to
    int64_t  saved_unix = 0;  // wall-clock time the state was written
};
Info Peek(const std::string& path);

// Serialize the entire machine state of `bus` to `path` (written atomically
// via a temp file + rename). Returns "" on success or a short error message.
std::string Save(const Bus& bus, const std::string& path);

// Restore `bus` from the savestate at `path`. On any failure - missing file,
// bad magic, unsupported version, wrong game, or a truncated payload - `bus`
// is left exactly as it was and a short error message is returned.
std::string Load(Bus& bus, const std::string& path);

}
