// Emulate a Taito Space Invaders arcade cabinet.
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// for documentation on all the hardware emulated here.

#include <iostream>
#include <thread>
#include <cassert>

#include "i8080/i8080_opcodes.h"
#include "emu.hpp"


#define MACHINE static_cast<machine*>(cpu->udata)

static i8080_word_t cpu_mem_read(i8080* cpu, i8080_addr_t addr) {
    return MACHINE->mem[addr];
}

static void cpu_mem_write(i8080* cpu, i8080_addr_t addr, i8080_word_t word) {
    MACHINE->mem[addr] = word;
}

static i8080_word_t cpu_intr_read(i8080* cpu) {
    return MACHINE->intr_opcode;
}

static i8080_word_t cpu_io_read(i8080* cpu, i8080_word_t port)
{
    machine* m = MACHINE;
    switch (port)
    {
    case 0: return 0b00001110; // unused by ROM
    case 1: return m->in_port1;
    case 2: return m->in_port2;

    case 3: // read word, offset from MSB
        return i8080_word_t(m->shiftreg >> (8 - m->shiftreg_off));

    default: 
        std::fprintf(stderr, 
            "Warning: IO read from unmapped port %d\n", int(port));
        return 0;
    }
}

static void cpu_io_write(i8080* cpu, i8080_word_t port, i8080_word_t word)
{
    machine* m = MACHINE;
    switch (port)
    {
    case 3: case 5:
        std::printf("Sounds are unimplemented\n");
        break;

    case 2:
        m->shiftreg_off = (word & 0x7);
        break;

    case 4: // do shift (from MSB)
        m->shiftreg >>= 8;
        m->shiftreg |= (i8080_dword_t(word) << 8);
        break;

    case 6: break; // ignore

    default:
        std::fprintf(stderr,
            "Warning: IO write to unmapped port %d\n", int(port));
        break;
    }
}

static int read_file(const fs::path& path, i8080_word_t* mem, unsigned size)
{
    DECL_UTF8PATH_STR(path);

    scopedFILE file = SAFE_FOPEN(path.c_str(), "rb");
    if (!file) {
        return mERROR("Could not open file %s", path_str);
    }
    if (std::fread(mem, 1, size, file.get()) != size) {
        return mERROR("Could not read %u bytes from file %s", size, path_str);
    }
    std::fgetc(file.get()); // set eof
    if (!std::feof(file.get())) {
        return mERROR("%s is larger than %u bytes", path_str, size);
    }
    return 0;
}

int emulator::read_rom(const fs::path& path)
{
    if (fs::is_directory(path)) 
    {
        std::printf("Loading ROM from invaders.efgh...\n");
        int e;
        e = read_file(path / "invaders.h", &m.mem[0],    2048); if (e) { return e; }
        e = read_file(path / "invaders.g", &m.mem[2048], 2048); if (e) { return e; }
        e = read_file(path / "invaders.f", &m.mem[4096], 2048); if (e) { return e; }
        e = read_file(path / "invaders.e", &m.mem[6144], 2048); if (e) { return e; }
        return 0;
    }
    else { return read_file(path, m.mem.get(), 8192); }
}

int emulator::init_graphics(unsigned scresX, unsigned scresY)
{
    if (scresX % SCREEN_NATIVERES_X != 0 ||
        scresY % SCREEN_NATIVERES_Y != 0 ||
        (scresX / SCREEN_NATIVERES_X) != (scresY / SCREEN_NATIVERES_Y)) {
        return mERROR("Screen res is not a multiple of native res (%ux%u)",
            SCREEN_NATIVERES_X, SCREEN_NATIVERES_Y);
    }
    m_scalefac = scresX / SCREEN_NATIVERES_X;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        return mERROR("SDL_Init(): %s\n", SDL_GetError());
    }

    m_window = SDL_CreateWindow("Space Invaders",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, scresX, scresY, SDL_WINDOW_HIDDEN);
    if (!m_window) {
        return mERROR("SDL_CreateWindow(): %s\n", SDL_GetError());
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        return mERROR("SDL_CreateRenderer(): %s\n", SDL_GetError());
    }

    // graphics drivers prefer ARGB8888, other formats may or may not work
    // https://stackoverflow.com/questions/56143991/
    m_screentex = SDL_CreateTexture(m_renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, scresX, scresY);
    if (!m_screentex) {
        return mERROR("SDL_CreateTexture(): %s", SDL_GetError());
    }

    return 0;
}

emulator::emulator(const fs::path& rom_path, unsigned scresX, unsigned scresY) :
    m_ok(false)
{
    m.cpu.mem_read = cpu_mem_read;
    m.cpu.mem_write = cpu_mem_write;
    m.cpu.io_read = cpu_io_read;
    m.cpu.io_write = cpu_io_write;
    m.cpu.intr_read = cpu_intr_read;
    m.cpu.udata = &m;
    m.cpu.reset();

    m.in_port1 = 0b00001000;
    m.in_port2 = 0;
    m.shiftreg = 0;
    m.shiftreg_off = 0;
    m.intr_opcode = i8080_NOP;

    std::printf("Reset machine\n");

    m.mem = std::make_unique<i8080_word_t[]>(65536);
    if (read_rom(rom_path) != 0) { return; }
    std::printf("Loaded ROM\n");

    if (init_graphics(scresX, scresY) != 0) { return; }
    std::printf("Initialized graphics\n");

    m_ok = true;
}

void emulator::input_handler(SDL_Scancode sc, bool pressed)
{
    switch (sc)
    {
    case KEY_CREDIT:   set_bit(&m.in_port1, 0, pressed); break;
    case KEY_2P_START: set_bit(&m.in_port1, 1, pressed); break;
    case KEY_1P_START: set_bit(&m.in_port1, 2, pressed); break;

    case KEY_P1_FIRE:  set_bit(&m.in_port1, 4, pressed); break;
    case KEY_P1_LEFT:  set_bit(&m.in_port1, 5, pressed); break;
    case KEY_P1_RIGHT: set_bit(&m.in_port1, 6, pressed); break;

    case KEY_P2_FIRE:  set_bit(&m.in_port2, 4, pressed); break;
    case KEY_P2_LEFT:  set_bit(&m.in_port2, 5, pressed); break;
    case KEY_P2_RIGHT: set_bit(&m.in_port2, 6, pressed); break;

    default: break;
    }
}

void emulator::run()
{
    SDL_ShowWindow(m_window);

    // 100ns resolution on Windows
    using clk = std::chrono::steady_clock;

    bool running = true;
    while (running)
    {

        auto tbeg = clk::now();

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                input_handler(e.key.keysym.scancode, true);
                break;

            case SDL_KEYUP:
                input_handler(e.key.keysym.scancode, false);
                break;

            default: break;
            }
        }
        
        uint64_t last_cycles = m.cpu.cycles;

        while (m.cpu.step() == 0 && m.cpu.cycles - last_cycles < 14286); // 96 lines
        m.intr_opcode = i8080_RST_1;
        m.cpu.interrupt();

        //last_cycles = m.cpu.cycles;

        while (m.cpu.step() == 0 && m.cpu.cycles - last_cycles < 33333); // 224 lines
        m.intr_opcode = i8080_RST_2;
        m.cpu.interrupt();


        uint32_t* pixels; int pitch;
        SDL_LockTexture(m_screentex, NULL, (void**)&pixels, &pitch);
        assert(pitch == 896);

        i8080_word_t* VRAM_start = &m.mem[0x2400];
        
        uint VRAM_idx = 0;
        for (uint x = 0; x < SCREEN_NATIVERES_X; ++x) {
            for (uint y = 0; y < SCREEN_NATIVERES_Y; y += 8) 
            {
                i8080_word_t word = VRAM_start[VRAM_idx++];

                for (int bit = 0; bit < 8; ++bit)
                {
                    uint idx = SCREEN_NATIVERES_X * (SCREEN_NATIVERES_Y - y - bit - 1) + x;
                    pixels[idx] = 
                        ((word & (0x1 << bit)) == 0) ? 0xFF000000 : 0xFFFFFFFF;
                }
            }
        }

        SDL_UnlockTexture(m_screentex);
        SDL_RenderCopy(m_renderer, m_screentex, NULL, NULL);
        

        //while (frame_clk::now())
        //{
        //    // Interrupts are generated at 120Hz (twice per frame).
        //    // At 2Mhz, the CPU will run for ~16667 cycles between interrupts.
        //
        //    // Run CPU in 700 cycle bursts.
        //    // 700 cycles at 2MHz is 350us, which means at worst we're
        //    // under 5% 
        //    // Want interrupt timing as close to correct as possible.
        //    // The 
        //
        //    
        //}

        //const uint16_t kVideoRamStart = 0x2400;
        //
        //int i = 0;
        //const int kVideoRamWidth = 224;
        //const int kVideoRamHeight = 256;
        //
        //for (int ix = 0; ix < kVideoRamWidth; ix++) {
        //    for (int iy = 0; iy < kVideoRamHeight; iy += 8) {
        //        uint8_t byte = m.mem[kVideoRamStart + i++];
        //
        //        for (int b = 0; b < 8; b++) {
        //            if ((byte & 0x1) == 0x1)
        //                SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
        //            else
        //                SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        //
        //            SDL_RenderDrawPoint(m_renderer, ix, (kVideoRamHeight - (iy + b)));
        //            byte >>= 1;
        //        }
        //    }
        //}


        SDL_RenderPresent(m_renderer);

        auto tend = clk::now();
        
        std::this_thread::sleep_for(std::chrono::microseconds(16667) - (tend - tbeg));
    }
}

emulator::~emulator()
{
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

//void disassemble()
//{
//    scopedFILE file = SAFE_FOPENA("disassembly.asm", "wb");
//
//    while (dat.cpu.pc < 0x2000) {
//        dat.cpu.disassemble(file.get());
//    }
//}