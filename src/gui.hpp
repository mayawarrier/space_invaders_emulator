
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>

#include "utils.hpp"
#include "emu.hpp"


struct emu_gui
{
    emu_gui(emu_interface emu);
    ~emu_gui();

    void print_dbginfo();

    bool process_event(SDL_Event* e);

    // True if GUI wants keyboard inputs.
    bool want_keyboard();
    // True if GUI wants mouse inputs.
    bool want_mouse();

    void run();

    SDL_Rect viewport_rect() const
    {
        return {
            .x = 0,
            .y = m_menubar_height,
            .w = int(m_emu.screenresX()),
            .h = int(m_emu.screenresY())
        };
    }

    void set_fps(int fps) { m_fps = fps; }

    void set_delta_t(float delta_t)
    {
        m_deltat = delta_t;
        m_deltat_min = std::min(m_deltat_min, delta_t);
        m_deltat_max = std::max(m_deltat_max, delta_t);
    }

    bool ok() const { return m_ok; }

private: 
    void draw_help_content();
    void draw_settings_content();
    void draw_panel(const char* title, void(emu_gui::*draw_content)());
    void draw_inputkey(const char* label, inputtype inptype);

private:
    emu_interface m_emu;
    ImFont* m_hdr_font;
    ImFont* m_txt_font;
    int m_menubar_height;
    int m_cur_panel;
    // in current frame
    SDL_Scancode m_lastkeypress;

    int m_fps;
    float m_deltat;
    float m_deltat_min;
    float m_deltat_max;

    bool m_inputkey_focused[INPUT_NUM_INPUTS];

    bool m_ok;
};


#endif