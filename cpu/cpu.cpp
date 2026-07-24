#include "cpu.h"
#include "bus.h"

uint8_t CPU::read(uint16_t addr)              { return bus->read(addr); }
void    CPU::write(uint16_t addr, uint8_t d)  { bus->write(addr, d); }

uint8_t CPU::GetFlag(FLAGS6502 f) const { return (status & f) ? 1 : 0; }
void    CPU::SetFlag(FLAGS6502 f, bool v) {
    if (v) status |= f;
    else   status &= ~f;
}

uint8_t CPU::fetch() {
    if (!acc_mode)
        fetched = read(addr_abs);
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
    addr_abs = read(PC); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::ZPX() {
    acc_mode = false;
    addr_abs = (read(PC) + X); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::ZPY() {
    acc_mode = false;
    addr_abs = (read(PC) + Y); PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::REL() {
    acc_mode = false;
    addr_rel = read(PC); PC++;
    if (addr_rel & 0x80) addr_rel |= 0xFF00; // sign-extend
    return 0;
}

uint8_t CPU::ABS() {
    acc_mode = false;
    uint16_t lo = read(PC); PC++;
    uint16_t hi = read(PC); PC++;
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::ABX() {
    acc_mode = false;
    uint16_t lo = read(PC); PC++;
    uint16_t hi = read(PC); PC++;
    addr_abs = ((hi << 8) | lo) + X;
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0; // page crossed
}

uint8_t CPU::ABY() {
    acc_mode = false;
    uint16_t lo = read(PC); PC++;
    uint16_t hi = read(PC); PC++;
    addr_abs = ((hi << 8) | lo) + Y;
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

uint8_t CPU::IND() {
    acc_mode = false;
    uint16_t ptr_lo = read(PC); PC++;
    uint16_t ptr_hi = read(PC); PC++;
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    if (ptr_lo == 0x00FF) // reproduce the famous page-boundary hardware bug
        addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr);
    else
        addr_abs = (read(ptr + 1) << 8) | read(ptr);
    return 0;
}

uint8_t CPU::IZX() {
    acc_mode = false;
    uint16_t t = read(PC); PC++;
    uint16_t lo = read((uint16_t)(t + X) & 0x00FF);
    uint16_t hi = read((uint16_t)(t + X + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::IZY() {
    acc_mode = false;
    uint16_t t = read(PC); PC++;
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
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
    return 1; // may need extra cycle from addressing mode
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
    else          write(addr_abs, temp & 0x00FF);
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

    write(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    write(0x0100 + SP, PC & 0x00FF);        SP--;

    SetFlag(B, 1);
    write(0x0100 + SP, status);
    SetFlag(B, 0);
    SP--;

    PC = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
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
    write(addr_abs, temp & 0x00FF);
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
    write(addr_abs, temp & 0x00FF);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::INX() { X++; SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::INY() { Y++; SetFlag(Z, Y == 0x00); SetFlag(N, Y & 0x80); return 0; }

uint8_t CPU::JMP() { PC = addr_abs; return 0; }

uint8_t CPU::JSR() {
    PC--;
    write(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    write(0x0100 + SP, PC & 0x00FF);        SP--;
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
    else          write(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::NOP() { return 0; }

uint8_t CPU::ORA() {
    fetch();
    A = A | fetched;
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 1;
}

uint8_t CPU::PHA() {
    write(0x0100 + SP, A);
    SP--;
    return 0;
}

uint8_t CPU::PHP() {
    // B and U are pushed as 1, but do not persist as real CPU state
    write(0x0100 + SP, status | B | U);
    SetFlag(B, 0);
    SetFlag(U, 0);
    SP--;
    return 0;
}

uint8_t CPU::PLA() {
    SP++;
    A = read(0x0100 + SP);
    SetFlag(Z, A == 0x00);
    SetFlag(N, A & 0x80);
    return 0;
}

uint8_t CPU::PLP() {
    SP++;
    status = read(0x0100 + SP);
    SetFlag(U, 1); // unused bit always reads as 1
    return 0;
}

uint8_t CPU::ROL() {
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          write(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::ROR() {
    fetch();
    uint16_t temp = ((uint16_t)GetFlag(C) << 7) | (fetched >> 1);
    SetFlag(C, fetched & 0x01);
    SetFlag(Z, (temp & 0x00FF) == 0x0000);
    SetFlag(N, temp & 0x0080);
    if (acc_mode) A = temp & 0x00FF;
    else          write(addr_abs, temp & 0x00FF);
    return 0;
}

uint8_t CPU::RTI() {
    SP++;
    status = read(0x0100 + SP);
    status &= ~B;
    status &= ~U;

    SP++;
    PC = (uint16_t)read(0x0100 + SP);
    SP++;
    PC |= (uint16_t)read(0x0100 + SP) << 8;
    return 0;
}

uint8_t CPU::RTS() {
    SP++;
    PC = (uint16_t)read(0x0100 + SP);
    SP++;
    PC |= (uint16_t)read(0x0100 + SP) << 8;
    PC++;
    return 0;
}

uint8_t CPU::SBC() {
    fetch();
    // SBC is ADC with the operand bitwise-inverted
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

uint8_t CPU::STA() { write(addr_abs, A); return 0; }
uint8_t CPU::STX() { write(addr_abs, X); return 0; }
uint8_t CPU::STY() { write(addr_abs, Y); return 0; }

uint8_t CPU::TAX() { X = A;  SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::TAY() { Y = A;  SetFlag(Z, Y == 0x00); SetFlag(N, Y & 0x80); return 0; }
uint8_t CPU::TSX() { X = SP; SetFlag(Z, X == 0x00); SetFlag(N, X & 0x80); return 0; }
uint8_t CPU::TXA() { A = X;  SetFlag(Z, A == 0x00); SetFlag(N, A & 0x80); return 0; }
uint8_t CPU::TXS() { SP = X; return 0; } // no flags affected
uint8_t CPU::TYA() { A = Y;  SetFlag(Z, A == 0x00); SetFlag(N, A & 0x80); return 0; }

uint8_t CPU::XXX() { return 0; } // illegal/undocumented opcode placeholder

// ============================ External Signals ==============================

void CPU::clock() {
    if (cycles == 0) {
        opcode = read(PC);
        SetFlag(U, true);
        PC++;

        cycles = lookup[opcode].cycles;

        uint8_t additional_cycle1 = (this->*lookup[opcode].addrmode)();
        uint8_t additional_cycle2 = (this->*lookup[opcode].operate)();

        // Both the addressing mode AND the opcode must agree an extra
        // cycle is possible (page-crossing penalty) before it's added.
        cycles += (additional_cycle1 & additional_cycle2);

        SetFlag(U, true);
    }
    clock_count++;
    cycles--;
}

void CPU::reset() {
    A = 0; X = 0; Y = 0;
    SP = 0xFD;
    status = 0x00 | U;

    addr_abs = 0xFFFC;
    uint16_t lo = read(addr_abs);
    uint16_t hi = read(addr_abs + 1);
    PC = (hi << 8) | lo;

    addr_rel = 0x0000;
    addr_abs = 0x0000;
    fetched = 0x00;
    acc_mode = false;

    cycles = 8; // reset takes a fixed number of cycles
}

void CPU::irq() {
    if (GetFlag(I) == 0) {
        write(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
        write(0x0100 + SP, PC & 0x00FF);        SP--;

        SetFlag(B, 0);
        SetFlag(U, 1);
        SetFlag(I, 1);
        write(0x0100 + SP, status); SP--;

        addr_abs = 0xFFFE;
        uint16_t lo = read(addr_abs);
        uint16_t hi = read(addr_abs + 1);
        PC = (hi << 8) | lo;

        cycles = 7;
    }
}

void CPU::nmi() {
    write(0x0100 + SP, (PC >> 8) & 0x00FF); SP--;
    write(0x0100 + SP, PC & 0x00FF);        SP--;

    SetFlag(B, 0);
    SetFlag(U, 1);
    SetFlag(I, 1);
    write(0x0100 + SP, status); SP--;

    addr_abs = 0xFFFA;
    uint16_t lo = read(addr_abs);
    uint16_t hi = read(addr_abs + 1);
    PC = (hi << 8) | lo;

    cycles = 8;
}

// ============================== Opcode Table =================================
// 256 entries. Official opcodes carry their real mnemonic, addressing mode,
// and cycle count (matches the NESdev reference exactly). Unused/illegal slots
// are stubbed as {"???", XXX, IMP, 2} — fill these in later once official
// opcodes pass nestest, if you want illegal-opcode support too.

CPU::CPU() {
    using c = CPU;
    lookup = { {
        // 0x00
        {"BRK",&c::BRK,&c::IMP,7},{"ORA",&c::ORA,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,3},{"ORA",&c::ORA,&c::ZP0,3},{"ASL",&c::ASL,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"PHP",&c::PHP,&c::IMP,3},{"ORA",&c::ORA,&c::IMM,2},{"ASL",&c::ASL,&c::ACC,2},{"???",&c::XXX,&c::IMP,2},
        {"???",&c::XXX,&c::IMP,4},{"ORA",&c::ORA,&c::ABS,4},{"ASL",&c::ASL,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0x10
        {"BPL",&c::BPL,&c::REL,2},{"ORA",&c::ORA,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"ORA",&c::ORA,&c::ZPX,4},{"ASL",&c::ASL,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"CLC",&c::CLC,&c::IMP,2},{"ORA",&c::ORA,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"ORA",&c::ORA,&c::ABX,4},{"ASL",&c::ASL,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
        // 0x20
        {"JSR",&c::JSR,&c::ABS,6},{"AND",&c::AND,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"BIT",&c::BIT,&c::ZP0,3},{"AND",&c::AND,&c::ZP0,3},{"ROL",&c::ROL,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"PLP",&c::PLP,&c::IMP,4},{"AND",&c::AND,&c::IMM,2},{"ROL",&c::ROL,&c::ACC,2},{"???",&c::XXX,&c::IMP,2},
        {"BIT",&c::BIT,&c::ABS,4},{"AND",&c::AND,&c::ABS,4},{"ROL",&c::ROL,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0x30
        {"BMI",&c::BMI,&c::REL,2},{"AND",&c::AND,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"AND",&c::AND,&c::ZPX,4},{"ROL",&c::ROL,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"SEC",&c::SEC,&c::IMP,2},{"AND",&c::AND,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"AND",&c::AND,&c::ABX,4},{"ROL",&c::ROL,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
        // 0x40
        {"RTI",&c::RTI,&c::IMP,6},{"EOR",&c::EOR,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,3},{"EOR",&c::EOR,&c::ZP0,3},{"LSR",&c::LSR,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"PHA",&c::PHA,&c::IMP,3},{"EOR",&c::EOR,&c::IMM,2},{"LSR",&c::LSR,&c::ACC,2},{"???",&c::XXX,&c::IMP,2},
        {"JMP",&c::JMP,&c::ABS,3},{"EOR",&c::EOR,&c::ABS,4},{"LSR",&c::LSR,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0x50
        {"BVC",&c::BVC,&c::REL,2},{"EOR",&c::EOR,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"EOR",&c::EOR,&c::ZPX,4},{"LSR",&c::LSR,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"CLI",&c::CLI,&c::IMP,2},{"EOR",&c::EOR,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"EOR",&c::EOR,&c::ABX,4},{"LSR",&c::LSR,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
        // 0x60
        {"RTS",&c::RTS,&c::IMP,6},{"ADC",&c::ADC,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,3},{"ADC",&c::ADC,&c::ZP0,3},{"ROR",&c::ROR,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"PLA",&c::PLA,&c::IMP,4},{"ADC",&c::ADC,&c::IMM,2},{"ROR",&c::ROR,&c::ACC,2},{"???",&c::XXX,&c::IMP,2},
        {"JMP",&c::JMP,&c::IND,5},{"ADC",&c::ADC,&c::ABS,4},{"ROR",&c::ROR,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0x70
        {"BVS",&c::BVS,&c::REL,2},{"ADC",&c::ADC,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"ADC",&c::ADC,&c::ZPX,4},{"ROR",&c::ROR,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"SEI",&c::SEI,&c::IMP,2},{"ADC",&c::ADC,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"ADC",&c::ADC,&c::ABX,4},{"ROR",&c::ROR,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
        // 0x80
        {"???",&c::XXX,&c::IMP,2},{"STA",&c::STA,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,6},
        {"STY",&c::STY,&c::ZP0,3},{"STA",&c::STA,&c::ZP0,3},{"STX",&c::STX,&c::ZP0,3},{"???",&c::XXX,&c::IMP,3},
        {"DEY",&c::DEY,&c::IMP,2},{"???",&c::XXX,&c::IMP,2},{"TXA",&c::TXA,&c::IMP,2},{"???",&c::XXX,&c::IMP,2},
        {"STY",&c::STY,&c::ABS,4},{"STA",&c::STA,&c::ABS,4},{"STX",&c::STX,&c::ABS,4},{"???",&c::XXX,&c::IMP,4},
        // 0x90
        {"BCC",&c::BCC,&c::REL,2},{"STA",&c::STA,&c::IZY,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,6},
        {"STY",&c::STY,&c::ZPX,4},{"STA",&c::STA,&c::ZPX,4},{"STX",&c::STX,&c::ZPY,4},{"???",&c::XXX,&c::IMP,4},
        {"TYA",&c::TYA,&c::IMP,2},{"STA",&c::STA,&c::ABY,5},{"TXS",&c::TXS,&c::IMP,2},{"???",&c::XXX,&c::IMP,5},
        {"???",&c::XXX,&c::IMP,5},{"STA",&c::STA,&c::ABX,5},{"???",&c::XXX,&c::IMP,5},{"???",&c::XXX,&c::IMP,5},
        // 0xA0
        {"LDY",&c::LDY,&c::IMM,2},{"LDA",&c::LDA,&c::IZX,6},{"LDX",&c::LDX,&c::IMM,2},{"???",&c::XXX,&c::IMP,6},
        {"LDY",&c::LDY,&c::ZP0,3},{"LDA",&c::LDA,&c::ZP0,3},{"LDX",&c::LDX,&c::ZP0,3},{"???",&c::XXX,&c::IMP,3},
        {"TAY",&c::TAY,&c::IMP,2},{"LDA",&c::LDA,&c::IMM,2},{"TAX",&c::TAX,&c::IMP,2},{"???",&c::XXX,&c::IMP,2},
        {"LDY",&c::LDY,&c::ABS,4},{"LDA",&c::LDA,&c::ABS,4},{"LDX",&c::LDX,&c::ABS,4},{"???",&c::XXX,&c::IMP,4},
        // 0xB0
        {"BCS",&c::BCS,&c::REL,2},{"LDA",&c::LDA,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,5},
        {"LDY",&c::LDY,&c::ZPX,4},{"LDA",&c::LDA,&c::ZPX,4},{"LDX",&c::LDX,&c::ZPY,4},{"???",&c::XXX,&c::IMP,4},
        {"CLV",&c::CLV,&c::IMP,2},{"LDA",&c::LDA,&c::ABY,4},{"TSX",&c::TSX,&c::IMP,2},{"???",&c::XXX,&c::IMP,4},
        {"LDY",&c::LDY,&c::ABX,4},{"LDA",&c::LDA,&c::ABX,4},{"LDX",&c::LDX,&c::ABY,4},{"???",&c::XXX,&c::IMP,4},
        // 0xC0
        {"CPY",&c::CPY,&c::IMM,2},{"CMP",&c::CMP,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"CPY",&c::CPY,&c::ZP0,3},{"CMP",&c::CMP,&c::ZP0,3},{"DEC",&c::DEC,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"INY",&c::INY,&c::IMP,2},{"CMP",&c::CMP,&c::IMM,2},{"DEX",&c::DEX,&c::IMP,2},{"???",&c::XXX,&c::IMP,2},
        {"CPY",&c::CPY,&c::ABS,4},{"CMP",&c::CMP,&c::ABS,4},{"DEC",&c::DEC,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0xD0
        {"BNE",&c::BNE,&c::REL,2},{"CMP",&c::CMP,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"CMP",&c::CMP,&c::ZPX,4},{"DEC",&c::DEC,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"CLD",&c::CLD,&c::IMP,2},{"CMP",&c::CMP,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"CMP",&c::CMP,&c::ABX,4},{"DEC",&c::DEC,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
        // 0xE0
        {"CPX",&c::CPX,&c::IMM,2},{"SBC",&c::SBC,&c::IZX,6},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"CPX",&c::CPX,&c::ZP0,3},{"SBC",&c::SBC,&c::ZP0,3},{"INC",&c::INC,&c::ZP0,5},{"???",&c::XXX,&c::IMP,5},
        {"INX",&c::INX,&c::IMP,2},{"SBC",&c::SBC,&c::IMM,2},{"NOP",&c::NOP,&c::IMP,2},{"???",&c::XXX,&c::IMP,2},
        {"CPX",&c::CPX,&c::ABS,4},{"SBC",&c::SBC,&c::ABS,4},{"INC",&c::INC,&c::ABS,6},{"???",&c::XXX,&c::IMP,6},
        // 0xF0
        {"BEQ",&c::BEQ,&c::REL,2},{"SBC",&c::SBC,&c::IZY,5},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,8},
        {"???",&c::XXX,&c::IMP,4},{"SBC",&c::SBC,&c::ZPX,4},{"INC",&c::INC,&c::ZPX,6},{"???",&c::XXX,&c::IMP,6},
        {"SED",&c::SED,&c::IMP,2},{"SBC",&c::SBC,&c::ABY,4},{"???",&c::XXX,&c::IMP,2},{"???",&c::XXX,&c::IMP,7},
        {"???",&c::XXX,&c::IMP,4},{"SBC",&c::SBC,&c::ABX,4},{"INC",&c::INC,&c::ABX,7},{"???",&c::XXX,&c::IMP,7},
    } };
}
