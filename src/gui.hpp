
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>
#include <cmath>
#include <queue>

#include "utils.hpp"
#include "emu.hpp"


enum gui_panel
{
    PANEL_NONE,
    PANEL_SETTINGS,
    PANEL_ABOUT
};

enum gui_align
{
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_CENTER
};

struct gui_captureinfo
{
    // True if GUI wants to capture keyboard inputs.
    bool capture_keyboard;
    // True if GUI wants to capture mouse inputs.
    bool capture_mouse;

    gui_captureinfo() : 
        capture_keyboard(false), 
        capture_mouse(false)
    {}
};

struct gui_sizeinfo
{
    // Viewport offset from window origin.
    SDL_Point vp_offset;
    // Size reserved for GUI elements inside the window.
    SDL_Point resv_inwnd_size;
    // Size reserved for GUI elements outside the window.
    SDL_Point resv_outwnd_size;
};

struct gui_fonts
{
    ImFont* txt_font;
    ImFont* hdr_font;
    ImFont* subhdr_font;
};

using gui_fontatlas = std::unordered_map<int, ImFont*>;

struct emu_gui
{
    emu_gui(
        SDL_Window* window, 
        SDL_Renderer* renderer, 
        emu_interface emu);
    ~emu_gui();

    bool ok() const { return m_ok; }

    // Process an input event.
    bool process_event(const SDL_Event* event, gui_captureinfo& out_capture);

    // Get size/layout info for a frame drawn 
    // at the given display size.
    gui_sizeinfo frame_sizeinfo(SDL_Point disp_size) const;

    // Run GUI for one frame.
    void run(SDL_Point display_size, const SDL_Rect& viewport);

    // Check if a GUI page is currently visible.
    bool is_page_visible() const { return m_cur_panel != PANEL_NONE; }

    void set_delta_t(float delta_t)
    {
        m_fps = int(std::lroundf(1.f / delta_t));
    }

    static void log_dbginfo();

private: 
    void draw_help_content();
    void draw_settings_content();

    void draw_panel(const char* title, 
        const SDL_Rect& viewport, void(emu_gui::*draw_content)());

    void draw_inputkey(const char* label, input inptype,
        gui_align align = ALIGN_LEFT, float labelsizeX = -1);

    int init_fontatlas();

    ImFont* get_font_px(int size) const;
    ImFont* get_font_vh(float vmin, SDL_Point disp_size) const;

#ifdef __EMSCRIPTEN__
    friend void emcc_queue_touch(touchinput inp, bool pressed);
#endif

private:
    emu_interface m_emu;
    SDL_Renderer* m_renderer;
    gui_fontatlas m_fontatlas;
    gui_fonts m_fonts;
    int m_cur_panel;
    int m_fps;

    SDL_Scancode m_frame_lastkeypress; // cur frame
    bool m_inputkey_focused[NUM_INPUTS];

    bool m_anykeypress;
    bool m_drawingframe;
    
    bool m_ok;
};

#endif