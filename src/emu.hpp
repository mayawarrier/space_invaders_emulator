
#ifndef EMU_HPP
#define EMU_HPP

#include <array>
#include <memory>
#include <SDL.h>
#include <SDL_mixer.h>
#include <imgui.h>

#include "i8080/i8080.hpp"
#include "utils.hpp"


#define KEY_CREDIT SDL_SCANCODE_RETURN
#define KEY_1P_START SDL_SCANCODE_1
#define KEY_2P_START SDL_SCANCODE_2

#define KEY_P1_LEFT SDL_SCANCODE_LEFT
#define KEY_P1_RIGHT SDL_SCANCODE_RIGHT
#define KEY_P1_FIRE SDL_SCANCODE_SPACE

#define KEY_P2_LEFT SDL_SCANCODE_A
#define KEY_P2_RIGHT SDL_SCANCODE_D
#define KEY_P2_FIRE SDL_SCANCODE_LCTRL

#define RES_NATIVE_X 224
#define RES_NATIVE_Y 256
#define RES_SCALE_DEFAULT 3

#define NUM_SOUNDS 10
#define MAX_VOLUME 100


struct machine
{
    i8080 cpu;
    std::unique_ptr<i8080_word_t[]> mem;

    i8080_word_t in_port0;
    i8080_word_t in_port1;
    i8080_word_t in_port2;

    // Shift register chip
    i8080_dword_t shiftreg;
    i8080_word_t shiftreg_off;

    // Video chip interrupts
    i8080_word_t intr_opcode;

    // Sound chip
    Mix_Chunk* sounds[NUM_SOUNDS];
    uint8_t sndpin_lastvals[NUM_SOUNDS];
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

struct emu_gui
{
    int init(emu* emu, SDL_Rect* screen_rect);
    void shutdown();

    void print_dbginfo();

    void set_fps(int fps) { m_fps = fps; }

    void set_delta_t(float delta_t) 
    {
        m_deltat = delta_t;
        m_deltat_min = std::min(m_deltat_min, delta_t);
        m_deltat_max = std::max(m_deltat_max, delta_t);
    }

    bool process_event(SDL_Event* e);
    bool want_keyboard();
    bool want_mouse();

    void draw();

private:
    void draw_panel(const char* title, void(emu_gui::*content_cb)());
    void draw_help_content();
    void draw_settings_content();

private:
    emu* m_emu;
    ImFont* m_hdr_font;
    ImFont* m_txt_font;
    int m_menubar_height;
    int m_cur_panel;

    int m_fps;
    float m_deltat;
    float m_deltat_min;
    float m_deltat_max;
};

struct emu
{
    // \param rom_dir directory containing invaders ROM and audio files
    // \param res_scale resolution scaling factor.
    // game renders at res_scale * native resolution.
    emu(const fs::path& rom_dir, 
        uint res_scale = RES_SCALE_DEFAULT);

    // Open a window and start running.
    // Returns when window is closed.
    void run();

    // Set cabinet DIP switches.
    void set_switches(uint8_t values)
    {
        for (int i = 3; i < 8; ++i) {
            set_switch(i, get_bit(values, i));
        }
    }

    bool ok() const { return m_ok; }

    ~emu();

    friend emu_gui;

private:
    emu(uint scalefac);

    void print_dbginfo();

    int init_graphics();
    int init_audio(const fs::path& audiodir);
    int load_rom(const fs::path& dir);

    void handle_input(SDL_Scancode sc, bool pressed);

    bool get_switch(int index);
    void set_switch(int index, bool value);

    int get_volume() { return m_volume; }
    void set_volume(int volume);

    void run_cpu(uint64_t& cpucycles, uint64_t nframes);

    void draw_screen();

private:
    machine m;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Rect m_screenrect;
    SDL_Texture* m_screentex;

    const pix_fmt* m_pixfmt;
    uint m_scalefac;
    uint m_scresX;
    uint m_scresY;
    int m_volume;

    emu_gui m_gui;

    bool m_ok;
};

#endif