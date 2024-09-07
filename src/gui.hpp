
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>

#include "utils.hpp"

struct inputkey_widget
{
    const char* label;
    SDL_Scancode key;
    bool focused;

    inputkey_widget(const char* label, SDL_Scancode dflt_key) :
        label(label), key(dflt_key), focused(false)
    {}

    inputkey_widget() :
        inputkey_widget("", SDL_SCANCODE_UNKNOWN)
    {}

    // If last_keypress set to 0 after this call, it
    // indicates that the key input is consumed
    void draw(SDL_Scancode& last_keypress);
};

struct emu;

struct emu_interface
{
    emu_interface(emu* e) :
        m_emu(e)
    {}

private:
    emu* m_emu;
};

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

    bool showing_sidepanel() { return m_cur_panel != 0; }

    bool process_event(SDL_Event* e);
    bool want_keyboard();
    bool want_mouse();

    void draw();

private:
    void draw_panel(const char* title, void(emu_gui::* content_cb)());
    void draw_help_content();
    void draw_settings_content();

private:
    emu* m_emu;
    ImFont* m_hdr_font;
    ImFont* m_txt_font;
    int m_menubar_height;
    int m_cur_panel;
    SDL_Scancode m_lastkeypress;

    inputkey_widget m_keywgt_p1left;
    inputkey_widget m_keywgt_p1right;
    inputkey_widget m_keywgt_p1fire;

    inputkey_widget m_keywgt_p2left;
    inputkey_widget m_keywgt_p2right;
    inputkey_widget m_keywgt_p2fire;

    inputkey_widget m_keywgt_1pstart;
    inputkey_widget m_keywgt_2pstart;
    inputkey_widget m_keywgt_coinslot;

    int m_fps;
    float m_deltat;
    float m_deltat_min;
    float m_deltat_max;
};


#endif