
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>

#include "utils.hpp"
#include "emu.hpp"

struct gui_inputkey
{
    SDL_Scancode key;
    bool focused;

    gui_inputkey(SDL_Scancode dflt_key) :
        key(dflt_key), focused(false)
    {}

    gui_inputkey() :
        gui_inputkey(SDL_SCANCODE_UNKNOWN)
    {}
};

struct emu_gui
{
    emu_gui(emu_interface emu, SDL_Rect* screen_rect);
    ~emu_gui();
        
    bool ok() const { return m_ok; }

    void print_dbginfo();

    void set_fps(int fps) { m_fps = fps; }

    void set_delta_t(float delta_t)
    {
        m_deltat = delta_t;
        m_deltat_min = std::min(m_deltat_min, delta_t);
        m_deltat_max = std::max(m_deltat_max, delta_t);
    }

    bool showing_sidepanel() { return m_cur_panel != 0; }

    bool process_event(SDL_Event* e);
    bool want_keyboard();
    bool want_mouse();

    void draw();

private: 
    void draw_help_content();
    void draw_settings_content();
    void draw_panel(const char* title, void(emu_gui::*draw_content)());

    void draw_inputkey(const char* label, gui_inputkey& state);

private:
    emu_interface m_emu;
    ImFont* m_hdr_font;
    ImFont* m_txt_font;
    int m_menubar_height;
    int m_cur_panel;
    SDL_Scancode m_lastkeypress;

    int m_fps;
    float m_deltat;
    float m_deltat_min;
    float m_deltat_max;

    gui_inputkey m_keywgt_p1left;
    gui_inputkey m_keywgt_p1right;
    gui_inputkey m_keywgt_p1fire;

    gui_inputkey m_keywgt_p2left;
    gui_inputkey m_keywgt_p2right;
    gui_inputkey m_keywgt_p2fire;

    gui_inputkey m_keywgt_1pstart;
    gui_inputkey m_keywgt_2pstart;
    gui_inputkey m_keywgt_coinslot;

    bool m_ok;
};


#endif