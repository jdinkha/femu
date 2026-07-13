struct CPU {
    Bus* bus;
    uint16_t addr_abs = 0;   // resolved address for current instruction
    uint8_t  addr_rel = 0;   // for branch instructions
    bool     implied = false;

    // Example addressing modes
    void IMM() { addr_abs = PC++; }                     // immediate: operand is next byte
    void ZP0() { addr_abs = read(PC++) & 0x00FF; }       // zero page
    void ZPX() { addr_abs = (read(PC++) + X) & 0x00FF; } // zero page indexed by X
    void ABS() {
        uint16_t lo = read(PC++);
        uint16_t hi = read(PC++);
        addr_abs = (hi << 8) | lo;
    }
    void ABX() {
        uint16_t lo = read(PC++);
        uint16_t hi = read(PC++);
        addr_abs = ((hi << 8) | lo) + X;
        // note: if page boundary crossed, add 1 extra cycle — check this!
    }
    // ... IZX, IZY, IND, REL, ACC, IMP etc.
};
