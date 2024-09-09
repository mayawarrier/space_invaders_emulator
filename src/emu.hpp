
#ifndef EMU_HPP
#define EMU_HPP

#include <array>
#include <memory>
#include <bitset>

#include <SDL.h>
#include <SDL_mixer.h>

#include "i8080/i8080.hpp"
#include "utils.hpp"


#define RES_NATIVE_X 224
#define RES_NATIVE_Y 256
#define RES_SCALE_DEFAULT 3

#define NUM_SOUNDS 10
#define MAX_VOLUME_UI 100


enum inputtype
{
    INPUT_P1_LEFT,
    INPUT_P1_RIGHT,
    INPUT_P1_FIRE, 

    INPUT_P2_LEFT,
    INPUT_P2_RIGHT,
    INPUT_P2_FIRE,

    INPUT_CREDIT,
    INPUT_1P_START,
    INPUT_2P_START,

    INPUT_NUM_INPUTS
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
    std::bitset<NUM_SOUNDS> sndpin_lastvals;
};

struct pix_fmt
{
    uint32_t fmt;
    uint bpp;
    std::array<uint32_t, 4> colors;

    pix_fmt(uint32_t fmt, std::array<uint32_t, 4> pal) :
        fmt(fmt), bpp(SDL_BITSPERPIXEL(fmt)), colors(pal)
    {}
};

struct emu;
struct emu_gui;

// GUI can only use functions from here.
struct emu_interface
{
    emu_interface(emu* e) :
        m_emu(e), m_volume(MAX_VOLUME_UI)
    {}
    emu_interface() :
        emu_interface(nullptr)
    {}

    SDL_Window* window();
    SDL_Renderer* renderer();

    uint screenresX() const; // excluding UI
    uint screenresY() const;

    bool get_switch(int index) const;
    void set_switch(int index, bool value);

    int get_volume() const;
    void set_volume(int volume);

private:
    emu* m_emu;
    int m_volume;
};

struct emu
{
    // \param rom_dir directory containing invaders ROM and audio files
    // \param enable_ui True to enable emulator UI.
    // \param res_scale resolution scaling factor.
    // game renders at res_scale * native resolution.
    emu(const fs::path& rom_dir, 
        bool enable_ui = true,
        uint res_scale = RES_SCALE_DEFAULT);

    ~emu();

    bool ok() const { return m_ok; }

    // Set cabinet DIP switches.
    void set_switches(uint8_t values)
    {
        for (int i = 3; i < 8; ++i) {
            set_switch(i, get_bit(values, i));
        }
    }

    // Open a window and start running.
    // Returns when window is closed.
    void run();

    friend emu_interface;

private:
    emu(uint scalefac);

    void print_dbginfo();

    int init_graphics(bool enable_ui);
    int init_audio(const fs::path& audiodir);
    int load_rom(const fs::path& dir);

    bool get_switch(int index) const;
    void set_switch(int index, bool value);

    void emulate_cpu(uint64_t& cpucycles, uint64_t nframes);
    void draw_screen();

private:
    machine m;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    std::unique_ptr<emu_gui> m_gui;
    SDL_Rect m_viewportrect;
    SDL_Texture* m_viewporttex;

    const pix_fmt* m_pixfmt;
    uint m_scalefac;
    uint m_scresX;
    uint m_scresY;

    std::bitset<SDL_NUM_SCANCODES> m_keypressed;
    SDL_Scancode m_input2key[INPUT_NUM_INPUTS];

    bool m_ok;
};

inline SDL_Window* emu_interface::window() { return m_emu->m_window; }
inline SDL_Renderer* emu_interface::renderer() { return m_emu->m_renderer; }

inline uint emu_interface::screenresX() const { return m_emu->m_scresX; }
inline uint emu_interface::screenresY() const { return m_emu->m_scresY; }

inline bool emu_interface::get_switch(int index) const {
    return m_emu->get_switch(index);
}
inline void emu_interface::set_switch(int index, bool value) {
    m_emu->set_switch(index, value);
}

#endif