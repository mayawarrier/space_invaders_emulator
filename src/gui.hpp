
#ifndef GUI_HPP
#define GUI_HPP

#include <SDL.h>
#include <imgui.h>
#include <cmath>
#include <queue>

#include "utils.hpp"
#include "emu.hpp"


enum gui_view
{
    VIEW_GAME,
    VIEW_SETTINGS,
    VIEW_ABOUT,

    NUM_VIEWS
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

enum gui_font_type
{
    FONT_MENUBAR,
    FONT_TXT,
    FONT_HDR,

    NUM_FONT_TYPES
};

using gui_fontatlas = std::unordered_map<int, ImFont*>;

#ifdef __EMSCRIPTEN__
enum gui_playerselect
{
    PLAYER_SELECT_NONE = 0,
    PLAYER_SELECT_1P = 1,
    PLAYER_SELECT_2P = 2
};

enum gui_ctrls_state
{
    CTRLS_PLAYER_SELECT,
    CTRLS_GAME
};

extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_fire(bool pressed);
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_left(bool pressed);
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_right(bool pressed);
extern "C" EMSCRIPTEN_KEEPALIVE void web_click_1p(void);
extern "C" EMSCRIPTEN_KEEPALIVE void web_click_2p(void);
#endif

struct emu_gui
{
    emu_gui(
        const fs::path& assetdir,
        SDL_Window* window, 
        SDL_Renderer* renderer, 
        emu_interface emu);
    ~emu_gui();

    bool ok() const { return m_ok; }

    // Process an input event.
    bool process_event(const SDL_Event* event, gui_captureinfo& out_capture);

    // Get size/layout info for a frame drawn 
    // at a given display size.
    gui_sizeinfo get_sizeinfo(SDL_Point disp_size) const;

    // Get the current view.
    int current_view() const { return m_cur_view; }

    // Run GUI for one frame.
    void run(SDL_Point display_size, const SDL_Rect& viewport);

    static void log_dbginfo();

private: 
    void draw_about_content();
    void draw_settings_content();

    int draw_menubar(const SDL_Rect& viewport);

    void draw_view(const char* title, const SDL_Rect& viewport, 
        void(emu_gui::*draw_content)(), bool* p_closed);

    void draw_header(const char* title, gui_align align = ALIGN_LEFT);

    void draw_ctrlpanel(const char* id, const char* title,
        const std::pair<const char*, input> inputs[], int num_inputs, float panelsizeX);

    int init_fontatlas(const char* ttf_filepath);
    ImFont* get_font_px(int size) const;
    ImFont* get_font(gui_font_type type, SDL_Point disp_size) const;

#ifdef __EMSCRIPTEN__
    void handle_touchctrls_state();

    friend void web_touch_fire(bool pressed);
    friend void web_touch_left(bool pressed);
    friend void web_touch_right(bool pressed);
    friend void web_click_1p(void);
    friend void web_click_2p(void);
#endif

private:
    emu_interface m_emu;
    SDL_Renderer* m_renderer;
    gui_fontatlas m_fontatlas;
    ImFont* m_fonts[NUM_FONT_TYPES];
    int m_cur_view;

    SDL_Scancode m_lastkeypress; // cur frame
    bool m_inputkey_focused[NUM_INPUTS];

    bool m_touchenabled;

#ifdef __EMSCRIPTEN__
    int m_playerselinp_idx;
    gui_playerselect m_playersel;
    gui_ctrls_state m_ctrls_showing;
#endif
    bool m_anykeypress;
    bool m_drawingframe;
    
    bool m_ok;
};

#endif