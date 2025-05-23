
#ifndef EMU_HPP
#define EMU_HPP

#include <array>
#include <memory>
#include <bitset>

#include "i8080/i8080.hpp"
#include "utils.hpp"

#include <SDL.h>
#include <SDL_mixer.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#define RES_NATIVE_X 224
#define RES_NATIVE_Y 256
#define RES_SCALE_DEFAULT 3

#define NUM_SOUNDS 10
#define VOLUME_DEFAULT 50

// todo: these assume a compatible ROM
#define VRAM_START_ADDR 0x2400
#define GAMEMODE_ADDR 0x20ef
#define HISCORE_START_ADDR 0x20f4

enum input : uint8_t
{
    INPUT_P1_LEFT,
    INPUT_P1_RIGHT,
    INPUT_P1_FIRE, 

    INPUT_P2_LEFT,
    INPUT_P2_RIGHT,
    INPUT_P2_FIRE,

    INPUT_1P_START,
    INPUT_2P_START,
    INPUT_CREDIT,

    NUM_INPUTS
};

struct machine
{
    i8080 cpu;
    std::unique_ptr<i8080_word_t[]> mem;

    i8080_word_t in_port0;
    i8080_word_t in_port1;
    i8080_word_t in_port2;

    // Video chip interrupts
    i8080_word_t intr_opcode;

    // Shift register chip
    i8080_dword_t shiftreg;
    i8080_word_t shiftreg_off;

    // Sound chip
    Mix_Chunk* sounds[NUM_SOUNDS];
    std::bitset<NUM_SOUNDS> sndpins_last;
};

struct pix_fmt
{
    uint32_t fmt;
    std::array<uint32_t, 4> colors;

    pix_fmt(uint32_t fmt, std::array<uint32_t, 4> pal) :
        fmt(fmt),
        colors(pal)
    {
        SDL_assert(SDL_BYTESPERPIXEL(fmt) == 4);
    }
};

struct emu;
struct emu_gui;

// GUI can only use functions from here.
struct emu_interface
{
    emu_interface(emu* e) :
        m_emu(e)
    {}
    emu_interface() :
        emu_interface(nullptr)
    {}

    bool in_demo_mode() const;

    bool get_switch(int index) const;
    void set_switch(int index, bool value);

    int get_volume() const;
    void set_volume(int volume);

    // Delta t for last frame.
    float delta_t() const;

    void send_input(input inp, bool pressed);

    std::array<SDL_Scancode, NUM_INPUTS>& input2keymap();

private:
    emu* m_emu;
};

struct emu
{
    emu(const fs::path& asset_dir,
        const std::string& render_hint = "",
        bool enable_ui = true);

    ~emu();

    bool ok() const { return m_ok; }

    // Start running.
    // Returns <0 on error, otherwise 0 when window is closed.
    int run();

private:
    emu();

    static void log_dbginfo();

    int init_texture(SDL_Renderer* renderer, const SDL_RendererInfo& rend_info);
    int init_graphics(const fs::path& assetdir, const std::string& render_hint, bool enable_ui);
    int init_audio(const fs::path& audiodir);
    int load_rom(const fs::path& dir);

    int read_hiscore(uint16_t& out_hiscore);
    int load_udata();
    int save_udata();

    int resize_window();
    void send_input(input inp, bool pressed);

    enum mainloop_action {
        MAINLOOP_EXIT,
        MAINLOOP_SKIP,
        MAINLOOP_CONTINUE
    };
    mainloop_action process_events();
    
    bool get_switch(int index) const;
    void set_switch(int index, bool value);
    void set_volume(int volume);

    void emulate_cpu(uint64_t frame_idx, uint64_t& target_cycles);
    void render_screen();

private:
    machine m;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    
    const pix_fmt* m_pixfmt;
    SDL_Point m_dispsize;
    SDL_Rect m_viewportrect;
    SDL_Texture* m_viewporttex;
    std::unique_ptr<emu_gui> m_gui;
    
    std::array<bool, NUM_INPUTS> m_guiinputpressed;
    std::bitset<SDL_NUM_SCANCODES> m_keypressed;
    std::array<SDL_Scancode, NUM_INPUTS> m_input2key;

    int m_volume;
    bool m_audiopaused;

    bool m_hiscore_in_vmem;
    uint16_t m_hiscore; // bcd format

    float m_delta_t;

#ifdef __EMSCRIPTEN__
    bool m_resizepending;
    bool m_paused;
#endif
    bool m_ok;

public:
#ifdef __EMSCRIPTEN__
    friend bool emcc_on_window_resize(int, const EmscriptenUiEvent*, void*);
    friend bool emcc_on_viz_change(int, const EmscriptenVisibilityChangeEvent*, void*);
#endif
    friend emu_interface;
};

inline bool emu_interface::in_demo_mode() const {
    return m_emu->m.mem[GAMEMODE_ADDR] == 0;
}

inline bool emu_interface::get_switch(int index) const {
    return m_emu->get_switch(index);
}
inline void emu_interface::set_switch(int index, bool value) {
    m_emu->set_switch(index, value);
}

inline int emu_interface::get_volume() const { 
    return m_emu->m_volume; 
}
inline void emu_interface::set_volume(int volume) { 
    m_emu->set_volume(volume); 
}

inline void emu_interface::send_input(input inp, bool pressed) {
    m_emu->send_input(inp, pressed);
}

inline std::array<SDL_Scancode, NUM_INPUTS>& emu_interface::input2keymap() {
    return m_emu->m_input2key; 
}

inline float emu_interface::delta_t() const{
    return m_emu->m_delta_t;
}

#endif