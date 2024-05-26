// 
// Emulate an Intel 8080 microprocessor.
// All instructions are supported.
// 
// Example usage:
//
// void example(uint64_t cycles_to_run) 
// {
//     i8080 cpu;
//     cpu.mem_read = my_mem_read_cb;
//     cpu.mem_write = my_mem_write_cb;
//     cpu.io_read = my_io_read_cb;      // optional
//     cpu.io_write = my_io_write_cb;    // optional
//     cpu.intr_read = my_intr_read_cb;  // optional
//     
//     cpu.reset();
//     
//     while (cpu.step() == 0 && cpu.cycles < cycles_to_run) {
//         continue;
//     }
//     printf("Done!");
// }
//


#ifndef I8080_HPP
#define I8080_HPP

#include <cstdint>

using i8080_word_t = std::uint8_t;
using i8080_addr_t = std::uint16_t;
using i8080_dword_t = std::uint16_t; // reg pairs (eg. BC/DE/HL)

struct i8080
{
    // Working registers
    i8080_word_t a, b, c, d, e, h, l;

    i8080_addr_t sp; // Stack pointer
    i8080_addr_t pc; // Program counter

    // Flags
    i8080_word_t s;  // Sign
    i8080_word_t z;  // Zero
    i8080_word_t cy; // Carry
    i8080_word_t ac; // Aux carry
    i8080_word_t p;  // Parity
    i8080_word_t inte; // Interrupt enable
    i8080_word_t intr; // Interrupt request
    i8080_word_t halt; // HALT state

    // Clock cycles elapsed since last reset
    std::uint64_t cycles;

    // ---------- user-defined callbacks --------------

    i8080_word_t(*mem_read)(i8080*, i8080_addr_t addr);
    void(*mem_write)(i8080*, i8080_addr_t addr, i8080_word_t word);

    i8080_word_t(*io_read)(i8080*, i8080_word_t port);
    void(*io_write)(i8080*, i8080_word_t port, i8080_word_t word);

    i8080_word_t(*intr_read)(i8080*);

    void* udata;

    // ------------------------------------------------

    // Reset chip. Eq. to low on RESET pin.
    void reset();

    // Send an interrupt request.
    // If interrupts are enabled, the next call to 
    // step() will invoke intr_read() and execute
    // the returned opcode.
    void interrupt();

    // Run the next instruction.
    // Returns 0 on success, or -1 for 
    // missing IO/intr callback.
    int step();
};

#endif /* I8080_H */
