
#ifndef EMU_HPP
#define EMU_HPP

#include <array>
#include <memory>
#include <SDL.h>
#include <SDL_mixer.h>

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

#define SCREEN_NATIVERES_X 224
#define SCREEN_NATIVERES_Y 256

#define NUM_SOUNDS 10


struct machine
{
    i8080 cpu;
    std::unique_ptr<i8080_word_t[]> mem;

    i8080_word_t in_port1;
    i8080_word_t in_port2;

    // Shift register chip
    i8080_dword_t shiftreg;
    i8080_word_t shiftreg_off;

    // Video chip interrupts
    i8080_word_t intr_opcode;

    // Sound chip
    Mix_Chunk* sounds[NUM_SOUNDS];
    bool snd_playing[NUM_SOUNDS];
};

// sdl_pixelfmt -> color palette
using fmt_palette = std::pair<uint32_t, std::array<uint32_t, 4>>;

struct emulator
{
    // \param rom_dir directory containing invaders ROM and audio files
    // \param res_scale resolution scaling factor.
    // If equal to 2, renders at 2x native resolution, etc.
    emulator(const fs::path& rom_dir, uint res_scale = 2);

    // Open a window and start running.
    // Returns when window is closed.
    void run();

    bool ok() const { return m_ok; }
    operator bool() const { return ok(); }

    ~emulator();

private:
    int read_rom(const fs::path& dir);
    int init_graphics(uint scresX, uint scresY);
    int init_audio(const fs::path& audiodir);

    void handle_input(SDL_Scancode sc, bool pressed);

    void gen_frame(uint64_t& cpucycles, uint64_t nframes_rend);
    void render_frame() const;

private:
    machine m;
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;

    SDL_Texture* m_screentex;
    const fmt_palette* m_fmtpalette;
    uint m_scalefac;
    uint m_scresX;

    bool m_ok;
};

#endif