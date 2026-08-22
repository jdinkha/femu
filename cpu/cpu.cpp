#include "cpu.h"
#include "../mem/Bus.h"
#include <cstdio>

uint8_t CPU::cpuRead(uint16_t addr)              { return bus->cpuRead(addr); }
void    CPU::cpuWrite(uint16_t addr, uint8_t d)  { bus->cpuWrite(addr, d); }

uint8_t CPU::GetFlag(FLAGS6502 f) const { return (status & f) ? 1 : 0; }
void    CPU::SetFlag(FLAGS6502 f, bool v) {
    if (v) status |= f;
    else   status &= ~f;
}

uint8_t CPU::fetch() {
    if (!acc_mode)
        fetched = cpuRead(addr_abs);
    else
        fetched = A;
    return fetched;
}

// ============================= Addressing Modes =============================

uint8_t CPU::IMP() { acc_mode = false; fetched = A; return 0; }
uint8_t CPU::ACC() { acc_mode = true;  fetched = A; return 0; }

uint8_t CPU::IMM() { acc_mode = false; addr_abs = PC++; return 0; }

uint8_t CPU::ZP0() {
    acc_mode = false;
    addr_abs = cpuRead(PC); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::ZPX() {
    acc_mode = false;
    addr_abs = (cpuRead(PC) + X); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::ZPY() {
    acc_mode = false;
    addr_abs = (cpuRead(PC) + Y); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::REL() {
    acc_mode = false;
    addr_rel = cpuRead(PC); PC++;
    if (addr_rel & 0x80) addr_rel |= 0xFF00;
    return 0;
}

uint8_t CPU::ABS() {
    acc_mode = false;
    uint16_t lo = cpuRead(PC); PC++;
    uint16_t hi = cpuRead(PC); PC++;
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::ABX() {
    acc_mode = false;
    uint16_t lo = cpuRead(PC); PC++;
    uint16_t hi = cpuRead(PC); PC++;
    addr_abs = ((hi << 8) | lo) + X;
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

uint8_t CPU::ABY() {
    acc_mode = false;
    uint16_t lo = cpuRead(PC); PC++;
    uint16_t hi = cpuRead(PC); PC++;
    addr_abs = ((hi << 8) | lo) + Y;
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

uint8_t CPU::IND() {
    acc_mode = false;
    uint16_t ptr_lo = cpuRead(PC); PC++;
    uint16_t ptr_hi = cpuRead(PC); PC++;
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    if (ptr_lo == 0x00FF)
        addr_abs = (cpuRead(ptr & 0xFF00) << 8) | cpuRead(ptr);
    else
        addr_abs = (cpuRead(ptr + 1) << 8) | cpuRead(ptr);
    return 0;
}

uint8_t CPU::IZX() {
    acc_mode = false;
    uint16_t t = cpuRead(PC); PC++;
    uint16_t lo = cpuRead((uint16_t)(t + X) & 0x00FF);
    uint16_t hi = cpuRead((uint16_t)(t + X + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::IZY() {
    acc_mode = false;
    uint16_t t = cpuRead(PC); PC++;
    uint16_t lo = cpuRead(t & 0x00FF);
    uint16_t hi = cpuRead((t + 1) & 0x00FF);
    addr_abs = ((hi << 8) | lo) + Y;
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

// ================================= Opcodes ==================================

uint8_t CPU::ADC() {
    fetch();
    uint16_t temp = (uint16_t)A + (uint16_t)fetched + (uint16_t)GetFlag(C);
    SetFlag(C, temp > 255);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(V, (~((uint16_t)A ^ (uint16_t)fetched) & ((uint16_t)A ^ temp)) & 0x0080);
    SetFlag(N, temp & 0x0080);
    A = temp & 0x00FF;
    return 1;
}

uint8_t CPU::AND() {
    fetch();
    A = A & fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

uint8_t CPU::ASL() {
    fetch();
    uint16_t temp = (uint16_t)fetched << 1;
    SetFlag(C, (temp & 0xFF00) > 0);
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          cpuWrite(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::BCC() {
    if (GetFlag(C) == 0) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BCS() {
    if (GetFlag(C) == 1) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BEQ() {
    if (GetFlag(Z) == 1) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BIT() {
    fetch();
    uint16_t temp = A & fetched;
    SetFlag(Z, (temp & 0x00FF) == 0x00);
    SetFlag(N, fetched & (1 << 7));
    SetFlag(V, fetched & (1 << 6));
    return 0;
}

uint8_t CPU::BMI() {
    if (GetFlag(N) == 1) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BNE() {
    if (GetFlag(Z) == 0) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BPL() {
    if (GetFlag(N) == 0) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BRK() {
    PC++;
    SetFlag(I, 1);

    cpuWrite(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    cpuWrite(0x0100 + SP, PC & 0x00FF);        SP--;

    SetFlag(B, 1);
    cpuWrite(0x0100 + SP, status);
    SetFlag(B, 0);
    SP--;

    PC = (uint16_t)cpuRead(0xFFFE) | ((uint16_t)cpuRead(0xFFFF) << 8);
    return 0;
}

uint8_t CPU::BVC() {
    if (GetFlag(V) == 0) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BVS() {
    if (GetFlag(V) == 1) {
        cycles++;
        addr_abs = PC + addr_rel;
        if ((addr_abs & 0xFF00) != (PC & 0xFF00)) cycles++;
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::CLC() { SetFlag(C, false); return 0; }
uint8_t CPU::CLD() { SetFlag(D, false); return 0; }
uint8_t CPU::CLI() { SetFlag(I, false); return 0; }
uint8_t CPU::CLV() { SetFlag(V, false); return 0; }

uint8_t CPU::CMP() {
    fetch();
    uint16_t temp = (uint16_t)A - (uint16_t)fetched;
    SetFlag(C, A >= fetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 1;
}

uint8_t CPU::CPX() {
    fetch();
    uint16_t temp = (uint16_t)X - (uint16_t)fetched;
    SetFlag(C, X >= fetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::CPY() {
    fetch();
    uint16_t temp = (uint16_t)Y - (uint16_t)fetched;
    SetFlag(C, Y >= fetched);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::DEC() {
    fetch();
    uint16_t temp = fetched - 1;
    cpuWrite(addr_abs, temp & 0x00FF);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::DEX() { X--; SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::DEY() { Y--; SetFlag(Z, Y == 0x00); SetFlag(N, Y & 0x80); return 0; }

uint8_t CPU::EOR() {
    fetch();
    A = A ^ fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

uint8_t CPU::INC() {
    fetch();
    uint16_t temp = fetched + 1;
    cpuWrite(addr_abs, temp & 0x00FF);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::INX() { X++; SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::INY() { Y++; SetFlag(Z, Y == 0x00); SetFlag(N, Y & 0x80); return 0; }

uint8_t CPU::JMP() { PC = addr_abs; return 0; }

uint8_t CPU::JSR() {
    PC--;
    cpuWrite(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    cpuWrite(0x0100 + SP, PC & 0x00FF);        SP--;
    PC = addr_abs;
    return 0;
}

uint8_t CPU::LDA() {
    fetch();
    A = fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

uint8_t CPU::LDX() {
    fetch();
    X = fetched;
    SetFlag(Z, X == 0x00);
    SetFlag(N, X & 0x80);
    return 1;
}

uint8_t CPU::LDY() {
    fetch();
    Y = fetched;
    SetFlag(Z, Y == 0x00);
    SetFlag(N, Y & 0x80);
    return 1;
}

uint8_t CPU::LSR() {
    fetch();
    SetFlag(C, fetched & 0x0001);
    uint16_t temp = fetched >> 1;
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          cpuWrite(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::NOP() { return 1; } // 1 lets abs,X illegal NOPs pick up the addressing mode's page-cross cycle; official NOP (IMP) is unaffected since IMP always returns 0

uint8_t CPU::ORA() {
    fetch();
    A = A | fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

uint8_t CPU::PHA() {
    cpuWrite(0x0100 + SP, A);
    SP--;
    return 0;
}

uint8_t CPU::PHP() {
    // B and U are forced to 1 in the pushed byte, but that's purely a
    // property of the byte on the stack - the live status register's own
    // B/U bits are untouched.
    cpuWrite(0x0100 + SP, status | B | U);
    SP--;
    return 0;
}

uint8_t CPU::PLA() {
    SP++;
    A = cpuRead(0x0100 + SP);
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::PLP() {
    SP++;
    uint8_t pulled = cpuRead(0x0100 + SP);
    // Real hardware ignores bits 4 (B) and 5 (U) of the pulled byte - the
    // live status register keeps whatever it already had for those two.
    status = (uint8_t)((pulled & ~(B | U)) | (status & (B | U)));
    SetFlag(U, 1); // always reads as 1 regardless
    return 0;
}

uint8_t CPU::ROL() {
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          cpuWrite(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::ROR() {
    fetch();
    uint16_t temp = ((uint16_t)GetFlag(C) << 7) | (fetched >> 1);
    SetFlag(C, fetched & 0x01);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          cpuWrite(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::RTI() {
    SP++;
    uint8_t pulled = cpuRead(0x0100 + SP);
    // Same rule as PLP: bits 4 (B) and 5 (U) of the pulled byte are ignored.
    status = (uint8_t)((pulled & ~(B | U)) | (status & (B | U)));
    SetFlag(U, 1);

    SP++;
    PC = (uint16_t)cpuRead(0x0100 + SP);
    SP++;
    PC |= (uint16_t)cpuRead(0x0100 + SP) << 8;
    return 0;
}

uint8_t CPU::RTS() {
    SP++;
    PC = (uint16_t)cpuRead(0x0100 + SP);
    SP++;
    PC |= (uint16_t)cpuRead(0x0100 + SP) << 8;
    PC++;
    return 0;
}

uint8_t CPU::SBC() {
    fetch();
    uint16_t value = ((uint16_t)fetched) ^ 0x00FF;
    uint16_t temp = (uint16_t)A + value + (uint16_t)GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(V, (temp ^ (uint16_t)A) & (temp ^ value) & 0x0080);
    SetFlag(N, temp & 0x0080);
    A = temp & 0x00FF;
    return 1;
}

uint8_t CPU::SEC() { SetFlag(C, true); return 0; }
uint8_t CPU::SED() { SetFlag(D, true); return 0; }
uint8_t CPU::SEI() { SetFlag(I, true); return 0; }

uint8_t CPU::STA() { cpuWrite(addr_abs, A); return 0; }
uint8_t CPU::STX() { cpuWrite(addr_abs, X); return 0; }
uint8_t CPU::STY() { cpuWrite(addr_abs, Y); return 0; }

uint8_t CPU::TAX() { X = A;  SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::TAY() { Y = A;  SetFlag(Z, Y == 0x00); SetFlag(N, Y & 0x80); return 0; }
uint8_t CPU::TSX() { X = SP; SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::TXA() { A = X;  SetFlag(Z, A == 0x00); SetFlag(N, A & 0x80); return 0; }
uint8_t CPU::TXS() { SP = X; return 0; }
uint8_t CPU::TYA() { A = Y;  SetFlag(Z, A == 0x00); SetFlag(N, A & 0x80); return 0; }

uint8_t CPU::XXX() { return 0; } // JAM/KIL opcodes: real hardware freezes; we just no-op to avoid hanging

// ======================= Illegal / Undocumented Opcodes =======================
// These aren't in any official reference, but real games use several of them
// (LAX/SAX especially), and nestest's extended log exercises all of them. The
// combo ones (SLO/RLA/SRE/RRA/DCP/ISC) are literally "do the read-modify-write
// op, then feed the result into the usual accumulator op" - the CPU's internal
// data path does both because of how the decode logic overlaps, not because
// anyone designed it that way.

uint8_t CPU::LAX() {
    fetch();
    A = fetched;
    X = fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1; // some addressing modes have a page-cross penalty
}

uint8_t CPU::SAX() {
    uint8_t value = A & X;
    cpuWrite(addr_abs, value);
    return 0; // no flags affected, no operand fetch (this is a store)
}

uint8_t CPU::DCP() {
    fetch();
    uint8_t temp = fetched - 1;
    cpuWrite(addr_abs, temp);
    // ...then CMP against the decremented value
    uint16_t cmp = (uint16_t)A - (uint16_t)temp;
    SetFlag(C, A >= temp);
    SetFlag(Z, (cmp & 0x00FF) == 0x0000);
    SetFlag(N, cmp & 0x0080);
    return 0;
}

uint8_t CPU::ISC() {
    fetch();
    uint8_t temp = fetched + 1;
    cpuWrite(addr_abs, temp);
    // ...then SBC against the incremented value
    uint16_t value = ((uint16_t)temp) ^ 0x00FF;
    uint16_t sum = (uint16_t)A + value + (uint16_t)GetFlag(C);
    SetFlag(C, sum & 0xFF00);
    SetFlag(Z, (sum & 0x00FF) == 0);
    SetFlag(V, (sum ^ (uint16_t)A) & (sum ^ value) & 0x0080);
    SetFlag(N, sum & 0x0080);
    A = sum & 0x00FF;
    return 0;
}

uint8_t CPU::SLO() {
    fetch();
    uint16_t temp = (uint16_t)fetched << 1;
    SetFlag(C, (temp & 0xFF00) > 0);
    cpuWrite(addr_abs, temp & 0x00FF);
    // ...then ORA with the shifted value
    A = A | (temp & 0x00FF);
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::RLA() {
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    cpuWrite(addr_abs, temp & 0x00FF);
    // ...then AND with the rotated value
    A = A & (temp & 0x00FF);
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::SRE() {
    fetch();
    SetFlag(C, fetched & 0x0001);
    uint8_t temp = fetched >> 1;
    cpuWrite(addr_abs, temp);
    // ...then EOR with the shifted value
    A = A ^ temp;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::RRA() {
    fetch();
    uint16_t rotated = ((uint16_t)GetFlag(C) << 7) | (fetched >> 1);
    SetFlag(C, fetched & 0x01); // carry-out from the ROR step
    cpuWrite(addr_abs, rotated & 0x00FF);
    // ...then ADC with the rotated value (using the carry ROR just set)
    uint8_t value = rotated & 0x00FF;
    uint16_t sum = (uint16_t)A + (uint16_t)value + (uint16_t)GetFlag(C);
    SetFlag(C, sum > 0x00FF); // final carry reflects the ADC, not the ROR
    SetFlag(Z, (sum & 0x00FF) == 0);
    SetFlag(V, (~((uint16_t)A ^ (uint16_t)value) & ((uint16_t)A ^ sum)) & 0x0080);
    SetFlag(N, sum & 0x0080);
    A = sum & 0x00FF;
    return 0;
}

uint8_t CPU::ANC() {
    fetch();
    A = A & fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    SetFlag(C, A & 0x80); // carry mirrors the sign bit, as if ASL had run
    return 0;
}

uint8_t CPU::ALR() {
    fetch();
    A = A & fetched;
    SetFlag(C, A & 0x01);
    A = A >> 1;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80); // always 0 after a right shift, but set it properly anyway
    return 0;
}

uint8_t CPU::ARR() {
    fetch();
    A = A & fetched;
    A = (A >> 1) | (GetFlag(C) << 7);
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    // Quirky flag behavior unique to this opcode:
    SetFlag(C, A & 0x40);
    SetFlag(V, ((A & 0x40) >> 6) ^ ((A & 0x20) >> 5));
    return 0;
}

uint8_t CPU::AXS() {
    fetch();
    uint8_t anded = A & X;
    uint16_t temp = (uint16_t)anded - (uint16_t)fetched;
    SetFlag(C, anded >= fetched);
    X = temp & 0x00FF;
    SetFlag(Z, X == 0x00);
    SetFlag(N, X & 0x80);
    return 0;
}

// --- Unstable opcodes: real behavior depends on internal bus-conflict
// timing that varies between chip revisions. These are the commonly-used
// approximations (matches what most emulators do), not a guarantee of
// bit-exact hardware match. ---

uint8_t CPU::LXA() {
    fetch();
    A = X = fetched; // approximation; real hardware ANDs with an unstable constant first
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::XAA() {
    fetch();
    A = X & fetched; // approximation; real hardware involves an unstable magic constant too
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::SHA() {
    uint8_t value = A & X & (uint8_t)((addr_abs >> 8) + 1);
    cpuWrite(addr_abs, value);
    return 0;
}

uint8_t CPU::TAS() {
    SP = A & X;
    uint8_t value = SP & (uint8_t)((addr_abs >> 8) + 1);
    cpuWrite(addr_abs, value);
    return 0;
}

uint8_t CPU::SHY() {
    uint8_t value = Y & (uint8_t)((addr_abs >> 8) + 1);
    cpuWrite(addr_abs, value);
    return 0;
}

uint8_t CPU::SHX() {
    uint8_t value = X & (uint8_t)((addr_abs >> 8) + 1);
    cpuWrite(addr_abs, value);
    return 0;
}

uint8_t CPU::LAS() {
    fetch();
    uint8_t temp = fetched & SP;
    A = temp; X = temp; SP = temp;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

// ============================ External Signals ==============================

void CPU::clock() {
    if (cycles == 0) {
        opcode = cpuRead(PC);
        SetFlag(U, true);
        PC++;

        cycles = lookup[opcode].cycles;

        uint8_t additional_cycle1 = (this->*lookup[opcode].addrmode)();
        uint8_t additional_cycle2 = (this->*lookup[opcode].operate)();

        cycles += (additional_cycle1 & additional_cycle2);

        SetFlag(U, true);
    }
    clock_count++;
    cycles--;
}

void CPU::reset() {
    A = 0; X = 0; Y = 0;
    SP = 0xFD;
    status = 0x00 | U | I; // hardware sets interrupt-disable on reset

    addr_abs = 0xFFFC;
    uint16_t lo = cpuRead(addr_abs);
    uint16_t hi = cpuRead(addr_abs + 1);
    PC = (hi << 8) | lo;

    addr_rel = 0x0000;
    addr_abs = 0x0000;
    fetched = 0x00;
    acc_mode = false;

    cycles = 7; // real hardware reset sequence takes 7 clock cycles
}

void CPU::irq() {
    if (GetFlag(I) == 0) {
        cpuWrite(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
        cpuWrite(0x0100 + SP, PC & 0x00FF);        SP--;

        SetFlag(B, 0);
        SetFlag(U, 1);
        SetFlag(I, 1);
        cpuWrite(0x0100 + SP, status); SP--;

        addr_abs = 0xFFFE;
        uint16_t lo = cpuRead(addr_abs);
        uint16_t hi = cpuRead(addr_abs + 1);
        PC = (hi << 8) | lo;

        cycles = 7;
    }
}

void CPU::nmi() {
    cpuWrite(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    cpuWrite(0x0100 + SP, PC & 0x00FF);        SP--;

    SetFlag(B, 0);
    SetFlag(U, 1);
    SetFlag(I, 1);
    cpuWrite(0x0100 + SP, status); SP--;

    addr_abs = 0xFFFA;
    uint16_t lo = cpuRead(addr_abs);
    uint16_t hi = cpuRead(addr_abs + 1);
    PC = (hi << 8) | lo;

    cycles = 8;
}

// ============================ Disassembly (peek-only) ========================
// Mirrors the addressing-mode logic above but never advances PC, never calls
// fetch(), and never writes anything - pure inspection for debuggers/logging.
// Format loosely follows Nintendulator-style logs (what nestest.log uses).

CPU::Disasm CPU::DisassembleAt(uint16_t addr) {
    uint8_t op = cpuRead(addr);
    const INSTRUCTION& instr = lookup[op];
    uint8_t length = 1;
    char buf[64] = {0};

    if (instr.addrmode == &CPU::IMP) {
        length = 1;
    } else if (instr.addrmode == &CPU::ACC) {
        length = 1;
        std::snprintf(buf, sizeof(buf), "A");
    } else if (instr.addrmode == &CPU::IMM) {
        uint8_t v = cpuRead(addr + 1);
        length = 2;
        std::snprintf(buf, sizeof(buf), "#$%02X", v);
    } else if (instr.addrmode == &CPU::ZP0) {
        uint8_t zp = cpuRead(addr + 1);
        uint8_t val = cpuRead(zp);
        length = 2;
        std::snprintf(buf, sizeof(buf), "$%02X = %02X", zp, val);
    } else if (instr.addrmode == &CPU::ZPX) {
        uint8_t zp = cpuRead(addr + 1);
        uint8_t eff = (uint8_t)(zp + X);
        uint8_t val = cpuRead(eff);
        length = 2;
        std::snprintf(buf, sizeof(buf), "$%02X,X @ %02X = %02X", zp, eff, val);
    } else if (instr.addrmode == &CPU::ZPY) {
        uint8_t zp = cpuRead(addr + 1);
        uint8_t eff = (uint8_t)(zp + Y);
        uint8_t val = cpuRead(eff);
        length = 2;
        std::snprintf(buf, sizeof(buf), "$%02X,Y @ %02X = %02X", zp, eff, val);
    } else if (instr.addrmode == &CPU::REL) {
        int8_t off = (int8_t)cpuRead(addr + 1);
        uint16_t target = (uint16_t)(addr + 2 + off);
        length = 2;
        std::snprintf(buf, sizeof(buf), "$%04X", target);
    } else if (instr.addrmode == &CPU::ABS) {
        uint16_t lo = cpuRead(addr + 1), hi = cpuRead(addr + 2);
        uint16_t a16 = (uint16_t)((hi << 8) | lo);
        length = 3;
        if (instr.name == "JMP" || instr.name == "JSR")
            std::snprintf(buf, sizeof(buf), "$%04X", a16);
        else
            std::snprintf(buf, sizeof(buf), "$%04X = %02X", a16, cpuRead(a16));
    } else if (instr.addrmode == &CPU::ABX) {
        uint16_t lo = cpuRead(addr + 1), hi = cpuRead(addr + 2);
        uint16_t base = (uint16_t)((hi << 8) | lo);
        uint16_t eff = (uint16_t)(base + X);
        length = 3;
        std::snprintf(buf, sizeof(buf), "$%04X,X @ %04X = %02X", base, eff, cpuRead(eff));
    } else if (instr.addrmode == &CPU::ABY) {
        uint16_t lo = cpuRead(addr + 1), hi = cpuRead(addr + 2);
        uint16_t base = (uint16_t)((hi << 8) | lo);
        uint16_t eff = (uint16_t)(base + Y);
        length = 3;
        std::snprintf(buf, sizeof(buf), "$%04X,Y @ %04X = %02X", base, eff, cpuRead(eff));
    } else if (instr.addrmode == &CPU::IND) {
        uint16_t ptr_lo = cpuRead(addr + 1), ptr_hi = cpuRead(addr + 2);
        uint16_t ptr = (uint16_t)((ptr_hi << 8) | ptr_lo);
        uint16_t eff;
        if ((ptr & 0x00FF) == 0x00FF)
            eff = (uint16_t)((cpuRead(ptr & 0xFF00) << 8) | cpuRead(ptr)); // page-boundary bug
        else
            eff = (uint16_t)((cpuRead(ptr + 1) << 8) | cpuRead(ptr));
        length = 3;
        std::snprintf(buf, sizeof(buf), "($%04X) = %04X", ptr, eff);
    } else if (instr.addrmode == &CPU::IZX) {
        uint8_t t = cpuRead(addr + 1);
        uint8_t zp = (uint8_t)(t + X);
        uint16_t lo = cpuRead(zp), hi = cpuRead((uint8_t)(zp + 1));
        uint16_t eff = (uint16_t)((hi << 8) | lo);
        length = 2;
        std::snprintf(buf, sizeof(buf), "($%02X,X) @ %02X = %04X = %02X", t, zp, eff, cpuRead(eff));
    } else if (instr.addrmode == &CPU::IZY) {
        uint8_t t = cpuRead(addr + 1);
        uint16_t lo = cpuRead(t), hi = cpuRead((uint8_t)(t + 1));
        uint16_t base = (uint16_t)((hi << 8) | lo);
        uint16_t eff = (uint16_t)(base + Y);
        length = 2;
        std::snprintf(buf, sizeof(buf), "($%02X),Y = %04X @ %04X = %02X", t, base, eff, cpuRead(eff));
    }

    std::string text = instr.name;
    if (buf[0] != '\0') { text += " "; text += buf; }
    return { text, length };
}

// ============================== Opcode Table =================================

CPU::CPU() {
    lookup = { {
        // 0x00-0x0F
        {"BRK",&CPU::BRK,&CPU::IMP,7},{"ORA",&CPU::ORA,&CPU::IZX,6},{"???",&CPU::XXX,&CPU::IMP,2},{"SLO",&CPU::SLO,&CPU::IZX,8},
        {"NOP",&CPU::NOP,&CPU::ZP0,3},{"ORA",&CPU::ORA,&CPU::ZP0,3},{"ASL",&CPU::ASL,&CPU::ZP0,5},{"SLO",&CPU::SLO,&CPU::ZP0,5},
        {"PHP",&CPU::PHP,&CPU::IMP,3},{"ORA",&CPU::ORA,&CPU::IMM,2},{"ASL",&CPU::ASL,&CPU::ACC,2},{"ANC",&CPU::ANC,&CPU::IMM,2},
        {"NOP",&CPU::NOP,&CPU::ABS,4},{"ORA",&CPU::ORA,&CPU::ABS,4},{"ASL",&CPU::ASL,&CPU::ABS,6},{"SLO",&CPU::SLO,&CPU::ABS,6},

        // 0x10-0x1F
        {"BPL",&CPU::BPL,&CPU::REL,2},{"ORA",&CPU::ORA,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"SLO",&CPU::SLO,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"ORA",&CPU::ORA,&CPU::ZPX,4},{"ASL",&CPU::ASL,&CPU::ZPX,6},{"SLO",&CPU::SLO,&CPU::ZPX,6},
        {"CLC",&CPU::CLC,&CPU::IMP,2},{"ORA",&CPU::ORA,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"SLO",&CPU::SLO,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"ORA",&CPU::ORA,&CPU::ABX,4},{"ASL",&CPU::ASL,&CPU::ABX,7},{"SLO",&CPU::SLO,&CPU::ABX,7},

        // 0x20-0x2F
        {"JSR",&CPU::JSR,&CPU::ABS,6},{"AND",&CPU::AND,&CPU::IZX,6},{"???",&CPU::XXX,&CPU::IMP,2},{"RLA",&CPU::RLA,&CPU::IZX,8},
        {"BIT",&CPU::BIT,&CPU::ZP0,3},{"AND",&CPU::AND,&CPU::ZP0,3},{"ROL",&CPU::ROL,&CPU::ZP0,5},{"RLA",&CPU::RLA,&CPU::ZP0,5},
        {"PLP",&CPU::PLP,&CPU::IMP,4},{"AND",&CPU::AND,&CPU::IMM,2},{"ROL",&CPU::ROL,&CPU::ACC,2},{"ANC",&CPU::ANC,&CPU::IMM,2},
        {"BIT",&CPU::BIT,&CPU::ABS,4},{"AND",&CPU::AND,&CPU::ABS,4},{"ROL",&CPU::ROL,&CPU::ABS,6},{"RLA",&CPU::RLA,&CPU::ABS,6},

        // 0x30-0x3F
        {"BMI",&CPU::BMI,&CPU::REL,2},{"AND",&CPU::AND,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"RLA",&CPU::RLA,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"AND",&CPU::AND,&CPU::ZPX,4},{"ROL",&CPU::ROL,&CPU::ZPX,6},{"RLA",&CPU::RLA,&CPU::ZPX,6},
        {"SEC",&CPU::SEC,&CPU::IMP,2},{"AND",&CPU::AND,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"RLA",&CPU::RLA,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"AND",&CPU::AND,&CPU::ABX,4},{"ROL",&CPU::ROL,&CPU::ABX,7},{"RLA",&CPU::RLA,&CPU::ABX,7},

        // 0x40-0x4F
        {"RTI",&CPU::RTI,&CPU::IMP,6},{"EOR",&CPU::EOR,&CPU::IZX,6},{"???",&CPU::XXX,&CPU::IMP,2},{"SRE",&CPU::SRE,&CPU::IZX,8},
        {"NOP",&CPU::NOP,&CPU::ZP0,3},{"EOR",&CPU::EOR,&CPU::ZP0,3},{"LSR",&CPU::LSR,&CPU::ZP0,5},{"SRE",&CPU::SRE,&CPU::ZP0,5},
        {"PHA",&CPU::PHA,&CPU::IMP,3},{"EOR",&CPU::EOR,&CPU::IMM,2},{"LSR",&CPU::LSR,&CPU::ACC,2},{"ALR",&CPU::ALR,&CPU::IMM,2},
        {"JMP",&CPU::JMP,&CPU::ABS,3},{"EOR",&CPU::EOR,&CPU::ABS,4},{"LSR",&CPU::LSR,&CPU::ABS,6},{"SRE",&CPU::SRE,&CPU::ABS,6},

        // 0x50-0x5F
        {"BVC",&CPU::BVC,&CPU::REL,2},{"EOR",&CPU::EOR,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"SRE",&CPU::SRE,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"EOR",&CPU::EOR,&CPU::ZPX,4},{"LSR",&CPU::LSR,&CPU::ZPX,6},{"SRE",&CPU::SRE,&CPU::ZPX,6},
        {"CLI",&CPU::CLI,&CPU::IMP,2},{"EOR",&CPU::EOR,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"SRE",&CPU::SRE,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"EOR",&CPU::EOR,&CPU::ABX,4},{"LSR",&CPU::LSR,&CPU::ABX,7},{"SRE",&CPU::SRE,&CPU::ABX,7},

        // 0x60-0x6F
        {"RTS",&CPU::RTS,&CPU::IMP,6},{"ADC",&CPU::ADC,&CPU::IZX,6},{"???",&CPU::XXX,&CPU::IMP,2},{"RRA",&CPU::RRA,&CPU::IZX,8},
        {"NOP",&CPU::NOP,&CPU::ZP0,3},{"ADC",&CPU::ADC,&CPU::ZP0,3},{"ROR",&CPU::ROR,&CPU::ZP0,5},{"RRA",&CPU::RRA,&CPU::ZP0,5},
        {"PLA",&CPU::PLA,&CPU::IMP,4},{"ADC",&CPU::ADC,&CPU::IMM,2},{"ROR",&CPU::ROR,&CPU::ACC,2},{"ARR",&CPU::ARR,&CPU::IMM,2},
        {"JMP",&CPU::JMP,&CPU::IND,5},{"ADC",&CPU::ADC,&CPU::ABS,4},{"ROR",&CPU::ROR,&CPU::ABS,6},{"RRA",&CPU::RRA,&CPU::ABS,6},

        // 0x70-0x7F
        {"BVS",&CPU::BVS,&CPU::REL,2},{"ADC",&CPU::ADC,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"RRA",&CPU::RRA,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"ADC",&CPU::ADC,&CPU::ZPX,4},{"ROR",&CPU::ROR,&CPU::ZPX,6},{"RRA",&CPU::RRA,&CPU::ZPX,6},
        {"SEI",&CPU::SEI,&CPU::IMP,2},{"ADC",&CPU::ADC,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"RRA",&CPU::RRA,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"ADC",&CPU::ADC,&CPU::ABX,4},{"ROR",&CPU::ROR,&CPU::ABX,7},{"RRA",&CPU::RRA,&CPU::ABX,7},

        // 0x80-0x8F
        {"NOP",&CPU::NOP,&CPU::IMM,2},{"STA",&CPU::STA,&CPU::IZX,6},{"NOP",&CPU::NOP,&CPU::IMM,2},{"SAX",&CPU::SAX,&CPU::IZX,6},
        {"STY",&CPU::STY,&CPU::ZP0,3},{"STA",&CPU::STA,&CPU::ZP0,3},{"STX",&CPU::STX,&CPU::ZP0,3},{"SAX",&CPU::SAX,&CPU::ZP0,3},
        {"DEY",&CPU::DEY,&CPU::IMP,2},{"NOP",&CPU::NOP,&CPU::IMM,2},{"TXA",&CPU::TXA,&CPU::IMP,2},{"XAA",&CPU::XAA,&CPU::IMM,2},
        {"STY",&CPU::STY,&CPU::ABS,4},{"STA",&CPU::STA,&CPU::ABS,4},{"STX",&CPU::STX,&CPU::ABS,4},{"SAX",&CPU::SAX,&CPU::ABS,4},

        // 0x90-0x9F
        {"BCC",&CPU::BCC,&CPU::REL,2},{"STA",&CPU::STA,&CPU::IZY,6},{"???",&CPU::XXX,&CPU::IMP,2},{"SHA",&CPU::SHA,&CPU::IZY,6},
        {"STY",&CPU::STY,&CPU::ZPX,4},{"STA",&CPU::STA,&CPU::ZPX,4},{"STX",&CPU::STX,&CPU::ZPY,4},{"SAX",&CPU::SAX,&CPU::ZPY,4},
        {"TYA",&CPU::TYA,&CPU::IMP,2},{"STA",&CPU::STA,&CPU::ABY,5},{"TXS",&CPU::TXS,&CPU::IMP,2},{"TAS",&CPU::TAS,&CPU::ABY,5},
        {"SHY",&CPU::SHY,&CPU::ABX,5},{"STA",&CPU::STA,&CPU::ABX,5},{"SHX",&CPU::SHX,&CPU::ABY,5},{"SHA",&CPU::SHA,&CPU::ABY,5},

        // 0xA0-0xAF
        {"LDY",&CPU::LDY,&CPU::IMM,2},{"LDA",&CPU::LDA,&CPU::IZX,6},{"LDX",&CPU::LDX,&CPU::IMM,2},{"LAX",&CPU::LAX,&CPU::IZX,6},
        {"LDY",&CPU::LDY,&CPU::ZP0,3},{"LDA",&CPU::LDA,&CPU::ZP0,3},{"LDX",&CPU::LDX,&CPU::ZP0,3},{"LAX",&CPU::LAX,&CPU::ZP0,3},
        {"TAY",&CPU::TAY,&CPU::IMP,2},{"LDA",&CPU::LDA,&CPU::IMM,2},{"TAX",&CPU::TAX,&CPU::IMP,2},{"LXA",&CPU::LXA,&CPU::IMM,2},
        {"LDY",&CPU::LDY,&CPU::ABS,4},{"LDA",&CPU::LDA,&CPU::ABS,4},{"LDX",&CPU::LDX,&CPU::ABS,4},{"LAX",&CPU::LAX,&CPU::ABS,4},

        // 0xB0-0xBF
        {"BCS",&CPU::BCS,&CPU::REL,2},{"LDA",&CPU::LDA,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"LAX",&CPU::LAX,&CPU::IZY,5},
        {"LDY",&CPU::LDY,&CPU::ZPX,4},{"LDA",&CPU::LDA,&CPU::ZPX,4},{"LDX",&CPU::LDX,&CPU::ZPY,4},{"LAX",&CPU::LAX,&CPU::ZPY,4},
        {"CLV",&CPU::CLV,&CPU::IMP,2},{"LDA",&CPU::LDA,&CPU::ABY,4},{"TSX",&CPU::TSX,&CPU::IMP,2},{"LAS",&CPU::LAS,&CPU::ABY,4},
        {"LDY",&CPU::LDY,&CPU::ABX,4},{"LDA",&CPU::LDA,&CPU::ABX,4},{"LDX",&CPU::LDX,&CPU::ABY,4},{"LAX",&CPU::LAX,&CPU::ABY,4},

        // 0xC0-0xCF
        {"CPY",&CPU::CPY,&CPU::IMM,2},{"CMP",&CPU::CMP,&CPU::IZX,6},{"NOP",&CPU::NOP,&CPU::IMM,2},{"DCP",&CPU::DCP,&CPU::IZX,8},
        {"CPY",&CPU::CPY,&CPU::ZP0,3},{"CMP",&CPU::CMP,&CPU::ZP0,3},{"DEC",&CPU::DEC,&CPU::ZP0,5},{"DCP",&CPU::DCP,&CPU::ZP0,5},
        {"INY",&CPU::INY,&CPU::IMP,2},{"CMP",&CPU::CMP,&CPU::IMM,2},{"DEX",&CPU::DEX,&CPU::IMP,2},{"AXS",&CPU::AXS,&CPU::IMM,2},
        {"CPY",&CPU::CPY,&CPU::ABS,4},{"CMP",&CPU::CMP,&CPU::ABS,4},{"DEC",&CPU::DEC,&CPU::ABS,6},{"DCP",&CPU::DCP,&CPU::ABS,6},

        // 0xD0-0xDF
        {"BNE",&CPU::BNE,&CPU::REL,2},{"CMP",&CPU::CMP,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"DCP",&CPU::DCP,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"CMP",&CPU::CMP,&CPU::ZPX,4},{"DEC",&CPU::DEC,&CPU::ZPX,6},{"DCP",&CPU::DCP,&CPU::ZPX,6},
        {"CLD",&CPU::CLD,&CPU::IMP,2},{"CMP",&CPU::CMP,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"DCP",&CPU::DCP,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"CMP",&CPU::CMP,&CPU::ABX,4},{"DEC",&CPU::DEC,&CPU::ABX,7},{"DCP",&CPU::DCP,&CPU::ABX,7},

        // 0xE0-0xEF
        {"CPX",&CPU::CPX,&CPU::IMM,2},{"SBC",&CPU::SBC,&CPU::IZX,6},{"NOP",&CPU::NOP,&CPU::IMM,2},{"ISC",&CPU::ISC,&CPU::IZX,8},
        {"CPX",&CPU::CPX,&CPU::ZP0,3},{"SBC",&CPU::SBC,&CPU::ZP0,3},{"INC",&CPU::INC,&CPU::ZP0,5},{"ISC",&CPU::ISC,&CPU::ZP0,5},
        {"INX",&CPU::INX,&CPU::IMP,2},{"SBC",&CPU::SBC,&CPU::IMM,2},{"NOP",&CPU::NOP,&CPU::IMP,2},{"SBC",&CPU::SBC,&CPU::IMM,2},
        {"CPX",&CPU::CPX,&CPU::ABS,4},{"SBC",&CPU::SBC,&CPU::ABS,4},{"INC",&CPU::INC,&CPU::ABS,6},{"ISC",&CPU::ISC,&CPU::ABS,6},

        // 0xF0-0xFF
        {"BEQ",&CPU::BEQ,&CPU::REL,2},{"SBC",&CPU::SBC,&CPU::IZY,5},{"???",&CPU::XXX,&CPU::IMP,2},{"ISC",&CPU::ISC,&CPU::IZY,8},
        {"NOP",&CPU::NOP,&CPU::ZPX,4},{"SBC",&CPU::SBC,&CPU::ZPX,4},{"INC",&CPU::INC,&CPU::ZPX,6},{"ISC",&CPU::ISC,&CPU::ZPX,6},
        {"SED",&CPU::SED,&CPU::IMP,2},{"SBC",&CPU::SBC,&CPU::ABY,4},{"NOP",&CPU::NOP,&CPU::IMP,2},{"ISC",&CPU::ISC,&CPU::ABY,7},
        {"NOP",&CPU::NOP,&CPU::ABX,4},{"SBC",&CPU::SBC,&CPU::ABX,4},{"INC",&CPU::INC,&CPU::ABX,7},{"ISC",&CPU::ISC,&CPU::ABX,7},
    } };
}
