// 
// Emulator for the Intel 8080 microprocessor.
//
// Ported to C++ from my ANSI C library:
// (https://github.com/mayawarrier/intel8080-emulator)
// 
// Example usage:
//
// void run_8080(uint64_t num_clk_cycles) 
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
//     while (cpu.step() == 0 && cpu.cycles < num_clk_cycles) {
//         // some code
//         // call cpu.interrupt() here
//     }
//     printf("Done!");
// }
//

#ifndef I8080_HPP
#define I8080_HPP

#include <cstdint>
#include <cstdio>

using i8080_word_t = std::uint8_t;
using i8080_addr_t = std::uint16_t;
using i8080_dword_t = std::uint16_t; // reg pairs (eg. BC/DE/HL)

struct i8080
{
    // Working registers
    i8080_word_t a, b, c, d, e, h, l;

    i8080_addr_t sp; // Stack pointer
    i8080_addr_t pc; // Program counter

    i8080_word_t int_rq; // Interrupt request

    // Flags
    i8080_word_t s : 1;  // Sign
    i8080_word_t z : 1;  // Zero
    i8080_word_t cy : 1; // Carry
    i8080_word_t ac : 1; // Aux carry
    i8080_word_t p : 1;  // Parity

    i8080_word_t halt : 1; // In HALT state?

    i8080_word_t int_en : 1; // Interrupts enabled (INTE pin)  
    i8080_word_t int_ff : 1; // Interrupt latch

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

    // Run one instruction.
    // Returns 0 on success, or -1 for 
    // missing IO/intr callback.
    int step();

    // Disassemble one instruction.
    // This can be called before i8080_step() to print the
    // instruction that is about to be executed.
    // It can also be called in a loop to disassemble a section of memory.
    // Returns 0 on success.
    void disassemble(std::FILE* os);

    // Send an interrupt request.
    // If interrupts are enabled, intr_read() will be
    // invoked by step() and the returned opcode 
    // will be executed.
    void interrupt();
};

#endif /* I8080_H */
