//
// Emulate all the hardware inside the Space Invaders arcade machine, and 
// provide a user interface to interact with the emulator.
// 
// See https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
// for documentation.
//

#include <cmath>
#include <string_view>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "i8080/i8080_opcodes.h"
#include "emu.hpp"


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
        WARNING("IO read from unmapped port %d", int(port));
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
        WARNING("IO write to unmapped port %d", int(port));
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

// https://github.com/ocornut/imgui/issues/252
static int imgui_menubar_height()
{
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // nasty workaround!
    // create a dummy window and measure its menu bar size
    ImGui::Begin("Dummy", NULL, ImGuiWindowFlags_NoTitleBar);
    ImGui::BeginMainMenuBar();
    ImGui::EndMainMenuBar();
    ImGui::End();

    int height = (int)ImGui::GetFrameHeight();
    ImGui::EndFrame();

    return height;
}

int emulator::init_graphics()
{
    MESSAGE("Initializing graphics");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return ERROR("SDL_Init(): %s", SDL_GetError());
    }

    // DirectX backend temporarily freezes if window size/position is set after creation.
    // Probably related to this: https://stackoverflow.com/questions/40312553/
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    m_window = SDL_CreateWindow("Space Invaders", 0, 0, 0, 0, SDL_WINDOW_HIDDEN);
    if (!m_window) {
        return ERROR("SDL_CreateWindow(): %s", SDL_GetError());
    }
    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (!m_renderer) {
        return ERROR("SDL_CreateRenderer(): %s", SDL_GetError());
    }

    SDL_RendererInfo rendinfo;
    if (SDL_GetRendererInfo(m_renderer, &rendinfo) != 0) {
        return ERROR("SDL_GetRendererInfo(): %s", SDL_GetError());
    }

    MESSAGE("-- Render backend: %s", rendinfo.name);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer) ||
        !ImGui_ImplSDLRenderer2_Init(m_renderer)) {
        return ERROR("Failed to initialize ImGui with SDL backend");
    }

    ImFontConfig font_cfg;
    font_cfg.SizePixels = 15.f;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontDefault(&font_cfg);
    io.Fonts->Build();

    int menubar_height = imgui_menubar_height();
    m_screenrect = {
        .x = 0,
        .y = menubar_height,
        .w = int(m_scresX),
        .h = int(m_scresY)
    };

    SDL_SetWindowSize(m_window, m_scresX, m_scresY + menubar_height);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    MESSAGE("-- Screen bounds: x: %d, y: %d, w: %d, h: %d",
        m_screenrect.x, m_screenrect.y, m_screenrect.w, m_screenrect.h);

    // use first supported texture format
    // see https://stackoverflow.com/questions/56143991/
    m_pixfmt = nullptr;
    for (auto& pixfmt : PIXFMTS) {
        for (uint32_t i = 0; i < rendinfo.num_texture_formats; ++i)
        {
            if (rendinfo.texture_formats[i] == pixfmt.fmt) {
                MESSAGE("-- Texture format: %s", pixfmt_name(pixfmt.fmt));
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

        return ERROR("Could not find a supported texture format.\n"
            "Supported: %s\nAvailable: %s", suppfmts.c_str(), hasfmts.c_str());
    }
    
    m_screentex = SDL_CreateTexture(m_renderer, 
        m_pixfmt->fmt, SDL_TEXTUREACCESS_STREAMING, m_scresX, m_scresY);
    if (!m_screentex) {
        return ERROR("SDL_CreateTexture(): %s", SDL_GetError());
    }

    return 0;
}

int emulator::init_audio(const fs::path& audio_dir) 
{
    MESSAGE("Initializing audio");

    // chunksize is small to reduce latency
    if (Mix_OpenAudio(11025, AUDIO_U8, 1, 512) != 0) {
        return ERROR("Mix_OpenAudio(): %s", Mix_GetError());
    }
    if (Mix_AllocateChannels(NUM_SOUNDS) != NUM_SOUNDS) {
        return ERROR("Mix_AllocateChannels(): %s", Mix_GetError());
    }
    // adjust volume
    Mix_Volume(0, MIX_MAX_VOLUME / 2); // UFO fly
    Mix_Volume(8, MIX_MAX_VOLUME / 2); // UFO die
    Mix_Volume(3, MIX_MAX_VOLUME / 2); // Alien die
    Mix_Volume(1, MIX_MAX_VOLUME / 2); // Shoot

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
            WARNING("Audio file %d (aka %s) is missing", i, AUDIO_FILENAMES[i][1]);
        }
    }
    if (num_loaded == NUM_SOUNDS) {
        MESSAGE("Loaded audio files");
    } else {
        MESSAGE("Loaded %d/%d audio files", num_loaded, NUM_SOUNDS);
    }
    return 0;
}

static int load_file(const fs::path& path, i8080_word_t* mem, unsigned size)
{
    auto filename = path.filename().string();

    scopedFILE file = SAFE_FOPEN(path.c_str(), "rb");
    if (!file) {
        return ERROR("Could not open file %s", filename.c_str());
    }
    if (std::fread(mem, 1, size, file.get()) != size) {
        return ERROR("Could not read %u bytes from file %s", size, filename.c_str());
    }
    std::fgetc(file.get()); // set eof
    if (!std::feof(file.get())) {
        return ERROR("File %s is larger than %u bytes", filename.c_str(), size);
    }
    return 0;
}

int emulator::load_rom(const fs::path& dir)
{
    int e;
    if (fs::exists(dir / "invaders.rom")) {
        e = load_file(dir / "invaders.rom", m.mem.get(), 8192);
        if (e) { return e; }
        MESSAGE("Loaded ROM");
    }
    else {
        e = load_file(dir / "invaders.h", &m.mem[0], 2048);    if (e) { return e; }
        e = load_file(dir / "invaders.g", &m.mem[2048], 2048); if (e) { return e; }
        e = load_file(dir / "invaders.f", &m.mem[4096], 2048); if (e) { return e; }
        e = load_file(dir / "invaders.e", &m.mem[6144], 2048); if (e) { return e; }

        MESSAGE("Loaded ROM files: invaders.e,f,g,h");
    }
    return 0;
}

static int imgui_demo_window()
{
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    ImGui::StyleColorsLight();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    bool running = true;
    bool show_demo_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}

emulator::emulator(uint scalefac) :
    m_window(nullptr), m_renderer(nullptr), m_screentex(nullptr),
    m_scalefac(scalefac), 
    m_scresX(RES_NATIVE_X * scalefac),
    m_scresY(RES_NATIVE_Y * scalefac),
    m_ok(false)
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        m.sounds[i] = nullptr;
    }
}

// helpful for debugging
void emulator::print_envstats()
{
    MESSAGE("Platform: "
        XSTR(COMPILER_NAME) " " XSTR(COMPILER_VERSION)
#ifdef COMPILER_FRONTNAME
        " (" XSTR(COMPILER_FRONTNAME) " frontend)"
#endif
        ", " XSTR(OS_NAME) " " XSTR(OS_VERSION));

    SDL_version version;
    SDL_GetVersion(&version);

    MESSAGE("SDL2 version (header/DLL): %d.%d.%d/%d.%d.%d",
        SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
        version.major, version.minor, version.patch);

    const SDL_version* mix_version = Mix_Linked_Version();

    MESSAGE("SDL2_mixer version (header/DLL): %d.%d.%d/%d.%d.%d",
        SDL_MIXER_MAJOR_VERSION, SDL_MIXER_MINOR_VERSION, SDL_MIXER_PATCHLEVEL,
        mix_version->major, mix_version->minor, mix_version->patch);

    MESSAGE("ImGui version: %s", ImGui::GetVersion());

    MESSAGE("");
}


emulator::emulator(const fs::path& romdir, uint scalefac) :
    emulator(scalefac)
{
    print_envstats();

    imgui_demo_window();

    if (init_graphics() != 0 ||
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

    MESSAGE("Ready!");
    m_ok = true;
}

emulator::~emulator()
{
    for (int i = 0; i < NUM_SOUNDS; ++i) {
        Mix_FreeChunk(m.sounds[i]);
    }
    Mix_CloseAudio();
    SDL_DestroyTexture(m_screentex);

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
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

void emulator::handle_switch(int index, bool value)
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

void emulator::draw_screen(uint64_t& last_cpucycles, uint64_t nframes_rend)
{
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

    void* pixels; int pitch;
    SDL_LockTexture(m_screentex, NULL, &pixels, &pitch);

    uint VRAM_idx = 0;
    i8080_word_t* VRAM_start = &m.mem[0x2400];

    // Unpack (8 on/off pixels per byte) and rotate counter-clockwise
    for (uint x = 0; x < RES_NATIVE_X; ++x) {
        for (uint y = 0; y < RES_NATIVE_Y; y += 8)
        {
            i8080_word_t word = VRAM_start[VRAM_idx++];

            for (int bit = 0; bit < 8; ++bit)
            {
                colr_idx colridx = get_bit(word, bit) ? pixel_color(x, y) : COLRIDX_BLACK;
                uint32_t color = m_pixfmt->colors[colridx];

                uint st_idx = m_scalefac * (m_scresX * (RES_NATIVE_Y - y - bit - 1) + x);

                // Draw a square instead of a pixel if resolution is higher than native
                for (uint xsqr = 0; xsqr < m_scalefac; ++xsqr) {
                    for (uint ysqr = 0; ysqr < m_scalefac; ++ysqr)
                    {
                        uint idx = st_idx + m_scresX * ysqr + xsqr;

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
    SDL_UnlockTexture(m_screentex);
    SDL_RenderCopy(m_renderer, m_screentex, NULL, &m_screenrect);
}

void emulator::draw_handle_ui()
{
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Settings")) {
        }
        ImGui::SameLine();

        if (ImGui::Button("Help!")) {
            int width, height;
            SDL_GetWindowSize(m_window, &width, &height);
            SDL_SetWindowSize(m_window, width + 500, height);
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
        ImGui::SameLine();

        ImGui::Text("FPS: %d", m_ui_fps);

        ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);
}

// 100ns resolution on Windows
using clk = tim::steady_clock;

void emulator::run()
{
    SDL_ShowWindow(m_window);

    uint64_t nframes = 0;   // total frames rendered
    uint64_t cpucycles = 0; // 8080 cpu cycles
    float fps_sum = 0;

    tim::microseconds tframe_target(16667); // 60 fps
    clk::time_point t_start = clk::now();

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);

            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (!ImGui::GetIO().WantCaptureKeyboard) {
                    handle_input(e.key.keysym.scancode, true);
                }
                break;
            case SDL_KEYUP:
                if (!ImGui::GetIO().WantCaptureKeyboard) {
                    handle_input(e.key.keysym.scancode, false);
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

        SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
        SDL_RenderClear(m_renderer);

        draw_screen(cpucycles, nframes);
        draw_handle_ui();

        SDL_RenderPresent(m_renderer);

        // Wait until vsync
        auto tframe = clk::now() - t_start;
        if (tframe < tframe_target) {
            auto sleep_ms = tim::round<tim::milliseconds>(tframe_target - tframe);
            SDL_Delay(uint32_t(sleep_ms.count()));
        }

        auto t_laststart = t_start;
        t_start = clk::now();

#define FPS_SMOOTH_NFRAMES 10

        // Adjust sleep time to meet CRT's 60Hz refresh rate as closely as possible
        if (nframes % FPS_SMOOTH_NFRAMES == 0) 
        {
            long fps_avg = std::lroundf(fps_sum / FPS_SMOOTH_NFRAMES);
            if (fps_avg < 57) {
                tframe_target -= tim::microseconds(1000);
            } else if (fps_avg < 60) {
                tframe_target -= tim::microseconds(100);
            } else if (fps_avg > 63) {
                tframe_target += tim::microseconds(1000);
            } else if (fps_avg > 60) {
                tframe_target += tim::microseconds(100);
            }
            fps_sum = 0;

            std::printf("\rFPS: %ld", fps_avg);
            std::fflush(stdout);

            m_ui_fps = (int)fps_avg;
        } 
        
        float fps = 1.f / tim::duration<float>(t_start - t_laststart).count();
        fps_sum += fps;

        nframes++;
    }

    // newline for cmd prompt
    std::printf("\n");
}

