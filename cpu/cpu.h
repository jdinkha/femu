#pragma once
#include <cstdint>
#include <array>
#include <string>

class Bus;
struct StateWriter;
struct StateReader;

// MOS 6502 (2A03) CPU core - official instruction set only.
// Illegal/undocumented opcodes are stubbed as XXX (no-op) for now;
// add them later once the official set passes nestest.
class CPU {
public:
    CPU();
    ~CPU() = default;

    void ConnectBus(Bus* b) { bus = b; }

    uint8_t cpuRead(uint16_t addr);
    void    cpuWrite(uint16_t addr, uint8_t data);

    // ---- Registers ----
    uint8_t  A  = 0x00;   // Accumulator
    uint8_t  X  = 0x00;   // Index X
    uint8_t  Y  = 0x00;   // Index Y
    uint8_t  SP = 0xFD;   // Stack pointer (stack lives at $0100-$01FF)
    uint16_t PC = 0x0000; // Program counter
    uint8_t  status = 0x00; // Processor status flags

    enum FLAGS6502 {
        C = (1 << 0), // Carry
        Z = (1 << 1), // Zero
        I = (1 << 2), // Interrupt disable
        D = (1 << 3), // Decimal (present but inert on the NES)
        B = (1 << 4), // Break (only meaningful in the byte pushed to stack)
        U = (1 << 5), // Unused, always reads as 1
        V = (1 << 6), // Overflow
        N = (1 << 7), // Negative
    };

    uint8_t GetFlag(FLAGS6502 f) const;
    void    SetFlag(FLAGS6502 f, bool v);

    // ---- External signals ----
    void clock();  // advance one clock cycle
    void reset();  // reset interrupt
    void irq();    // maskable interrupt request
    void nmi();    // non-maskable interrupt

    bool complete() const { return cycles == 0; }

    uint32_t GetClockCount() const { return clock_count; }
    void     SetClockCount(uint32_t v) { clock_count = v; } // for aligning trace CYC with a reference log

    // Savestate hooks. These persist the full architectural + mid-instruction
    // state (registers plus the addressing scratch the current opcode is
    // partway through), so a state saved between any two clocks resumes
    // exactly. The `lookup` table is rebuilt by the constructor and never
    // changes, so it isn't serialized.
    void SerializeState(StateWriter& w) const;
    void DeserializeState(StateReader& r);

    // Peek-only disassembly of the instruction at addr - never advances PC,
    // never calls fetch(), never writes anything. Used by the nestest
    // harness / debuggers. Text format loosely follows the Nintendulator
    // style used by nestest.log (operand plus resolved effective address
    // and memory value where relevant, e.g. "LDA $10,X @ 15 = 00").
    struct Disasm { std::string text; uint8_t length; };
    Disasm DisassembleAt(uint16_t addr);

private:
    Bus* bus = nullptr;

    uint8_t  fetched  = 0x00; // value the current opcode operates on
    uint16_t addr_abs = 0x0000; // resolved address for the current instruction
    uint16_t addr_rel = 0x0000; // signed relative offset, for branches
    uint8_t  opcode   = 0x00; // currently executing opcode byte
    uint8_t  cycles   = 0;    // cycles remaining for current instruction
    uint32_t clock_count = 0;
    bool     acc_mode = false; // true if current addressing mode is Accumulator

    uint8_t fetch();

    // ---- Addressing modes ----
    uint8_t IMP(); uint8_t ACC(); uint8_t IMM();
    uint8_t ZP0(); uint8_t ZPX(); uint8_t ZPY();
    uint8_t REL();
    uint8_t ABS(); uint8_t ABX(); uint8_t ABY();
    uint8_t IND(); uint8_t IZX(); uint8_t IZY();

    // ---- Official opcodes (56 mnemonics / 151 opcodes) ----
    uint8_t ADC(); uint8_t AND(); uint8_t ASL(); uint8_t BCC(); uint8_t BCS();
    uint8_t BEQ(); uint8_t BIT(); uint8_t BMI(); uint8_t BNE(); uint8_t BPL();
    uint8_t BRK(); uint8_t BVC(); uint8_t BVS(); uint8_t CLC(); uint8_t CLD();
    uint8_t CLI(); uint8_t CLV(); uint8_t CMP(); uint8_t CPX(); uint8_t CPY();
    uint8_t DEC(); uint8_t DEX(); uint8_t DEY(); uint8_t EOR(); uint8_t INC();
    uint8_t INX(); uint8_t INY(); uint8_t JMP(); uint8_t JSR(); uint8_t LDA();
    uint8_t LDX(); uint8_t LDY(); uint8_t LSR(); uint8_t NOP(); uint8_t ORA();
    uint8_t PHA(); uint8_t PHP(); uint8_t PLA(); uint8_t PLP(); uint8_t ROL();
    uint8_t ROR(); uint8_t RTI(); uint8_t RTS(); uint8_t SBC(); uint8_t SEC();
    uint8_t SED(); uint8_t SEI(); uint8_t STA(); uint8_t STX(); uint8_t STY();
    uint8_t TAX(); uint8_t TAY(); uint8_t TSX(); uint8_t TXA(); uint8_t TXS();
    uint8_t TYA();

    // Catch-all placeholder for opcodes with no defined behavior (JAM/KIL -
    // these freeze real hardware; we just no-op rather than hanging)
    uint8_t XXX();

    // ---- Illegal/undocumented opcodes ----
    // Deterministic combo instructions (well-documented, used by real games
    // and by nestest's extended opcode tests):
    uint8_t LAX(); uint8_t SAX(); uint8_t DCP(); uint8_t ISC();
    uint8_t SLO(); uint8_t RLA(); uint8_t SRE(); uint8_t RRA();
    uint8_t ANC(); uint8_t ALR(); uint8_t ARR(); uint8_t AXS();
    // Genuinely unstable on real silicon (depend on internal bus-conflict
    // behavior that varies by chip revision). Implemented as the common
    // approximation most emulators use; don't expect a bit-exact match
    // against every reference trace.
    uint8_t LXA(); uint8_t XAA(); uint8_t SHA(); uint8_t TAS();
    uint8_t SHY(); uint8_t SHX(); uint8_t LAS();

    struct INSTRUCTION {
        std::string name;
        uint8_t (CPU::*operate)(void)  = nullptr;
        uint8_t (CPU::*addrmode)(void) = nullptr;
        uint8_t cycles = 0;
    };

    std::array<INSTRUCTION, 256> lookup;
};
