//
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// to learn about the hardware inside the Space Invaders arcade machine,
// and to understand how the emulator works.
// 
// For CPU emulation, see i8080.cpp.
// This file emulates everything else (audio, video, I/O, interrupts, etc.).
//

#include <cmath>
#include <string_view>

#include "i8080/i8080_opcodes.hpp"
#include "gui.hpp"
#include "emu.hpp"

#ifdef _WIN32
#include "win32.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <functional>
#endif

static const char* input_ininame(input type)
{
    switch (type)
    {
    case INPUT_P1_LEFT:  return "InputP1Left";
    case INPUT_P1_RIGHT: return "InputP1Right";
    case INPUT_P1_FIRE:  return "InputP1Fire";
    case INPUT_P2_LEFT:  return "InputP2Left";
    case INPUT_P2_RIGHT: return "InputP2Right";
    case INPUT_P2_FIRE:  return "InputP2Fire";
    case INPUT_1P_START: return "Input1PStart";
    case INPUT_2P_START: return "Input2PStart";
    case INPUT_CREDIT:   return "InputCredit";
    default:
        return nullptr;
    }
}

static SDL_Scancode input_dflt_key(input type)
{
    switch (type)
    {
    case INPUT_P1_LEFT:  return SDL_SCANCODE_LEFT;
    case INPUT_P1_RIGHT: return SDL_SCANCODE_RIGHT;
    case INPUT_P1_FIRE:  return SDL_SCANCODE_SPACE;
    case INPUT_P2_LEFT:  return SDL_SCANCODE_LEFT;
    case INPUT_P2_RIGHT: return SDL_SCANCODE_RIGHT;
    case INPUT_P2_FIRE:  return SDL_SCANCODE_SPACE;
    case INPUT_1P_START: return SDL_SCANCODE_1;
    case INPUT_2P_START: return SDL_SCANCODE_2;
    case INPUT_CREDIT:   return SDL_SCANCODE_RETURN;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static int get_disp_size(SDL_Window* window, SDL_Point& out_size)
{
    int disp_idx = SDL_GetWindowDisplayIndex(window);
    if (disp_idx < 0) {
        logERROR("SDL_GetWindowDisplayIndex(): %s", SDL_GetError());
        return -1;
    }
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds(disp_idx, &bounds) != 0) {
        logERROR("SDL_GetDisplayUsableBounds(): %s", SDL_GetError());
        return -1;
    }
    out_size = { .x = bounds.w, .y = bounds.h };
    return 0;
}

static SDL_Point get_viewport_size(SDL_Window* window, int maxX, int maxY)
{
    int sizeX, sizeY;
    if (is_emscripten() ||
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN))
    {
        // max resolution maintaining aspect ratio
        int scaledX = int(maxY * float(RES_NATIVE_X) / RES_NATIVE_Y);
        if (scaledX <= maxX) {
            sizeX = scaledX;
            sizeY = maxY;
        } else {
            sizeX = maxX;
            sizeY = int(maxX * float(RES_NATIVE_Y) / RES_NATIVE_X);
        }
    }
    else {
        // max discrete multiple of native resolution
        int max_factorX = maxX / RES_NATIVE_X;
        int max_factorY = maxY / RES_NATIVE_Y;
        int factor = std::min(max_factorX, max_factorY);
        sizeX = RES_NATIVE_X * factor;
        sizeY = RES_NATIVE_Y * factor;
    }
    return { .x = sizeX, .y = sizeY };
}

// Recompute window/GUI/viewport sizes.
// Window and GUI must be set up first.
int emu::resize_window(void)
{
    int e = get_disp_size(m_window, m_dispsize);
    if (e) { return e; }

    auto& vp = m_viewportrect;

    SDL_Point vp_offset{ .x = 0, .y = 0 };
    SDL_Point vp_maxsize = m_dispsize;
    gui_sizeinfo guiinfo = {0};
    if (m_gui)
    {
        guiinfo = m_gui->get_sizeinfo(m_dispsize);
        auto total_resv = sdl_ptadd(guiinfo.resv_inwnd_size, guiinfo.resv_outwnd_size);

        vp_offset = guiinfo.vp_offset;
        vp_maxsize = sdl_ptsub(vp_maxsize, total_resv);
    }

    SDL_Point vp_size = get_viewport_size(m_window, vp_maxsize.x, vp_maxsize.y);
    vp = {
        .x = vp_offset.x,
        .y = vp_offset.y,
        .w = vp_size.x,
        .h = vp_size.y
    };

    SDL_Point win_size = sdl_ptadd(vp_size, guiinfo.resv_inwnd_size);

    SDL_SetWindowSize(m_window, win_size.x, win_size.y);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if constexpr (!is_emscripten() || is_debug()) { // too frequent on emscripten
        logMESSAGE("Viewport bounds: x: %d, y: %d, w: %d, h: %d", vp.x, vp.y, vp.w, vp.h);
        logMESSAGE("Window size: x: %d, y: %d", win_size.x, win_size.y);
    }
    return 0;
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

int emu::init_texture(SDL_Renderer* renderer)
{
    SDL_RendererInfo rendinfo;
    if (SDL_GetRendererInfo(renderer, &rendinfo) != 0) {
        logERROR("SDL_GetRendererInfo(): %s", SDL_GetError());
        return -1;
    }

    logMESSAGE("Render backend: %s", rendinfo.name);

    // Get first supported texture format
    // see https://stackoverflow.com/questions/56143991/
    for (auto& pixfmt : PIXFMTS) {
        for (uint32_t i = 0; i < rendinfo.num_texture_formats; ++i)
        {
            if (rendinfo.texture_formats[i] == pixfmt.fmt) {
                logMESSAGE("Texture format: %s", pixfmt_name(pixfmt.fmt));
                m_pixfmt = &pixfmt;
                goto done;
            }
        }
    }
done:
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

        logERROR("Could not find a supported texture format.\n"
            "Supported: %s\nAvailable: %s", suppfmts.c_str(), hasfmts.c_str());
        return -1;
    }

    m_viewporttex = SDL_CreateTexture(renderer, m_pixfmt->fmt,
        SDL_TEXTUREACCESS_STREAMING, RES_NATIVE_X, RES_NATIVE_Y);
    if (!m_viewporttex) {
        logERROR("SDL_CreateTexture(): %s", SDL_GetError());
        return -1;
    }

    return 0;
}

int emu::init_graphics(const fs::path& assetdir, bool enable_ui)
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

    int e = init_texture(m_renderer);
    if (e) { return e; }

    if (enable_ui) {
        m_gui = std::make_unique<emu_gui>(assetdir, m_window, m_renderer, this);
        if (!m_gui->ok()) { return -1; }
    }

    // Set viewport and window size etc.
    int err = resize_window();
    if (err) { return err; }

    return 0;
}

static const int MAX_MIX_VOLUMES[] =
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
    if (Mix_OpenAudio(11025, AUDIO_U8, 1, is_emscripten() ? 1024 : 512) != 0) {
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
        m.sndpins_last[i] = false;

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
        if (!m->sndpins_last[idx]) {
            int loops = snd_is_looping(idx) ? -1 : 0;
            Mix_PlayChannel(idx, m->sounds[idx], loops);
            m->sndpins_last[idx] = true;
        }
    }
    else {
        if (snd_is_looping(idx)) {
            Mix_HaltChannel(idx);
        }
        m->sndpins_last[idx] = false;
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
        XSTR(CC_NAME) " " XSTR(CC_VERSION)
#ifdef CC_SIMNAME
        " (" XSTR(CC_SIMNAME) " frontend)"
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

// default values
emu::emu() :
    m_window(nullptr),
    m_renderer(nullptr),
    m_pixfmt(nullptr),
    m_dispsize({ .x = 0,.y = 0 }),
    m_viewportrect({ .x = 0,.y = 0,.w = 0,.h = 0 }),
    m_viewporttex(nullptr),
    m_volume(0),
    m_audiopaused(false),
    m_delta_t(-1),
    m_hiscore(0),
    m_hiscore_in_vmem(false),
#ifdef __EMSCRIPTEN__
    m_resizepending(false),
#endif
    m_ok(false)
{
    std::fill_n(m.sounds, NUM_SOUNDS, nullptr);

    std::fill_n(m_guiinputpressed.begin(), NUM_INPUTS, false);

    for (int i = 0; i < NUM_INPUTS; ++i) {
        m_input2key[i] = input_dflt_key(input(i));
    }
}

#ifdef __EMSCRIPTEN__
bool emcc_on_window_resize(int, const EmscriptenUiEvent*, void* udata)
{
    static_cast<emu*>(udata)->m_resizepending = true;
    return true;
}
bool emcc_on_viz_change(int, const EmscriptenVisibilityChangeEvent* event, void* udata)
{
    if (event->hidden || event->visibilityState == EMSCRIPTEN_VISIBILITY_UNLOADED) {
        static_cast<emu*>(udata)->save_udata();
    }
    return true;
}
#endif

emu::emu(const fs::path& assetdir, bool enable_ui) :
    emu()
{
    log_dbginfo();

    if (init_graphics(assetdir, enable_ui) != 0 ||
        init_audio(assetdir) != 0) {
        return;
    }

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_RESULT res;
    
    res = emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, this, true, emcc_on_window_resize);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        logERROR("emscripten_set_resize_callback(): %s", emcc_result_name(res));
        return;
    }
    // SDL_QUIT aka browser unload() event is very unreliable and
    // beforeunload does not work reliably either on mobile browsers.
    // visibilitychange is the best option to save state.
    // https://developer.chrome.com/docs/web-platform/page-lifecycle-api
    res = emscripten_set_visibilitychange_callback(this, true, emcc_on_viz_change);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        logERROR("emscripten_set_visibilitychange_callback(): %s", emcc_result_name(res));
        return;
    }
#endif

    m.mem = std::make_unique<i8080_word_t[]>(65536);
    if (load_rom(assetdir) != 0) {
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
#ifdef __EMSCRIPTEN__
    emscripten_set_visibilitychange_callback(NULL, 0, NULL);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, 0, NULL);
#endif

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
        for (int i = 0; i < NUM_SOUNDS; ++i)
        {
            float scaled_vol = MAX_MIX_VOLUMES[i] * (float(new_volume) / 100);
            Mix_Volume(i, int(std::lroundf(scaled_vol)));
        }
        m_volume = new_volume;
    }
}

void emu::emulate_cpu(uint64_t frame_idx, uint64_t& target_cycles)
{
    // pass input to machine ports
    set_bit(&m.in_port1, 0, m_keypressed[m_input2key[INPUT_CREDIT]]   || m_guiinputpressed[INPUT_CREDIT]);
    set_bit(&m.in_port1, 1, m_keypressed[m_input2key[INPUT_2P_START]] || m_guiinputpressed[INPUT_2P_START]);
    set_bit(&m.in_port1, 2, m_keypressed[m_input2key[INPUT_1P_START]] || m_guiinputpressed[INPUT_1P_START]);
    set_bit(&m.in_port1, 4, m_keypressed[m_input2key[INPUT_P1_FIRE]]  || m_guiinputpressed[INPUT_P1_FIRE]);
    set_bit(&m.in_port1, 5, m_keypressed[m_input2key[INPUT_P1_LEFT]]  || m_guiinputpressed[INPUT_P1_LEFT]);
    set_bit(&m.in_port1, 6, m_keypressed[m_input2key[INPUT_P1_RIGHT]] || m_guiinputpressed[INPUT_P1_RIGHT]);
    set_bit(&m.in_port2, 4, m_keypressed[m_input2key[INPUT_P2_FIRE]]  || m_guiinputpressed[INPUT_P2_FIRE]);
    set_bit(&m.in_port2, 5, m_keypressed[m_input2key[INPUT_P2_LEFT]]  || m_guiinputpressed[INPUT_P2_LEFT]);
    set_bit(&m.in_port2, 6, m_keypressed[m_input2key[INPUT_P2_RIGHT]] || m_guiinputpressed[INPUT_P2_RIGHT]);

    // 33333.33 clk cycles at emulated CPU's 2Mhz clock speed (16667us/0.5us)
    uint64_t frame_cycles = 33333 + (frame_idx % 3 == 0);
    uint64_t prev_targetcycles = target_cycles;

    // run till mid-screen
    // 14286 = (96/224) * (16667us/0.5us)
    while (m.cpu.cycles - prev_targetcycles < 14286) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_1;
    m.cpu.interrupt();

    // run till end of screen (start of VBLANK)
    while (m.cpu.cycles - prev_targetcycles < frame_cycles) {
        m.cpu.step();
    }
    m.intr_opcode = i8080_RST_2;
    m.cpu.interrupt();

    // extra cycles adjusted in next frame
    target_cycles += frame_cycles;
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

void emu::render_screen()
{
    void* pixels; int pitch;
    SDL_LockTexture(m_viewporttex, NULL, &pixels, &pitch);

    uint VRAM_idx = 0;
    i8080_word_t* VRAM_start = &m.mem[VRAM_START_ADDR];
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

#ifndef __EMSCRIPTEN__
static const fs::path& APPDATA_DIR()
{
    static fs::path g_path = [] {
        char* pathstr = SDL_GetPrefPath("SpaceInvaders", "v1");
        fs::path path = fs::u8path(pathstr);
        SDL_free(pathstr);
        return path;
        }();
    return g_path;
}

static const fs::path& INI_PATH() {
    static fs::path path = APPDATA_DIR() / "spaceinvaders.ini";
    return path;
}
static const fs::path& HISCORE_PATH() {
    static fs::path path = APPDATA_DIR() / "hiscore.dat";
    return path;
}
#endif

// discourage casual tampering :)
static uint16_t checksum(uint16_t in)
{
    char buf[5];
    auto res = std::to_chars(buf, buf + 5, in);

    uint16_t s1 = 0, s2 = 0;
    for (int i = 0; i < int(res.ptr - buf); ++i)
    {
        s1 = (s1 + uint8_t(buf[i])) % 255;
        s2 = (s2 + s1) % 255;
    }
    return ((s2 << 8) | s1);
}


int emu::read_hiscore(uint16_t& out_hiscore)
{
    uint16_t hiscore, cksum;

#ifdef __EMSCRIPTEN__
    inireader ini;
    auto value = ini.get_string("HiScore", "value");
    if (!value.has_value()) {
        out_hiscore = 0;
        return 0; // okay, will be saved on exit
    }
    if (value->size() != 10) {
        logERROR("Invalid highscore value");
        return -1;
    }

    const char* str = value->c_str();
    auto res1 = std::from_chars(str, str + 5, hiscore);
    auto res2 = std::from_chars(str + 5, str + 10, cksum);

    if (res1.ec != std::errc() || res1.ptr != str + 5 ||
        res2.ec != std::errc() || res2.ptr != str + 10) {
        logERROR("Invalid highscore value");
        return -1;
    }
#else
    if (!fs::exists(HISCORE_PATH())) {
        out_hiscore = 0;
        return 0; // okay, will be saved on exit
    }

    uint8_t buf[4];
    auto file = SAFE_FOPEN(HISCORE_PATH().c_str(), "rb");
    if (!file || std::fread(buf, 1, 4, file.get()) != 4) {
        logERROR("Could not open or read highscore file");
        return -1;
    }

    std::memcpy(&hiscore, buf, 2);
    std::memcpy(&cksum, buf + 2, 2);
#endif

    uint16_t cksum_calc = checksum(hiscore);
    if (cksum != cksum_calc) {
        logERROR("Highscore checksum does not match");
        return -1;
    }

    out_hiscore = hiscore;
    return 0;
}

int emu::load_udata()
{
#ifdef __EMSCRIPTEN__
    inireader ini;
#else
    if (!fs::exists(INI_PATH())) {
        // okay, inifile will be created on exit
        return 0;
    }
    inireader ini(INI_PATH());
    if (!ini.ok()) {
        return -1;
    }
#endif

    auto volume = ini.get_num<int>("Settings", "Volume");
    if (volume.has_value()) {
        if (volume < 0 || volume > 100) {
            logERROR("%s: Invalid volume", ini.path_cstr());
            return -1;
        }
        set_volume(volume.value());
    }
    for (int i = 3; i < 8; ++i)
    {
        char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
        auto sw = ini.get_num<uint>("Settings", sw_name);
        if (sw.has_value()) {
            if (sw > 1u) {
                logERROR("%s: Invalid %s", ini.path_cstr(), sw_name);
                return -1;
            }
            set_switch(i, bool(sw.value()));
        }
    }
    for (int i = 0; i < NUM_INPUTS; ++i)
    {
        auto keyname = ini.get_string("Settings", input_ininame(input(i)));
        if (keyname.has_value())
        {
            SDL_Scancode key = SDL_GetScancodeFromName(keyname->c_str());
            if (key == SDL_SCANCODE_UNKNOWN) {
                logERROR("%s: Invalid %s", ini.path_cstr(), input_ininame(input(i)));
                return -1;
            }
            m_input2key[i] = key;
        }
    }

    if (read_hiscore(m_hiscore) != 0) {
        return -1;
    }
    m_hiscore_in_vmem = false;

    logMESSAGE("Loaded user data");
    return 0;
}

int emu::save_udata()
{
#ifdef __EMSCRIPTEN__
    iniwriter ini;
#else
    iniwriter ini(INI_PATH());
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
    for (int i = 0; i < NUM_INPUTS; ++i)
    {
        ini.write_keyvalue(input_ininame(input(i)),
            SDL_GetScancodeName(m_input2key[i]));
    }

    if (!ini.flush()) {
        logERROR("Could not flush ini file");
        return -1;
    }

    if (m_hiscore_in_vmem)
    {
        uint16_t new_hiscore = m.mem[HISCORE_START_ADDR] |
            (uint16_t(m.mem[HISCORE_START_ADDR + 1]) << 8);

        // in case two instances of the game are running, 
        // don't overwrite a higher score set by the other instance
        uint16_t cur_hiscore;
        if (read_hiscore(cur_hiscore) == 0 && cur_hiscore > new_hiscore) {
            logMESSAGE("Skipped saving hiscore, current is greater");
        }
        else {
            uint16_t cksum = checksum(new_hiscore);

#ifdef __EMSCRIPTEN__
            std::string value;

            char buf[5];
            auto res = std::to_chars(buf, buf + 5, new_hiscore);
            value.append(buf + 5 - res.ptr, '0');
            value.append(buf, res.ptr - buf);

            res = std::to_chars(buf, buf + 5, cksum);
            value.append(buf + 5 - res.ptr, '0');
            value.append(buf, res.ptr - buf);

            ini.write_section("HiScore");
            ini.write_keyvalue("value", value);
            if (!ini.flush()) {
                logERROR("Could not save hiscore");
                return -1;
            }
#else
            uint8_t buf[4] = {};
            std::memcpy(buf, &new_hiscore, 2);
            std::memcpy(buf + 2, &cksum, 2);

            auto file = SAFE_FOPEN(HISCORE_PATH().c_str(), "wb");
            if (!file || std::fwrite(buf, 1, 4, file.get()) != 4) {
                logERROR("Could not open or write highscore file");
                return -1;
            }
            if (std::fflush(file.get()) != 0) {
                logERROR("Could not flush highscore file");
                return -1;
            }
#endif
        }
    }

    if constexpr (!is_emscripten()) { // too frequent on emscripten
        logMESSAGE("Saved user data");
    }
    return 0;
}

#ifdef __EMSCRIPTEN__
// https://bugzilla.mozilla.org/show_bug.cgi?id=1826224
// https://bugzilla.mozilla.org/show_bug.cgi?id=1881627
// https://github.com/emscripten-core/emscripten/issues/20628
// emscripten_sleep() aka setTimeout() are broken on Windows Firefox.
// Min sleep time is ~30ms (~33fps), which is too slow for the game's 60Hz CRT.
// Use requestAnimationFrame() instead, and busy wait if host refresh rate is > 60Hz.
//
EM_JS(int, web_has_broken_sleep, (), {
    return navigator.userAgent.includes("Windows") &&
        navigator.userAgent.includes("Firefox");
    });

static const bool WEB_HAS_BROKEN_SLEEP = web_has_broken_sleep() != 0;

// Audio glitches seem worse when using requestAnimationFrame
// on Chrome, so enable it only if necessary.
// Probably related: https://issues.chromium.org/issues/40486473
static const int WEB_MAINLOOP_FPS = WEB_HAS_BROKEN_SLEEP ? -1 : 60;
#else

static constexpr bool WEB_HAS_BROKEN_SLEEP = false;
#endif

// Vsync with high precision.
// Much more accurate than std::sleep_for() or PRESENT_VSYNC.
static void vsync(clk::time_point tframe_start)
{
    if (!is_emscripten() || WEB_HAS_BROKEN_SLEEP)
    {
        // 60 Hz CRT refresh rate
        static constexpr tim::microseconds tframe_target(16667);

        auto tframe = clk::now() - tframe_start;
        if (tframe < tframe_target)
        {
            auto twait = tframe_target - tframe;

            static constexpr auto wake_interval_us = tim::microseconds(3000);
            static constexpr auto wake_tolerance = tim::microseconds(500);
            static const uint64_t perfctr_freq = SDL_GetPerformanceFrequency();

            if (perfctr_freq < US_PER_S) [[unlikely]] {
                SDL_Delay(uint32_t(tim::round<tim::milliseconds>(twait).count()));
                return;
            }

            auto tcur = clk::now();
            auto tend = tcur + twait;
            auto trem = tend - tcur;

            while (tcur < tend)
            {
                trem = tend - tcur;

                if (!WEB_HAS_BROKEN_SLEEP &&
                    trem > wake_interval_us + wake_tolerance) {
#ifdef _WIN32
                    win32_sleep_ns(wake_interval_us.count() * NS_PER_US);
#else
                    SDL_Delay(wake_interval_us.count() / US_PER_MS);
#endif  
                    // adjust for overhead
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
    }
}

void emu::send_input(input inp, bool pressed)
{
    m_guiinputpressed[inp] = pressed;
}

// Handle all input events, window events etc.
emu::mainloop_action emu::process_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        gui_captureinfo evt_capture;
        if (m_gui) {
            m_gui->process_event(&event, evt_capture);
        }

        switch (event.type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!evt_capture.capture_keyboard) {
                auto scancode = event.key.keysym.scancode;
                m_keypressed[scancode] = (event.type == SDL_KEYDOWN);
            }
            break;

        case SDL_QUIT:
            // emscripten udata saved on viz change
            if constexpr (!is_emscripten()) {
                logMESSAGE("Quitting...");
                save_udata();
            }
            return MAINLOOP_EXIT;
        }
    }
#ifdef __EMSCRIPTEN__
    if (m_resizepending)
    {
        resize_window(); // ignore failure
        m_resizepending = false;
    }
    if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_HIDDEN) {
        return MAINLOOP_SKIP;
    }
#else
    if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) {
        SDL_Delay(20);
        return MAINLOOP_SKIP;
    }
#endif

    return MAINLOOP_CONTINUE;
}

#ifdef __EMSCRIPTEN__
// this idea from imgui
static std::function<void()> emcc_mainloop_func;
static void emcc_mainloop() { emcc_mainloop_func(); }

#define EMCC_MAINLOOP_BEGIN emcc_mainloop_func = [&]() -> void { do
#define EMCC_MAINLOOP_END \
    while (0); }; emscripten_set_main_loop(emcc_mainloop, WEB_MAINLOOP_FPS, true)
#endif

int emu::run()
{
    int err = load_udata();
    if (err) { return err; }

    logMESSAGE("Starting emulator...");

    SDL_ShowWindow(m_window);

    uint64_t frame_idx = 0;
    uint64_t target_clkcycles = 0;
    clk::time_point t_start = clk::now();

#ifdef __EMSCRIPTEN__
    EMCC_MAINLOOP_BEGIN
#else
    while (true)
#endif
    {
        emu::mainloop_action action = process_events();
#ifdef __EMSCRIPTEN__
        if (action != MAINLOOP_CONTINUE) { return; }
#else
        if (action == MAINLOOP_EXIT) { break; }
        else if (action == MAINLOOP_SKIP) { continue; }
#endif
        SDL_assert(action == MAINLOOP_CONTINUE);

        // nasty workaround, since the score table is erased in frame 0
        if (frame_idx == 1) [[unlikely]] {
            m.mem[HISCORE_START_ADDR] = uint8_t(m_hiscore);
            m.mem[HISCORE_START_ADDR + 1] = uint8_t(m_hiscore >> 8);
            m_hiscore_in_vmem = true;
        }

        if (!m_gui || m_gui->current_view() == VIEW_GAME)
        {
            // Emulate CPU for 1 frame.
            emulate_cpu(frame_idx, target_clkcycles);
            // Draw game.
            render_screen();

            if (m_audiopaused) {
                Mix_Resume(-1);
                m_audiopaused = false;
            }
        }
        else if (!m_audiopaused) {
            Mix_Pause(-1);
            m_audiopaused = true;
        }

        // Draw GUI.
        if (m_gui) {
            m_gui->run(m_dispsize, m_viewportrect);
        }

        SDL_RenderPresent(m_renderer);

        // Vsync at 60 fps.
        vsync(t_start);

        auto t_laststart = t_start;
        t_start = clk::now();

        m_delta_t = tim::duration<float>(t_start - t_laststart).count();
        frame_idx++;
    }
#ifdef __EMSCRIPTEN__
    EMCC_MAINLOOP_END;
#endif

    return 0;
}
