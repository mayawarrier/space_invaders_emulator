//
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// for documentation on the hardware inside the Space Invaders arcade machine.
// It provides context for a lot of what is emulated here.
//

#include <cmath>
#include <string_view>

#include "i8080/i8080_opcodes.h"
#include "gui.hpp"
#include "emu.hpp"

#ifdef _WIN32
#include "win32.hpp"
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
    case INPUT_P1_LEFT:  return "Key to use for P1 left.";
    case INPUT_P1_RIGHT: return "Key to use for P1 right.";
    case INPUT_P1_FIRE:  return "Key to use for P1 fire.";
    case INPUT_P2_LEFT:  return "Key to use for P2 left.";
    case INPUT_P2_RIGHT: return "Key to use for P2 right.";
    case INPUT_P2_FIRE:  return "Key to use for P2 fire.";
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

    std::printf("\n General\n");
    std::printf(CONFIG_FMTSTR, "RomDir", "(<dir>)", "Directory containing ROM and audio files.");
    std::printf(CONFIG_FMTSTR, "EnableUI", "(yes/no)", "Enable emulator UI (settings / help panels etc.)");
    std::printf(CONFIG_FMTSTR, "ResScale", "<int>", "Resolution scaling factor.");

    std::printf("\n Settings\n");
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

static inline machine* MACHINE(i8080* cpu) { 
    return static_cast<machine*>(cpu->udata);
}

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

int emu::init_graphics(bool enable_ui)
{
    logMESSAGE("Initializing graphics");

    uint scalefac;
    if (!m_ini.get_num_or_dflt<uint>(
        "General", "ResScale", RES_SCALE_DEFAULT, scalefac) ||
        scalefac == 0) {
        logERROR("Invalid ResScale in %s", m_ini.path_str().c_str());
        return -1;
    }
    m_scalefac = scalefac;
    m_screenresX = RES_NATIVE_X * scalefac;
    m_screenresY = RES_NATIVE_Y * scalefac;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return logERROR("SDL_Init(): %s", SDL_GetError());
    }

    // Prevents several assorted freezes and lag on Windows
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif

    m_window = SDL_CreateWindow("Space Invaders", 0, 0, 0, 0, SDL_WINDOW_HIDDEN);
    if (!m_window) {
        return logERROR("SDL_CreateWindow(): %s", SDL_GetError());
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (!m_renderer) {
        return logERROR("SDL_CreateRenderer(): %s", SDL_GetError());
    }

    SDL_RendererInfo rendinfo;
    if (SDL_GetRendererInfo(m_renderer, &rendinfo) != 0) {
        return logERROR("SDL_GetRendererInfo(): %s", SDL_GetError());
    }

    logMESSAGE("-- Render backend: %s", rendinfo.name);

    if (enable_ui) {
        m_gui = std::make_unique<emu_gui>(this);
        if (!m_gui->ok()) { return -1; }
        m_viewportrect = m_gui->viewport_rect();
    }
    else {
        m_viewportrect = {
            .x = 0,
            .y = 0,
            .w = int(m_screenresX),
            .h = int(m_screenresY)
        };
    }

    SDL_SetWindowSize(m_window, m_screenresX, m_screenresY + m_viewportrect.y);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    logMESSAGE("-- Viewport bounds: x: %d, y: %d, w: %d, h: %d",
        m_viewportrect.x, m_viewportrect.y, m_viewportrect.w, m_viewportrect.h);

    // use first supported texture format
    // see https://stackoverflow.com/questions/56143991/
    m_pixfmt = nullptr;
    for (auto& pixfmt : PIXFMTS) {
        for (uint32_t i = 0; i < rendinfo.num_texture_formats; ++i)
        {
            if (rendinfo.texture_formats[i] == pixfmt.fmt) {
                logMESSAGE("-- Texture format: %s", pixfmt_name(pixfmt.fmt));
                m_pixfmt = &pixfmt;
                goto pixfmt_done;
            }
        }
    }
pixfmt_done:
    if (!m_pixfmt) 
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

        return logERROR("Could not find a supported texture format.\n"
            "Supported: %s\nAvailable: %s", suppfmts.c_str(), hasfmts.c_str());
    }
    
    m_viewporttex = SDL_CreateTexture(m_renderer, 
        m_pixfmt->fmt, SDL_TEXTUREACCESS_STREAMING, m_screenresX, m_screenresY);
    if (!m_viewporttex) {
        return logERROR("SDL_CreateTexture(): %s", SDL_GetError());
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
        return logERROR("Mix_OpenAudio(): %s", Mix_GetError());
    }
    if (Mix_AllocateChannels(NUM_SOUNDS) != NUM_SOUNDS) {
        return logERROR("Mix_AllocateChannels(): %s", Mix_GetError());
    }
    // adjust volume, some tracks are too loud
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        Mix_Volume(i, MAX_VOLUMES[i]);
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
            std::fputs("-- ", LOGFILE);
            std::fputs("-- ", stderr);
            logWARNING("Audio file %d (aka %s) is missing", i, AUDIO_FILENAMES[i][1]);
        }
    }
    if (num_loaded == NUM_SOUNDS) {
        logMESSAGE("Loaded audio files");
    } else {
        logMESSAGE("Loaded %d/%d audio files", num_loaded, NUM_SOUNDS);
    }
    return 0;
}

static int load_file(const fs::path& path, i8080_word_t* mem, unsigned size)
{
    scopedFILE file = SAFE_FOPEN(path.c_str(), "rb");
    if (!file) {
        return logERROR("Could not open file %s", path.string().c_str());
    }
    if (std::fread(mem, 1, size, file.get()) != size) {
        return logERROR("Could not read %u bytes from file %s", size, path.string().c_str());
    }
    std::fgetc(file.get()); // set eof
    if (!std::feof(file.get())) {
        return logERROR("File %s is larger than %u bytes", path.string().c_str(), size);
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

// helpful for debugging
void emu::print_dbginfo()
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

    if (m_gui) {
        m_gui->print_dbginfo();
    }

    logMESSAGE("");
}

// default values in case of init failure or exception.
emu::emu() noexcept :
    m_window(nullptr),
    m_renderer(nullptr),
    m_viewportrect({ .x = 0,.y = 0,.w = 0,.h = 0 }),
    m_viewporttex(nullptr),
    m_pixfmt(nullptr),
    m_scalefac(0),
    m_screenresX(0),
    m_screenresY(0),
    m_volume(0),
    m_ok(false)
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        m.sounds[i] = nullptr;
    }
    for (int i = 0; i < INPUT_NUM_INPUTS; ++i) {
        m_input2key[i] = SDL_SCANCODE_UNKNOWN;
    }
}

int emu::load_prefs()
{
    int volume;
    if (!m_ini.get_num_or_dflt("Settings", "Volume", VOLUME_DEFAULT, volume) ||
        volume < 0 || volume > VOLUME_MAX) {
        return logERROR("Invalid Volume in %s", m_ini.path_str().c_str());
    }
    set_volume(volume);

    for (int i = 3; i < 8; ++i) 
    {
        char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0'};
        uint sw_val;
        if (!m_ini.get_num_or_dflt("Settings", sw_name, 0u, sw_val) || sw_val > 1) {
            return logERROR("Invalid %s in %s", sw_name, m_ini.path_str().c_str());
        }      
        set_switch(i, bool(sw_val));
    }

    for (int i = 0; i < INPUT_NUM_INPUTS; ++i)
    {
        auto keyname = m_ini.get_value("Settings", input_ininame(inputtype(i)));
        if (!keyname.data()) {
            m_input2key[i] = input_dflt_key(inputtype(i));
        } else {
            SDL_Scancode key = SDL_GetScancodeFromName(keyname.data());
            if (key == SDL_SCANCODE_UNKNOWN) {
                return logERROR("Invalid %s in %s", input_ininame(inputtype(i)), m_ini.path_str().c_str());
            }
            m_input2key[i] = key;
        }
    }

    logMESSAGE("Loaded prefs");
    return 0;
}

int emu::save_prefs()
{
    m_ini.set_value("Settings", "Volume", std::to_string(m_volume));

    for (int i = 3; i < 8; ++i) {
        char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
        m_ini.set_value("Settings", sw_name, std::to_string(uint(get_switch(i))));
    }
    for (int i = 0; i < INPUT_NUM_INPUTS; ++i) {
        m_ini.set_value("Settings", 
            input_ininame(inputtype(i)), SDL_GetScancodeName(m_input2key[i]));
    }
    return m_ini.write();
}

emu::emu(const fs::path& ini_path) : emu()
{
    print_dbginfo();

    m_ini = ini(ini_path);
    if (m_ini.read() != 0) {
        return;
    }
    
    fs::path rom_dir = m_ini.get_value_or_dflt("General", "RomDir", "data");
    bool enable_ui = m_ini.get_value_or_dflt("General", "EnableUI", "yes") == "yes";

    if (init_graphics(enable_ui) != 0 || 
        init_audio(rom_dir) != 0) {
        return;
    }
    m.mem = std::make_unique<i8080_word_t[]>(65536);
    if (load_rom(rom_dir) != 0) {
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

    if (load_prefs() != 0) {
        return;
    }

    logMESSAGE("Ready!");
    m_ok = true;
}

emu::~emu()
{
    save_prefs();

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
    // may not be equal to screenresX!
    const uint texpitch = pitch / m_pixfmt->bypp;

    // Unpack (8 on/off pixels per byte) and rotate counter-clockwise
    for (uint x = 0; x < RES_NATIVE_X; ++x) {
        for (uint y = 0; y < RES_NATIVE_Y; y += 8)
        {
            i8080_word_t word = VRAM_start[VRAM_idx++];

            for (int bit = 0; bit < 8; ++bit)
            {
                colr_idx colridx = get_bit(word, bit) ? pixel_color(x, y) : COLRIDX_BLACK;
                uint32_t color = m_pixfmt->colors[colridx];

                uint st_idx = m_scalefac * (texpitch * (RES_NATIVE_Y - y - bit - 1) + x);

                // Draw a square instead of a pixel if resolution is higher than native
                for (uint xsqr = 0; xsqr < m_scalefac; ++xsqr) {
                    for (uint ysqr = 0; ysqr < m_scalefac; ++ysqr)
                    {
                        uint idx = st_idx + texpitch * ysqr + xsqr;

                        switch (m_pixfmt->bpp)
                        {
                        case 16: static_cast<uint16_t*>(pixels)[idx] = uint16_t(color); break;
                        case 32: static_cast<uint32_t*>(pixels)[idx] = color;           break;
                        }
                    }
                }
            }
        }
    }
    SDL_UnlockTexture(m_viewporttex);
    SDL_RenderCopy(m_renderer, m_viewporttex, NULL, &m_viewportrect);
}

// Much more accurate than std::sleep_for() or PRESENT_VSYNC.
template <typename T>
static void vsync_sleep_for(T tsleep)
{
    static constexpr auto wake_interval_us = tim::microseconds(3000);
    static constexpr auto wake_tolerance = tim::microseconds(500);
    static const uint64_t perfctr_freq = SDL_GetPerformanceFrequency();

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

        if (trem > wake_interval_us + wake_tolerance) {
#ifdef _WIN32
            win32_sleep_ns(wake_interval_us.count() * NS_PER_US);
#else
            SDL_Delay(wake_interval_us.count() / US_PER_MS);
#endif  
            // adjust for stdlib + call overhead
            tend -= tim::microseconds(2);
        }
        else {
            auto trem_us = tim::round<tim::microseconds>(trem);
            uint64_t cur_ctr = SDL_GetPerformanceCounter();
            uint64_t target_ctr = cur_ctr + trem_us.count() * perfctr_freq / US_PER_S;

            while (cur_ctr < target_ctr) {
                cur_ctr = SDL_GetPerformanceCounter();
            }
        }
        tcur = clk::now();
    }
}

void emu::run()
{
    SDL_ShowWindow(m_window);
        
    // 60 Hz CRT refresh rate
    static constexpr auto tframe_target = tim::microseconds(16667);

    uint64_t nframes = 0;
    uint64_t cpucycles = 0; 

    clk::time_point t_start = clk::now();

    bool running = true;
    while (running)
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
        // Pause when minimized
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            t_start = clk::now();
            continue;
        }

        // Emulate CPU for 1 frame.
        emulate_cpu(cpucycles, nframes);
        // Draw contents of VRAM.
        draw_screen();

        if (m_gui) { 
            m_gui->run();
        }
        
        SDL_RenderPresent(m_renderer);

        // Vsync
        auto tframe = clk::now() - t_start;
        if (tframe < tframe_target) {
            vsync_sleep_for(tframe_target - tframe);
        }

        auto t_laststart = t_start;
        t_start = clk::now();
        
        // exclude first frame, first one is always
        // slower due to lazy initialization
        if (m_gui && nframes != 0) 
        {
            float delta_t = tim::duration<float>(t_start - t_laststart).count();
            m_gui->set_delta_t(delta_t);
            m_gui->set_fps(1.f / delta_t);
        }

        nframes++;
    }
}



