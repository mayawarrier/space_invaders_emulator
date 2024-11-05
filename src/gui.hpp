
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>

#include "utils.hpp"
#include "emu.hpp"


enum gui_panel_t
{
    PANEL_NONE,
    PANEL_SETTINGS,
    PANEL_HELP
};

struct emu_gui
{
    emu_gui(emu_interface emu);
    ~emu_gui();

    bool ok() const { return m_ok; }

    bool process_event(SDL_Event* e);

    // True if GUI wants keyboard inputs.
    bool want_keyboard() const;
    // True if GUI wants mouse inputs.
    bool want_mouse() const;

    // Run the GUI for one frame.
    void run_frame();

    int menubar_height() const { return m_menubar_height; }

    int panel_size() const;

    void set_fps(int fps) { m_fps = fps; }

    static void log_dbginfo();

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
    gui_panel_t m_cur_panel;
    int m_fps;

    SDL_Scancode m_lastkeypress; // cur frame
    bool m_inputkey_focused[INPUT_NUM_INPUTS];

    bool m_ok;
};


#endif