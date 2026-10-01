#pragma once
#include <cstdint>
#include <vector>
#include "serialize.h"

// A serial EEPROM on a two-wire (I2C-style) bus, as the Bandai LZ93D50
// boards use for saves. The CPU bit-bangs SCL and SDA; the EEPROM watches
// for start/stop conditions (SDA changing while SCL is high), samples bits
// on SCL rising edges and drives its own bits after SCL falls.
//   24C02  (256 bytes): device-select byte $A0/$A1, then a word address,
//          then data; 8-byte write pages.
//   X24C01 (128 bytes): no device select - the first byte is a 7-bit
//          address plus the R/W bit; 4-byte write pages.
// Bytes go MSB first on both. The memory itself is the cartridge's PRG-RAM,
// so it's battery-saved like any other save RAM.
class I2cEeprom {
public:
    enum class Type : uint8_t { C24C02, X24C01 };

    void Attach(std::vector<uint8_t>* memory, Type t) { mem = memory; type = t; }

    // The master's SCL and SDA levels after a CPU write.
    void Write(bool scl_new, bool sda_new) {
        if (scl && scl_new && sda != sda_new) {
            if (!sda_new) Start(); else state = IDLE; // SDA falls: start; rises: stop
        } else if (!scl && scl_new) {
            Rise(sda_new);
        } else if (scl && !scl_new) {
            Fall();
        }
        scl = scl_new;
        sda = sda_new;
    }

    // What the EEPROM is driving onto SDA (1 = released).
    bool Read() const { return out; }

    void Serialize(StateWriter& w) const {
        w.write(scl); w.write(sda); w.write(out); w.write(state); w.write(shift);
        w.write(bits); w.write(byte_index); w.write(address); w.write(reading); w.write(ack_driven);
    }
    void Deserialize(StateReader& r) {
        r.read(scl); r.read(sda); r.read(out); r.read(state); r.read(shift);
        r.read(bits); r.read(byte_index); r.read(address); r.read(reading); r.read(ack_driven);
    }

private:
    enum State : uint8_t { IDLE, RECEIVE, ACK, SEND, MASTER_ACK };

    std::vector<uint8_t>* mem = nullptr;
    Type type = Type::C24C02;
    bool scl = true, sda = true, out = true;
    State state = IDLE;
    uint8_t shift = 0, bits = 0, byte_index = 0, address = 0;
    bool reading = false, ack_driven = false;

    uint8_t& Cell(uint8_t a) { return (*mem)[a % mem->size()]; }

    void Start() {
        state = RECEIVE;
        bits = 0;
        shift = 0;
        byte_index = 0;
        out = true;
    }

    void Rise(bool level) {
        if (state == RECEIVE) {
            shift = (uint8_t)((shift << 1) | level);
            if (++bits == 8) Received();
        } else if (state == SEND) {
            bits++;
        } else if (state == MASTER_ACK) {
            if (level) { state = IDLE; return; } // master NAK: done reading
            address++;
            state = SEND; // next byte goes out from the coming falling edge
            bits = 0xFF;  // marks "load a fresh byte"
        }
    }

    void Fall() {
        if (state == ACK) {
            if (!ack_driven) { out = false; ack_driven = true; return; } // drive the ACK bit
            out = true;                                                  // ACK clocked: release
            ack_driven = false;
            if (reading) { state = SEND; bits = 0xFF; Fall(); return; }
            state = RECEIVE;
            bits = 0;
            shift = 0;
        } else if (state == SEND) {
            if (bits == 0xFF) {          // start a new byte
                shift = mem && !mem->empty() ? Cell(address) : 0xFF;
                bits = 0;
            }
            if (bits < 8) { out = (shift >> (7 - bits)) & 1; return; }
            out = true;                  // all 8 bits sent: let the master ACK
            state = MASTER_ACK;
        }
    }

    // A whole byte arrived from the master.
    void Received() {
        bool accept = true;
        if (type == Type::C24C02) {
            if (byte_index == 0) {
                accept = (shift & 0xF0) == 0xA0;
                reading = shift & 1;
            } else if (byte_index == 1) {
                address = shift;
            } else if (mem && !mem->empty()) {
                Cell(address) = shift;
                address = (uint8_t)((address & 0xF8) | ((address + 1) & 0x07));
            }
        } else {
            if (byte_index == 0) {
                address = shift >> 1;
                reading = shift & 1;
            } else if (mem && !mem->empty()) {
                Cell(address & 0x7F) = shift;
                address = (uint8_t)((address & 0x7C) | ((address + 1) & 0x03));
            }
        }
        byte_index++;
        if (!accept) { state = IDLE; return; }
        state = ACK;
        ack_driven = false;
    }
};
