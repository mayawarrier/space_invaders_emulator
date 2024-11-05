//
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// to learn about the hardware inside the Space Invaders arcade machine,
// and to gain some context for what is emulated here.
// 
// See i8080/ for the bulk of the emulation.
//

#include <cmath>
#include <string_view>

#include "i8080/i8080_opcodes.h"
#include "gui.hpp"
#include "emu.hpp"

#ifdef _WIN32
#include "win32.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/val.h>
#include <functional>
#endif


static const char* input_ininame(inputtype type)
{
    switch (type)
    {
    case INPUT_P1_LEFT:  return "InputP1Left";
    case INPUT_P1_RIGHT: return "InputP1Right";
    case INPUT_P1_FIRE:  return "InputP1Fire";
    case INPUT_P2_LEFT:  return "InputP2Left";
    case INPUT_P2_RIGHT: return "InputP2Right";
    case INPUT_P2_FIRE:  return "InputP2Fire";
    case INPUT_CREDIT:   return "InputCredit";
    case INPUT_1P_START: return "Input1PStart";
    case INPUT_2P_START: return "Input2PStart";
    default:
        return nullptr;
    }
}

static const char* input_inidesc(inputtype type)
{
    switch (type)
    {
    case INPUT_P1_LEFT:  return "Key for P1 left.";
    case INPUT_P1_RIGHT: return "Key for P1 right.";
    case INPUT_P1_FIRE:  return "Key for P1 fire.";
    case INPUT_P2_LEFT:  return "Key for P2 left.";
    case INPUT_P2_RIGHT: return "Key for P2 right.";
    case INPUT_P2_FIRE:  return "Key for P2 fire.";
    case INPUT_CREDIT:   return "Key to insert coin.";
    case INPUT_1P_START: return "Key to start 1 player mode.";
    case INPUT_2P_START: return "Key to start 2 player mode.";
    default:
        return nullptr;
    }
}

static SDL_Scancode input_dflt_key(inputtype type)
{
    switch (type)
    {
    case INPUT_P1_LEFT:  return SDL_SCANCODE_LEFT;
    case INPUT_P1_RIGHT: return SDL_SCANCODE_RIGHT;
    case INPUT_P1_FIRE:  return SDL_SCANCODE_SPACE;
    case INPUT_P2_LEFT:  return SDL_SCANCODE_LEFT;
    case INPUT_P2_RIGHT: return SDL_SCANCODE_RIGHT;
    case INPUT_P2_FIRE:  return SDL_SCANCODE_SPACE;
    case INPUT_CREDIT:   return SDL_SCANCODE_RETURN;
    case INPUT_1P_START: return SDL_SCANCODE_1;
    case INPUT_2P_START: return SDL_SCANCODE_2;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

void emu::print_ini_help()
{
    static constexpr char CONFIG_FMTSTR[] = "  %-18s%-9s%s\n";
    std::printf(" Config file parameters\n");

    std::printf(CONFIG_FMTSTR, "Volume", "(0-100)", "Set SFX volume.");
    std::printf(CONFIG_FMTSTR, "DIP3", "(0/1)", "Set DIP switch 3. See README.");
    std::printf(CONFIG_FMTSTR, "DIP4", "(0/1)", "Set DIP switch 4. See README.");
    std::printf(CONFIG_FMTSTR, "DIP5", "(0/1)", "Set DIP switch 5. See README.");
    std::printf(CONFIG_FMTSTR, "DIP6", "(0/1)", "Set DIP switch 6. See README.");
    std::printf(CONFIG_FMTSTR, "DIP7", "(0/1)", "Set DIP switch 7. See README.");

    for (int i = 0; i < INPUT_NUM_INPUTS; ++i) {
        std::printf(CONFIG_FMTSTR, input_ininame(inputtype(i)), "(<key>)", input_inidesc(inputtype(i)));
    }
}

// Recompute window/GUI/viewport sizes.
// Window and GUI must be set up first.
int emu::resize_window()
{
    int disp_idx = SDL_GetWindowDisplayIndex(m_window);
    if (disp_idx < 0) {
        logERROR("SDL_GetWindowDisplayIndex(): %s", SDL_GetError());
        return -1;
    }
    SDL_Rect disp_bounds;
    if (SDL_GetDisplayUsableBounds(disp_idx, &disp_bounds) != 0) {
        logERROR("SDL_GetDesktopDisplayMode(): %s", SDL_GetError());
        return -1;
    }

    auto& vp = m_viewportrect;

    uint vp_offsetY = m_gui ? m_gui->menubar_height() : 0;
    uint vp_maxX = disp_bounds.w;
    uint vp_maxY = uint((is_emscripten() ? 0.95 : 0.9) * disp_bounds.h) - vp_offsetY;

    // Resize viewport
    if (is_emscripten() ||
        (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN))
    {
        // max resolution maintaining aspect ratio
        uint scaledX = uint(vp_maxY * float(RES_NATIVE_X) / RES_NATIVE_Y);
        if (scaledX <= vp_maxX) {
            vp.w = scaledX;
            vp.h = vp_maxY;
        } else {
            vp.w = vp_maxX;
            vp.h = uint(vp_maxX * float(RES_NATIVE_Y) / RES_NATIVE_X);
        }
    }
    else {
        // max discrete multiple of native resolution
        uint max_factorX = vp_maxX / RES_NATIVE_X;
        uint max_factorY = vp_maxY / RES_NATIVE_Y;
        uint factor = std::min(max_factorX, max_factorY);
        vp.w = RES_NATIVE_X * factor;
        vp.h = RES_NATIVE_Y * factor;
    }
    vp.x = 0;
    vp.y = vp_offsetY;

    int sizeX = vp.w + (m_gui ? m_gui->panel_size() : 0);
    int sizeY = vp.h + vp_offsetY;

    SDL_SetWindowSize(m_window, sizeX, sizeY);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if constexpr (!is_emscripten()) { // too frequent on emscripten
        logMESSAGE("-- Viewport bounds: x: %d, y: %d, w: %d, h: %d", vp.x, vp.y, vp.w, vp.h);
        logMESSAGE("-- Window size: x: %d, y: %u", vp.w, vp.h + vp_offsetY);
    }
    return 0;
}

#ifdef __EMSCRIPTEN__
bool emcc_on_window_resize(int, const EmscriptenUiEvent*, void* udata)
{
    int err = static_cast<emu*>(udata)->resize_window();
    return !err;
}
#endif

// like a palette, same order as PIXFMTS
enum colr_idx : uint8_t
{
    COLRIDX_BLACK,
    COLRIDX_GREEN,
    COLRIDX_RED,
    COLRIDX_WHITE,
};

static const std::array<pix_fmt, 3> PIXFMTS = {
                                     // black,      green,      red,        white
    pix_fmt(SDL_PIXELFORMAT_BGR565,   { 0x0000,     0x1FE3,     0x18FF,     0xFFFF,    }),
    pix_fmt(SDL_PIXELFORMAT_ARGB8888, { 0xFF000000, 0xFF1EFE1E, 0xFFFE1E1E, 0xFFFFFFFF }),
    pix_fmt(SDL_PIXELFORMAT_ABGR8888, { 0xFF000000, 0xFF1EFE1E, 0xFF1E1EFE, 0xFFFFFFFF }),
};

static const char* pixfmt_name(uint32_t fmt)
{
    std::string_view str(SDL_GetPixelFormatName(fmt));
    if (str.starts_with("SDL_PIXELFORMAT_")) {
        str.remove_prefix(sizeof("SDL_PIXELFORMAT_") - 1);
    }
    return str.data();
}

// Get first supported texture format
// see https://stackoverflow.com/questions/56143991/
static int get_pixfmt(SDL_RendererInfo& rendinfo, const pix_fmt*& out_pixfmt)
{
    for (auto& pixfmt : PIXFMTS) {
        for (uint32_t i = 0; i < rendinfo.num_texture_formats; ++i)
        {
            if (rendinfo.texture_formats[i] == pixfmt.fmt) {
                logMESSAGE("-- Texture format: %s", pixfmt_name(pixfmt.fmt));
                out_pixfmt = &pixfmt;
                goto done;
            }
        }
    }
done:
    if (!out_pixfmt)
    {
        std::string suppfmts;
        for (auto& pixfmt : PIXFMTS) {
            if (!suppfmts.empty()) { suppfmts += ", "; }
            suppfmts += pixfmt_name(pixfmt.fmt);
        }
        std::string hasfmts;
        for (uint32_t i = 0; i < rendinfo.num_texture_formats; ++i) {
            if (!hasfmts.empty()) { hasfmts += ", "; }
            hasfmts += pixfmt_name(rendinfo.texture_formats[i]);
        }

        logERROR("Could not find a supported texture format.\n"
            "Supported: %s\nAvailable: %s", suppfmts.c_str(), hasfmts.c_str());
        return -1;
    }
    return 0;
}

int emu::init_graphics(bool enable_ui, bool windowed)
{
    logMESSAGE("Initializing graphics");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        logERROR("SDL_Init(): %s", SDL_GetError());
        return -1;
    }

    // prevents freezes and lag on Windows
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif

    m_window = SDL_CreateWindow("Space Invaders", 0, 0, 0, 0, SDL_WINDOW_HIDDEN);
    if (!m_window) {
        logERROR("SDL_CreateWindow(): %s", SDL_GetError());
        return -1;
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (!m_renderer) {
        logERROR("SDL_CreateRenderer(): %s", SDL_GetError());
        return -1;
    }

    if (enable_ui) {
        m_gui = std::make_unique<emu_gui>(this);
        if (!m_gui->ok()) { return -1; }
    }

    if (!windowed) {
        //todo. doesn't currently work
        //SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);
    }

    // Set viewport and window size etc.
    int err = resize_window();
    if (err) { return err; }

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_RESULT res = emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false, emcc_on_window_resize);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        logERROR("Failed to set window resize callback");
        return -1;
    }
#endif

    SDL_RendererInfo rendinfo;
    if (SDL_GetRendererInfo(m_renderer, &rendinfo) != 0) {
        logERROR("SDL_GetRendererInfo(): %s", SDL_GetError());
        return -1;
    }
    logMESSAGE("-- Render backend: %s", rendinfo.name);

    err = get_pixfmt(rendinfo, m_pixfmt);
    if (err) { return err; }
    
    m_viewporttex = SDL_CreateTexture(m_renderer, m_pixfmt->fmt, 
        SDL_TEXTUREACCESS_STREAMING, RES_NATIVE_X, RES_NATIVE_Y);
    if (!m_viewporttex) {
        logERROR("SDL_CreateTexture(): %s", SDL_GetError());
        return -1;
    }

    return 0;
}

static const int MAX_VOLUMES[] = 
{
    MIX_MAX_VOLUME / 3, // UFO fly
    MIX_MAX_VOLUME / 2, // Shoot
    MIX_MAX_VOLUME,
    MIX_MAX_VOLUME / 2, // Alien die
    MIX_MAX_VOLUME,
    MIX_MAX_VOLUME,
    MIX_MAX_VOLUME,
    MIX_MAX_VOLUME,
    MIX_MAX_VOLUME / 2, // UFO die
    MIX_MAX_VOLUME,
};

int emu::init_audio(const fs::path& audio_dir) 
{
    logMESSAGE("Initializing audio");

    // chunksize is small to reduce latency
    if (Mix_OpenAudio(11025, AUDIO_U8, 1, 512) != 0) {
        logERROR("Mix_OpenAudio(): %s", Mix_GetError());
        return -1;
    }
    if (Mix_AllocateChannels(NUM_SOUNDS) != NUM_SOUNDS) {
        logERROR("Mix_AllocateChannels(): %s", Mix_GetError());
        return -1;
    }

    static const char* AUDIO_FILENAMES[NUM_SOUNDS][2] =
    {
        {"0.wav", "ufo_highpitch.wav"},
        {"1.wav", "shoot.wav"},
        {"2.wav", "explosion.wav"},
        {"3.wav", "invaderkilled.wav"},
        {"4.wav", "fastinvader1.wav"},
        {"5.wav", "fastinvader2.wav"},
        {"6.wav", "fastinvader3.wav"},
        {"7.wav", "fastinvader4.wav"},
        {"8.wav", "ufo_lowpitch.wav"},
        {"9.wav", "extendedplay.wav"}
    };
    
    int num_loaded = 0;
    for (int i = 0; i < NUM_SOUNDS; ++i)
    {
        m.sounds[i] = nullptr;
        m.sndpin_lastvals[i] = false;

        for (int j = 0; j < 2; ++j) 
        {
            fs::path path = audio_dir / AUDIO_FILENAMES[i][j];
            m.sounds[i] = Mix_LoadWAV(path.string().c_str());
            if (m.sounds[i]) {
                num_loaded++;
                break;
            }
        }
        if (!m.sounds[i]) {
            if constexpr (!is_emscripten()) {
                log_write(stderr, "-- ");
            }
            logWARNING("Audio file %d (aka %s) is missing", i, AUDIO_FILENAMES[i][1]);
        }
    }
    if (num_loaded == NUM_SOUNDS) {
        logMESSAGE("Loaded audio files");
    } else {
        logMESSAGE("Loaded %d/%d audio files", num_loaded, NUM_SOUNDS);
    }

    set_volume(VOLUME_DEFAULT);
    return 0;
}

static int load_file(const fs::path& path, i8080_word_t* mem, unsigned size)
{
    scopedFILE file = SAFE_FOPEN(path.c_str(), "rb");
    if (!file) {
        logERROR("Could not open file %s", path.string().c_str());
        return -1;
    }
    if (std::fread(mem, 1, size, file.get()) != size) {
        logERROR("Could not read %u bytes from file %s", size, path.string().c_str());
        return -1;
    }
    std::fgetc(file.get()); // set eof
    if (!std::feof(file.get())) {
        logERROR("File %s is larger than %u bytes", path.string().c_str(), size);
        return -1;
    }
    return 0;
}

int emu::load_rom(const fs::path& dir)
{
    int e;
    if (fs::exists(dir / "invaders.rom")) {
        e = load_file(dir / "invaders.rom", m.mem.get(), 8192);
        if (e) { return e; }
        logMESSAGE("Loaded ROM");
    }
    else {
        e = load_file(dir / "invaders.h", &m.mem[0], 2048);    if (e) { return e; }
        e = load_file(dir / "invaders.g", &m.mem[2048], 2048); if (e) { return e; }
        e = load_file(dir / "invaders.f", &m.mem[4096], 2048); if (e) { return e; }
        e = load_file(dir / "invaders.e", &m.mem[6144], 2048); if (e) { return e; }

        logMESSAGE("Loaded ROM files: invaders.e,f,g,h");
    }
    return 0;
}

static inline machine* MACHINE(i8080* cpu) {
    return static_cast<machine*>(cpu->udata);
}

// CPU emulation callbacks

static i8080_word_t cpu_mem_read(i8080* cpu, i8080_addr_t addr) {
    return MACHINE(cpu)->mem[addr];
}

static void cpu_mem_write(i8080* cpu, i8080_addr_t addr, i8080_word_t word) {
    MACHINE(cpu)->mem[addr] = word;
}

static i8080_word_t cpu_intr_read(i8080* cpu) {
    return MACHINE(cpu)->intr_opcode;
}

static i8080_word_t cpu_io_read(i8080* cpu, i8080_word_t port)
{
    machine* m = MACHINE(cpu);
    switch (port)
    {
    case 0: return m->in_port0;
    case 1: return m->in_port1;
    case 2: return m->in_port2;

    case 3: // offset from MSB
        return i8080_word_t(m->shiftreg >> (8 - m->shiftreg_off));

    default:
        logWARNING("IO read from unmapped port %d", int(port));
        return 0;
    }
}

static bool snd_is_looping(int idx)
{
    return idx == 0 || idx == 9;
}

// looping: repeat sound while pin is on.
// non-looping: restart sound every positive edge (off->on)
static void handle_sound(machine* m, int idx, bool pin_on)
{
    if (!m->sounds[idx]) {
        return;
    }
    if (pin_on) {
        if (!m->sndpin_lastvals[idx]) {
            int loops = snd_is_looping(idx) ? -1 : 0;
            Mix_PlayChannel(idx, m->sounds[idx], loops);
            m->sndpin_lastvals[idx] = true;
        }
    }
    else {
        if (snd_is_looping(idx)) {
            Mix_HaltChannel(idx);
        }
        m->sndpin_lastvals[idx] = false;
    }
}

static void cpu_io_write(i8080* cpu, i8080_word_t port, i8080_word_t word)
{
    machine* m = MACHINE(cpu);
    switch (port)
    {
    case 2:
        m->shiftreg_off = (word & 0x7);
        break;

    case 4:
        // shift from MSB
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

        // Watchdog port. Resets machine if unresponsive, 
        // not required for an emulator
    case 6: break;

    default:
        logWARNING("IO write to unmapped port %d", int(port));
        break;
    }
}

// helpful for debugging
void emu::log_dbginfo()
{
    logMESSAGE("Platform: "
        XSTR(COMPILER_NAME) " " XSTR(COMPILER_VERSION)
#ifdef COMPILER_FRONTNAME
        " (" XSTR(COMPILER_FRONTNAME) " frontend)"
#endif
        ", " XSTR(OS_NAME) " " XSTR(OS_VERSION));

    SDL_version version;
    SDL_GetVersion(&version);

    logMESSAGE("SDL2 version (header/DLL): %d.%d.%d/%d.%d.%d",
        SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
        version.major, version.minor, version.patch);

    const SDL_version* mix_version = Mix_Linked_Version();

    logMESSAGE("SDL2_mixer version (header/DLL): %d.%d.%d/%d.%d.%d",
        SDL_MIXER_MAJOR_VERSION, SDL_MIXER_MINOR_VERSION, SDL_MIXER_PATCHLEVEL,
        mix_version->major, mix_version->minor, mix_version->patch);

    emu_gui::log_dbginfo();
}

// default values in case of init failure or exception.
emu::emu(const fs::path& inipath) :
    m_window(nullptr),
    m_renderer(nullptr),
    m_pixfmt(nullptr),
    m_viewportrect({ .x = 0,.y = 0,.w = 0,.h = 0 }),
    m_viewporttex(nullptr),
    m_volume(0),
#ifndef __EMSCRIPTEN__
    m_inipath(inipath),
#endif
    m_ok(false)
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        m.sounds[i] = nullptr;
    }
    for (int i = 0; i < INPUT_NUM_INPUTS; ++i) {
        m_input2key[i] = input_dflt_key(inputtype(i));
    }
}

#ifdef __EMSCRIPTEN__
emu::emu(const fs::path& romdir, bool enable_ui) :
    emu("", romdir, false, enable_ui)
{}
#endif

emu::emu(const fs::path& inipath, 
    const fs::path& romdir, bool windowed, bool enable_ui) : 
    emu(inipath)
{
    log_dbginfo();

    if (init_graphics(enable_ui, windowed) != 0 || 
        init_audio(romdir) != 0) {
        return;
    }
    m.mem = std::make_unique<i8080_word_t[]>(65536);
    if (load_rom(romdir) != 0) {
        return;
    }

    m.cpu.mem_read = cpu_mem_read;
    m.cpu.mem_write = cpu_mem_write;
    m.cpu.io_read = cpu_io_read;
    m.cpu.io_write = cpu_io_write;
    m.cpu.intr_read = cpu_intr_read;
    m.cpu.udata = &m;
    m.cpu.reset();

    m.in_port0 = 0x0e; // debug port
    m.in_port1 = 0x08;
    m.in_port2 = 0;
    m.shiftreg = 0;
    m.shiftreg_off = 0;
    m.intr_opcode = i8080_NOP;

    m_ok = true;
}

emu::~emu()
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        Mix_FreeChunk(m.sounds[i]);
    }
    Mix_CloseAudio();
    SDL_DestroyTexture(m_viewporttex);

    m_gui.reset();

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void emu::set_switch(int index, bool value)
{
    switch (index)
    {
    case 3: set_bit(&m.in_port2, 0, value); break;
    case 4: set_bit(&m.in_port0, 0, value); break;
    case 5: set_bit(&m.in_port2, 1, value); break;
    case 6: set_bit(&m.in_port2, 3, value); break;
    case 7: set_bit(&m.in_port2, 7, value); break;
    default: break;
    }
}

bool emu::get_switch(int index) const
{
    switch (index)
    {
    case 3: return get_bit(m.in_port2, 0);
    case 4: return get_bit(m.in_port0, 0);
    case 5: return get_bit(m.in_port2, 1);
    case 6: return get_bit(m.in_port2, 3);
    case 7: return get_bit(m.in_port2, 7);
    default: return false;
    }
}

void emu::set_volume(int new_volume)
{
    SDL_assert(new_volume >= 0 && new_volume <= 100);
    if (new_volume != m_volume)
    {
        for (int i = 0; i < NUM_SOUNDS; ++i) {
            int scaled_vol = int((float(new_volume) / VOLUME_MAX) * MAX_VOLUMES[i]);
            Mix_Volume(i, scaled_vol);
        }
        m_volume = new_volume;
    } 
}

void emu::emulate_cpu(uint64_t& last_cpucycles, uint64_t nframes_rend)
{
    // pass input to machine ports
    set_bit(&m.in_port1, 0, m_keypressed[m_input2key[INPUT_CREDIT]]);
    set_bit(&m.in_port1, 1, m_keypressed[m_input2key[INPUT_2P_START]]);
    set_bit(&m.in_port1, 2, m_keypressed[m_input2key[INPUT_1P_START]]);
    set_bit(&m.in_port1, 4, m_keypressed[m_input2key[INPUT_P1_FIRE]]);
    set_bit(&m.in_port1, 5, m_keypressed[m_input2key[INPUT_P1_LEFT]]);
    set_bit(&m.in_port1, 6, m_keypressed[m_input2key[INPUT_P1_RIGHT]]);
    set_bit(&m.in_port2, 4, m_keypressed[m_input2key[INPUT_P2_FIRE]]);
    set_bit(&m.in_port2, 5, m_keypressed[m_input2key[INPUT_P2_LEFT]]);
    set_bit(&m.in_port2, 6, m_keypressed[m_input2key[INPUT_P2_RIGHT]]);

    // 33333.33 clk cycles at emulated CPU's 2Mhz clock speed (16667us/0.5us)
    uint64_t frame_cycles = 33333 + (nframes_rend % 3 == 0);

    // run till mid-screen
    // 14286 = (96/224) * (16667us/0.5us)
    while (m.cpu.cycles - last_cpucycles < 14286) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_1;
    m.cpu.interrupt();

    // run till end of screen (start of VBLANK)
    while (m.cpu.cycles - last_cpucycles < frame_cycles) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_2;
    m.cpu.interrupt();

    // extra cycles adjusted in next frame
    last_cpucycles += frame_cycles;
}

// Pixel color after gel overlay
// https://tcrf.net/images/a/af/SpaceInvadersArcColorUseTV.png
static colr_idx pixel_color(uint x, uint y)
{
    if ((y <= 15 && x > 24 && x < 136) || (y > 15 && y < 71)) {
        return COLRIDX_GREEN;
    }
    else if (y >= 192 && y < 223) {
        return COLRIDX_RED;
    }
    else { return COLRIDX_WHITE; }
}

void emu::draw_screen()
{
    void* pixels; int pitch;
    SDL_LockTexture(m_viewporttex, NULL, &pixels, &pitch);

    uint VRAM_idx = 0;
    i8080_word_t* VRAM_start = &m.mem[0x2400];
    const uint texpitch = pitch / m_pixfmt->bypp; // not always eq to original width!

    // Unpack (8 on/off pixels per byte) and rotate counter-clockwise
    for (uint x = 0; x < RES_NATIVE_X; ++x) 
    {
        for (uint y = 0; y < RES_NATIVE_Y; y += 8)
        {
            i8080_word_t word = VRAM_start[VRAM_idx++];

            for (int bit = 0; bit < 8; ++bit)
            {
                colr_idx colridx = get_bit(word, bit) ? pixel_color(x, y) : COLRIDX_BLACK;
                uint32_t color = m_pixfmt->colors[colridx];

                uint idx = texpitch * (RES_NATIVE_Y - y - bit - 1) + x;
                switch (m_pixfmt->bpp)
                {
                case 16: static_cast<uint16_t*>(pixels)[idx] = uint16_t(color); break;
                case 32: static_cast<uint32_t*>(pixels)[idx] = color;           break;
                }
            }
        }
    }
    SDL_UnlockTexture(m_viewporttex);
    SDL_RenderCopy(m_renderer, m_viewporttex, NULL, &m_viewportrect);
}

int emu::load_prefs()
{
#ifdef __EMSCRIPTEN__
    inireader ini;
#else
    if (!fs::exists(m_inipath)) {
        // okay, inifile will be created on exit
        return 0;
    }
    inireader ini(m_inipath);
    if (!ini.ok()) {
        return -1;
    }
#endif

    auto volume = ini.get_num<int>("Settings", "Volume");
    if (volume.has_value())
    {
        if (volume < 0 || volume > VOLUME_MAX) {
            logERROR("%s: Invalid volume", ini.path_cstr());
            return -1;
        }
        set_volume(volume.value());
    }
    for (int i = 3; i < 8; ++i)
    {
        char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
        auto sw = ini.get_num<uint>("Settings", sw_name);
        if (sw.has_value()) 
        {
            if (sw > 1u) {
                logERROR("%s: Invalid %s", ini.path_cstr(), sw_name);
                return -1;
            }
            set_switch(i, bool(sw.value()));
        }
    }
    for (int i = 0; i < INPUT_NUM_INPUTS; ++i)
    {
        auto keyname = ini.get_value("Settings", input_ininame(inputtype(i)));
        if (keyname.has_value()) 
        {
            SDL_Scancode key = SDL_GetScancodeFromName(keyname->c_str());
            if (key == SDL_SCANCODE_UNKNOWN) {
                logERROR("%s: Invalid %s", ini.path_cstr(), input_ininame(inputtype(i)));
                return -1;
            }
            m_input2key[i] = key;
        }
    }

    logMESSAGE("Loaded prefs");
    return 0;
}

int emu::save_prefs()
{
#ifdef __EMSCRIPTEN__
    iniwriter ini;
#else
    iniwriter ini(m_inipath);
    if (!ini.ok()) {
        return -1;
    }
#endif

    ini.write_section("Settings");

    ini.write_keyvalue("Volume", std::to_string(m_volume));

    for (int i = 3; i < 8; ++i)
    {
        char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
        char sw_val[] = { char('0' + get_switch(i)), '\0' };
        ini.write_keyvalue(sw_name, sw_val);
    }
    for (int i = 0; i < INPUT_NUM_INPUTS; ++i)
    {
        ini.write_keyvalue(input_ininame(inputtype(i)),
            SDL_GetScancodeName(m_input2key[i]));
    }

    if (!ini.flush()) {
        return -1;
    }

    logMESSAGE("Saved prefs");
    return 0;
}

#ifdef __EMSCRIPTEN__
// https://bugzilla.mozilla.org/show_bug.cgi?id=1826224
// https://bugzilla.mozilla.org/show_bug.cgi?id=1881627
// https://github.com/emscripten-core/emscripten/issues/20628
// emscripten_sleep() aka setTimeout() is broken on Windows Firefox.
// Min sleep time is ~30ms (~30fps) which makes the game very slow on high refresh-rate 
// displays where requestAnimationFrame() cannot be used directly.
// Resort to busy-wait in this case, which results in higher CPU usage.
//
// The proper way to solve this is to make the application independent of FPS,
// but that is not possible without a patched ROM.
//
EM_JS(int, fix_firefox_sleep, (), {
    return navigator.userAgent.includes("Windows") &&
        navigator.userAgent.includes("Firefox");
    });
static const bool FIX_FIREFOX_SLEEP = fix_firefox_sleep() != 0;

// this idea from imgui
static std::function<void()> mainloop_func_emcc;
static void mainloop_emcc() { mainloop_func_emcc(); }

#define EMCC_MAINLOOP_BEGIN mainloop_func_emcc = [&]() { do
#define EMCC_MAINLOOP_END \
    while (0); }; emscripten_set_main_loop(mainloop_emcc, FIX_FIREFOX_SLEEP ? -1 : 60, true)

#else
static const bool FIX_FIREFOX_SLEEP = false;
#endif

// More accurate than std::sleep_for() or PRESENT_VSYNC.
template <typename T>
static void wait_for(T tsleep)
{
    static constexpr auto wake_interval_us = tim::microseconds(3000);
    static constexpr auto wake_tolerance = tim::microseconds(500);
    static const uint64_t perfctr_freq = SDL_GetPerformanceFrequency();
    #define BUSY_LOOP_ONLY FIX_FIREFOX_SLEEP

    if (perfctr_freq < US_PER_S) [[unlikely]] {
        SDL_Delay(uint32_t(tim::round<tim::milliseconds>(tsleep).count()));
        return;
    }

    auto tcur = clk::now();
    auto tend = tcur + tsleep;
    auto trem = tend - tcur;

    while (tcur < tend)
    {
        trem = tend - tcur;

        if (!BUSY_LOOP_ONLY && trem > wake_interval_us + wake_tolerance) {
#ifdef _WIN32
            win32_sleep_ns(wake_interval_us.count() * NS_PER_US);
#else
            SDL_Delay(wake_interval_us.count() / US_PER_MS);
#endif  
            // adjust for call overhead
            tend -= tim::microseconds(5);
        }
        else {
            auto trem_us = tim::round<tim::microseconds>(trem);
            uint64_t cur_ctr = SDL_GetPerformanceCounter();
            uint64_t target_ctr = cur_ctr +
                uint64_t(trem_us.count() * (double(perfctr_freq) / US_PER_S));

            while (cur_ctr < target_ctr) {
                cur_ctr = SDL_GetPerformanceCounter();
            }
        }
        tcur = clk::now();
    }
}

#ifdef __EMSCRIPTEN__
const char* emcc_beforeunload(int, const void*, void* udata)
{
    static_cast<emu*>(udata)->save_prefs();
    return nullptr;
}
#endif

int emu::run()
{
    int err = load_prefs();
    if (err) { return err; }

#ifdef __EMSCRIPTEN__
    // Save prefs before exit.
    // This can't run after emscripten_set_main_loop (because infinite loop),
    // nor can it run in SDL_QUIT (browser unload event is very limited).
    // Callback must also be synchronous.
    EMSCRIPTEN_RESULT res = emscripten_set_beforeunload_callback(this, emcc_beforeunload);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        logERROR("Failed to set beforeunload callback");
        return -1;
    }
#endif

    SDL_ShowWindow(m_window);

    uint64_t nframes = 0;
    uint64_t cpucycles = 0;
    clk::time_point t_start = clk::now();   

    bool running = true;

    if (FIX_FIREFOX_SLEEP) {
        logMESSAGE("Enabled Firefox sleep fix");
    }

    logMESSAGE("Start!");

#ifdef __EMSCRIPTEN__
    EMCC_MAINLOOP_BEGIN
#else
    while (running)
#endif
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            bool handle_keyevent = true;
            if (m_gui) {
                m_gui->process_event(&e);
                handle_keyevent = !m_gui->want_keyboard();
            }

            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (handle_keyevent) {
                    m_keypressed[e.key.keysym.scancode] = true;
                }
                break;
            case SDL_KEYUP:
                if (handle_keyevent) {
                    m_keypressed[e.key.keysym.scancode] = false;
                }
                break;

            default: break;
            }
        }
        // "pause" when minimized
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Emulate CPU for 1 frame.
        emulate_cpu(cpucycles, nframes);
        // Draw contents of VRAM.
        draw_screen();

        if (m_gui) { 
            m_gui->run_frame();
        }

        SDL_RenderPresent(m_renderer);
        
        if (!is_emscripten() || FIX_FIREFOX_SLEEP)
        {
            // 60 Hz CRT refresh rate
            static constexpr tim::microseconds tframe_target(16667);

            auto tframe = clk::now() - t_start;
            if (tframe < tframe_target) {
                wait_for(tframe_target - tframe);
            }
        }

        auto t_laststart = t_start;
        t_start = clk::now();
        
        if (m_gui) 
        {
            float delta_t = tim::duration<float>(t_start - t_laststart).count();
            m_gui->set_fps(int(std::lroundf(1.f / delta_t)));
        }

        nframes++;
    }
#ifdef __EMSCRIPTEN__
    EMCC_MAINLOOP_END;
#endif

    // see emcc_beforeunload()
    if constexpr (!is_emscripten()) {
        int err = save_prefs();
        if (err) { return err; }
    }

    return 0;
}
