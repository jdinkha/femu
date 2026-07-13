struct CPU {
    uint8_t  A = 0;      // Accumulator
    uint8_t  X = 0;      // Index X
    uint8_t  Y = 0;      // Index Y
    uint8_t  SP = 0xFD;  // Stack pointer (grows downward, stack is $0100-$01FF)
    uint16_t PC = 0;     // Program counter
    uint8_t  status = 0x24; // Processor status flags

    // Flag bits (status register)
    enum Flag : uint8_t {
        C = 1 << 0, // Carry
        Z = 1 << 1, // Zero
        I = 1 << 2, // Interrupt disable
        D = 1 << 3, // Decimal (unused on NES, but flag still exists)
        B = 1 << 4, // Break
        U = 1 << 5, // Unused, always 1
        V = 1 << 6, // Overflow
        N = 1 << 7  // Negative
    };

    void setFlag(Flag f, bool v) { if (v) status |= f; else status &= ~f; }
    bool getFlag(Flag f) const   { return status & f; }
};
