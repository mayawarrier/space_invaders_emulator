
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>
#include <cmath>

#include "utils.hpp"
#include "emu.hpp"


enum gui_panel_t
{
    PANEL_NONE,
    PANEL_SETTINGS,
    PANEL_HELP
};

enum gui_align_t
{
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_CENTER
};

struct emu_gui
{
    emu_gui(emu_interface emu);
    ~emu_gui();

    bool ok() const { return m_ok; }

    bool process_event(const SDL_Event* e);

    // True if GUI wants keyboard inputs.
    bool want_keyboard() const;
    // True if GUI wants mouse inputs.
    bool want_mouse() const;

    // Run the GUI for one frame.
    void draw_frame();

    void set_delta_t(float delta_t)
    {
        m_fps = int(std::lroundf(1.f / delta_t));
    }

    SDL_Point max_viewport_size(SDL_Point vp_max) {
        return { .x = vp_max.x, .y = vp_max.y - m_menu_height };
    }

    SDL_Point viewport_offset() {
        return { .x = 0, .y = m_menu_height };
    }

    static void log_dbginfo();

private: 
    void draw_help_content();
    void draw_settings_content();
    void draw_panel(const char* title, void(emu_gui::*draw_content)());
    void draw_inputkey(const char* label, inputtype inptype,
        gui_align_t align = ALIGN_LEFT, float labelsizeX = -1);

private:
    emu_interface m_emu;
    ImFont* m_hdr_font;
    ImFont* m_subhdr_font;
    int m_menu_height;
    int m_cur_panel;
    int m_fps;

    SDL_Scancode m_lastkeypress; // cur frame
    bool m_inputkey_focused[INPUT_NUM_INPUTS];

    bool m_ok;
};

#endif