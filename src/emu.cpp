//
// Emulate a Taito Space Invaders arcade machine.
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// for documentation on everything emulated here.
//

#include <chrono>
#include <thread>
#include <string_view>
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
    case 0: return 0x0e; // unused by ROM
    case 1: return m->in_port1;
    case 2: return m->in_port2;

    case 3: // read word, offset from MSB
        return i8080_word_t(m->shiftreg >> (8 - m->shiftreg_off));

    default: 
        pWARNING("IO read from unmapped port %d", int(port));
        return 0;
    }
}

// looping sounds repeat until the pin goes off, 
// non-looping sounds are played in rapid succession
static bool snd_is_looping(int idx)
{
    return idx == 0 || idx == 9;
}

static void handle_sound(machine* m, int idx, bool pin_on)
{
    // debounce to avoid echo
    if (pin_on) {
        if (!m->snd_playing[idx]) {
            Mix_PlayChannel(idx, m->sounds[idx], snd_is_looping(idx) ? -1 : 0);
            m->snd_playing[idx] = true;
        }
    } else {
        if (snd_is_looping(idx)) {
            Mix_HaltChannel(idx);
        }
        m->snd_playing[idx] = false; 
    }
}

static void cpu_io_write(i8080* cpu, i8080_word_t port, i8080_word_t word)
{
    machine* m = MACHINE;
    switch (port)
    {
    case 2:
        m->shiftreg_off = (word & 0x7);
        break;

    case 4: // do shift (from MSB)
        m->shiftreg >>= 8;
        m->shiftreg |= (i8080_dword_t(word) << 8);
        break;

    case 3: 
        for (int i = 0; i < 4; ++i) {
            handle_sound(m, i, get_bit(word, i));
        }
        handle_sound(m, 9, get_bit(word, 4));
        break;

    case 5:
        for (int i = 0; i < 5; ++i) {
            handle_sound(m, i + 4, get_bit(word, i));
        }
        break;

        // Ignore watchdog port (intended to reset
        // the machine if a hardware issue occurs).
    case 6: break;

    default:
        pWARNING("IO write to unmapped port %d", int(port));
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

int emulator::read_rom(const fs::path& dir)
{
    if (fs::exists(dir / "invaders.rom")) {
        int e = read_file(dir / "invaders.rom", m.mem.get(), 8192);
        if (e) { return e; }
        std::printf("Loaded invaders.rom\n");
        return 0;
    }
    else {
        int e;
        e = read_file(dir / "invaders.h", &m.mem[0], 2048); if (e) { return e; }
        e = read_file(dir / "invaders.g", &m.mem[2048], 2048); if (e) { return e; }
        e = read_file(dir / "invaders.f", &m.mem[4096], 2048); if (e) { return e; }
        e = read_file(dir / "invaders.e", &m.mem[6144], 2048); if (e) { return e; }
        std::printf("Loaded ROM from invaders.efgh\n");
        return 0;
    }
}

enum palidx : uint8_t
{
    PALIDX_BLACK,
    PALIDX_GREEN,
    PALIDX_RED,
    PALIDX_WHITE,
};

static const std::array<fmt_palette, 3> PIXFMT_2PALETTE = {
                                         // black,      green,      red,        white    
    fmt_palette(SDL_PIXELFORMAT_RGB565,   { 0x00000000, 0x1FE3,     0xF8E3,     0xFFFF,    }),
    fmt_palette(SDL_PIXELFORMAT_XRGB8888, { 0x00000000, 0x001EFE1E, 0x00FE1E1E, 0x00FFFFFF }),
    fmt_palette(SDL_PIXELFORMAT_ARGB8888, { 0xFF000000, 0xFF1EFE1E, 0xFFFE1E1E, 0xFFFFFFFF })
};

static const char* pixfmt_name(uint32_t fmt)
{
    std::string_view str(SDL_GetPixelFormatName(fmt));
    if (str.starts_with("SDL_PIXELFORMAT_")) {
        str.remove_prefix(sizeof("SDL_PIXELFORMAT_") - 1);
    }
    return str.data();
}

int emulator::init_graphics(uint scresX, uint scresY)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return mERROR("SDL_Init(): %s", SDL_GetError());
    }

    m_window = SDL_CreateWindow("Space Invaders",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, scresX, scresY, SDL_WINDOW_HIDDEN);
    if (!m_window) {
        return mERROR("SDL_CreateWindow(): %s", SDL_GetError());
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        return mERROR("SDL_CreateRenderer(): %s", SDL_GetError());
    }

    // use first pixel format available 
    // see https://stackoverflow.com/questions/56143991/
    std::string triedfmts;
    for (auto& fmt : PIXFMT_2PALETTE)
    {
        m_screentex = SDL_CreateTexture(m_renderer,
            fmt.first, SDL_TEXTUREACCESS_STREAMING, scresX, scresY);
        if (!m_screentex) { continue; }

        uint32_t gotfmt;
        if (SDL_QueryTexture(m_screentex, &gotfmt, NULL, NULL, NULL) != 0) {
            return mERROR("SDL_QueryTexture(): %s", SDL_GetError());
        }
        if (gotfmt != fmt.first) {
            SDL_DestroyTexture(m_screentex);
            m_screentex = NULL;
            if (!triedfmts.empty()) { triedfmts += ", "; }
            triedfmts += pixfmt_name(fmt.first);
        }
        else {
            std::printf("Using pixel format %s\n", pixfmt_name(fmt.first));
            m_fmtpalette = &fmt;
            break;
        }
    }
    if (!m_screentex) {
        return mERROR("Could not create texture, "
            "tried formats: %s", triedfmts.c_str());
    }

    std::printf("Initialized graphics\n");
    return 0;
}

int emulator::init_audio(const fs::path& audio_dir) 
{
    // chunksize is small to reduce latency
    if (Mix_OpenAudio(11025, AUDIO_U8, 1, 512) != 0) {
        return mERROR("Mix_OpenAudio(): %s", Mix_GetError());
    }
    if (Mix_AllocateChannels(NUM_SOUNDS) != NUM_SOUNDS) {
        return mERROR("Mix_AllocateChannels(): %s", Mix_GetError());
    }

    const char* audio_fnames[] = {
        "0.wav", "1.wav", "2.wav", "3.wav", "4.wav",
        "5.wav", "6.wav", "7.wav", "8.wav", "9.wav"
    };
    int num_loaded = 0;
    for (int i = 0; i < NUM_SOUNDS; ++i)
    {
        fs::path path = audio_dir / audio_fnames[i];
        DECL_UTF8PATH_STR(path)
    
        m.sounds[i] = Mix_LoadWAV(path_str);
        m.snd_playing[i] = false;

        if (!m.sounds[i]) {
            pWARNING("Could not open %s: %s", path_str, Mix_GetError());
        }
        num_loaded += (m.sounds[i] != NULL);
    }

    // adjust volume
    Mix_Volume(0, MIX_MAX_VOLUME / 2); // UFO fly
    Mix_Volume(8, MIX_MAX_VOLUME / 2); // UFO die
    Mix_Volume(3, MIX_MAX_VOLUME / 2); // Alien die
    Mix_Volume(1, MIX_MAX_VOLUME / 2); // Shoot

    if (num_loaded == NUM_SOUNDS) {
        std::printf("Initialized audio\n");
    } else {
        pWARNING("Some audio files could not be loaded");
    }
    return 0;
}

emulator::emulator(const fs::path& romdir, uint scalefac) :
    m_scalefac(scalefac), m_scresX(SCREEN_NATIVERES_X * scalefac), m_ok(false)
{
    m.cpu.mem_read = cpu_mem_read;
    m.cpu.mem_write = cpu_mem_write;
    m.cpu.io_read = cpu_io_read;
    m.cpu.io_write = cpu_io_write;
    m.cpu.intr_read = cpu_intr_read;
    m.cpu.udata = &m;
    m.cpu.reset();

    m.in_port1 = 0b00001000;
    m.in_port2 = 0b00001000;
    m.shiftreg = 0;
    m.shiftreg_off = 0;
    m.intr_opcode = i8080_NOP;

    std::printf("Initialized machine\n");

    m.mem = std::make_unique<i8080_word_t[]>(65536);
    if (read_rom(romdir) != 0) { 
        return; 
    }

    uint scresX = SCREEN_NATIVERES_X * scalefac;
    uint scresY = SCREEN_NATIVERES_Y * scalefac;
    if (init_graphics(scresX, scresY) != 0) {
        return; 
    }
    if (init_audio(romdir) != 0) { 
        return; 
    }
    m_ok = true;
}

void emulator::handle_input(SDL_Scancode sc, bool pressed)
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

// Run CPU for one frame.
void emulator::gen_frame(uint64_t& nframes, uint64_t& last_cpucycles)
{
    // 33333.33 clk cycles at CPU's 2Mhz clock speed (16667us/0.5us)
    uint64_t frame_cycles = 33333 + (nframes % 3 == 0);

    // run till mid-screen interrupt 
    // 14286 = (96/224) * (16667us/0.5us)
    while (m.cpu.cycles - last_cpucycles < 14286) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_1;
    m.cpu.interrupt();

    // run till end of screen (VLANK) interrupt
    while (m.cpu.cycles - last_cpucycles < frame_cycles) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_2;
    m.cpu.interrupt();

    // adjust extra cycles in next frame
    last_cpucycles += frame_cycles;
    nframes++;
}

// Pixel color after gel overlay
// https://tcrf.net/images/a/af/SpaceInvadersArcColorUseTV.png
static palidx pixel_color(uint x, uint y, bool pixel)
{  
    if (!pixel) { return PALIDX_BLACK; }

    if ((y <= 15 && x > 24 && x < 136) || (y > 15 && y < 71)) {
        return PALIDX_GREEN;
    }
    else if (y >= 192 && y < 223) {
        return PALIDX_RED;
    }
    else { return PALIDX_WHITE; }
}

void emulator::render_frame() const
{
    void* pixels; int pitch;
    SDL_LockTexture(m_screentex, NULL, &pixels, &pitch);

    uint VRAM_idx = 0;
    i8080_word_t* VRAM_start = &m.mem[0x2400];

    // Unpack (8 on/off pixels per byte) and rotate counter-clockwise
    for (uint x = 0; x < SCREEN_NATIVERES_X; ++x) {
        for (uint y = 0; y < SCREEN_NATIVERES_Y; y += 8)
        {
            i8080_word_t word = VRAM_start[VRAM_idx++];

            for (int bit = 0; bit < 8; ++bit)
            {
                palidx colidx = pixel_color(x, y, get_bit(word, bit));
                uint32_t color = m_fmtpalette->second[colidx];

                uint st_idx = m_scalefac * (m_scresX * (SCREEN_NATIVERES_Y - y - bit - 1) + x);

                // Draw a square instead of a pixel if resolution is higher than native
                for (uint xsqr = 0; xsqr < m_scalefac; ++xsqr) {
                    for (uint ysqr = 0; ysqr < m_scalefac; ++ysqr)
                    {
                        uint idx = st_idx + m_scresX * ysqr + xsqr;

                        switch (m_fmtpalette->first)
                        {
                        case SDL_PIXELFORMAT_RGB565:
                            static_cast<uint16_t*>(pixels)[idx] = uint16_t(color);
                            break;
                        case SDL_PIXELFORMAT_XRGB8888:
                        case SDL_PIXELFORMAT_ARGB8888:
                            static_cast<uint32_t*>(pixels)[idx] = color;
                            break;
                        }
                    }
                }
            }
        }
    }

    SDL_UnlockTexture(m_screentex);
    SDL_RenderCopy(m_renderer, m_screentex, NULL, NULL);
    SDL_RenderPresent(m_renderer);
}

void emulator::run()
{
    SDL_ShowWindow(m_window);

    uint64_t nframes = 0;
    uint64_t last_cpucycles = 0;

    // 100ns resolution on Windows
    using clk = std::chrono::steady_clock;

    clk::time_point t_start = clk::now();

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                handle_input(e.key.keysym.scancode, true);
                break;
            case SDL_KEYUP:
                handle_input(e.key.keysym.scancode, false);
                break;

            default: break;
            }
        }

        gen_frame(nframes, last_cpucycles);
        render_frame();

        // Vsync at 60fps (or as close to it as possible)
        constexpr auto framedur_60fps = std::chrono::microseconds(16667);
        auto framedur = clk::now() - t_start;
        if (framedur < framedur_60fps) {
            std::this_thread::sleep_for(framedur_60fps - framedur);
        }
        
        auto t_laststart = t_start;
        t_start = clk::now();

        float fps = 1.f / std::chrono::duration<float>(t_start - t_laststart).count();
        std::printf("\rFPS: %ld", std::lroundf(fps));
    }
}

emulator::~emulator()
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        Mix_FreeChunk(m.sounds[i]);
    }
    Mix_CloseAudio();
    SDL_DestroyTexture(m_screentex);
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}
