#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <type_traits>

// Minimal binary serialization used by the savestate system (mem/savestate.*).
//
// Every piece of emulator state that needs saving is either a trivially-
// copyable scalar/struct or a fixed-size array of them, so this deliberately
// supports nothing more: no strings, no length prefixes, no versioning per
// field. The savestate file carries a single format version in its header;
// if a field layout ever changes, bump that version rather than trying to
// make this reader/writer self-describing.
struct StateWriter {
    std::vector<uint8_t> buf;

    template <typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value,
                      "StateWriter::write requires a trivially-copyable type");
        const auto* p = reinterpret_cast<const uint8_t*>(&value);
        buf.insert(buf.end(), p, p + sizeof(T));
    }

    void writeBytes(const void* data, size_t n) {
        const auto* p = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), p, p + n);
    }
};

// Bounds-checked counterpart. Once a read runs past the end of the buffer,
// `ok` latches false and every subsequent read is a no-op, so callers can
// deserialize a whole tree of objects and check `ok` just once at the end.
struct StateReader {
    const uint8_t* cur = nullptr;
    const uint8_t* end = nullptr;
    bool ok = true;

    StateReader() = default;
    StateReader(const uint8_t* data, size_t n) : cur(data), end(data + n) {}

    template <typename T>
    void read(T& value) {
        static_assert(std::is_trivially_copyable<T>::value,
                      "StateReader::read requires a trivially-copyable type");
        if (!ok || cur + sizeof(T) > end) { ok = false; return; }
        std::memcpy(&value, cur, sizeof(T));
        cur += sizeof(T);
    }

    void readBytes(void* dst, size_t n) {
        if (!ok || cur + n > end) { ok = false; return; }
        std::memcpy(dst, cur, n);
        cur += n;
    }
};
