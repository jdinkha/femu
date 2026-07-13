void LDA() {
    uint8_t value = read(addr_abs);
    A = value;
    setFlag(Z, A == 0x00);
    setFlag(N, A & 0x80);
}

void STA() {
    write(addr_abs, A);
}

void ADC() {
    uint8_t value = read(addr_abs);
    uint16_t sum = A + value + (getFlag(C) ? 1 : 0);
    setFlag(C, sum > 0xFF);
    setFlag(Z, (sum & 0xFF) == 0);
    setFlag(V, (~(A ^ value) & (A ^ sum)) & 0x80); // classic overflow trick
    setFlag(N, sum & 0x80);
    A = sum & 0xFF;
}



struct Instruction {
    std::string name;
    void (CPU::*operate)()  = nullptr;
    void (CPU::*addrmode)() = nullptr;
    uint8_t cycles = 0;
};

std::array<Instruction, 256> lookup = {
    { "BRK", &CPU::BRK, &CPU::IMP, 7 }, { "ORA", &CPU::ORA, &CPU::IZX, 6 },
    { "???", &CPU::XXX, &CPU::IMP, 2 }, /* ... illegal opcode ... */
    // ... all 256 entries
};


void clock() {
    if (cycles == 0) {
        uint8_t opcode = read(PC++);
        Instruction& instr = lookup[opcode];
        cycles = instr.cycles;
        (this->*instr.addrmode)();
        uint8_t extra1 = (this->*instr.operate)(); // some return extra cycle flags
        cycles += extra1; // if both addrmode and operate signal a boundary cross
    }
    cycles--;
}
