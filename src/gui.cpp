
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "emu.hpp"
#include "gui.hpp"

#if __has_include("buildnum.h")
#include "buildnum.h"
#endif

PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
// good for learning Imgui features
int demo_window();
#endif

static constexpr ImU32 WND_BGCOLOR_IM32 = IM_COL32(0, 0, 0, 255);
static constexpr color PRIMARY_COLOR(0x1E, 0xFE, 0x1E, 0xFF);

static constexpr ImGuiWindowFlags WND_DEFAULT_FLAGS = 
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoNavInputs;

// For emscripten there can only be one window
#ifdef __EMSCRIPTEN__
static emu_gui* ACTIVE_GUI = nullptr;
#endif


static const char* build_num()
{
#if defined(BUILD_NUM)
    const char* value = XSTR(BUILD_NUM);
    return std::string_view(value) == "0" ? nullptr : value;
#else
    return nullptr;
#endif
}

static constexpr int MIN_FONT_SIZE = 5;
static constexpr int MAX_FONT_SIZE = 50;

struct symbol_entry
{
    SDL_Scancode scancode;
    ImWchar codepoint;
    const char* utf8;
};

static const symbol_entry FONT_SYMBOLS[] =
{
    { SDL_SCANCODE_LEFT,  0x2190, (const char*)u8"\u2190" }, // left arrow
    { SDL_SCANCODE_RIGHT, 0x2192, (const char*)u8"\u2192" }, // right arrow
    { SDL_SCANCODE_UP,    0x2191, (const char*)u8"\u2191" }, // up arrow
    { SDL_SCANCODE_DOWN,  0x2193, (const char*)u8"\u2193" }  // down arrow
};

static const char* scancode_symbol(SDL_Scancode sc)
{
    // linear search okay, very few symbols
    for (int i = 0; i < IM_ARRAYSIZE(FONT_SYMBOLS); ++i) {
        if (FONT_SYMBOLS[i].scancode == sc) {
            return FONT_SYMBOLS[i].utf8;
        }
    }
    return nullptr;
}

int emu_gui::init_fontatlas(const char* ttf_filepath)
{
    logMESSAGE("Building font atlas");

    ImGuiIO& io = ImGui::GetIO();

    ImVector<ImWchar> glyph_ranges;
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        
        for (int i = 0; i < IM_ARRAYSIZE(FONT_SYMBOLS); ++i) {
            builder.AddChar(FONT_SYMBOLS[i].codepoint);
        }
        builder.BuildRanges(&glyph_ranges);
    }
    
    for (int i = MIN_FONT_SIZE; i <= MAX_FONT_SIZE; ++i)
    {
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            ttf_filepath, i, nullptr, glyph_ranges.Data);

        m_fontatlas.insert({ i, font });
    }
    
    if (!io.Fonts->Build()) {
        logERROR("Failed to build font atlas");
        return -1;
    }
    return 0;
}

ImFont* emu_gui::get_font_px(int size) const
{
    if (size < MIN_FONT_SIZE) {
        size = MIN_FONT_SIZE;
    } else if (size > MAX_FONT_SIZE) {
        size = MAX_FONT_SIZE;
    }
    return m_fontatlas.at(size);
}

static int vh_to_px(float vh, SDL_Point disp_size) {
    return int(std::lroundf(vh * disp_size.y / 100.f));
}

static int get_font_px_size(gui_font_type type, SDL_Point disp_size)
{
    float vh = 0;
    switch (type)
    {
    case FONT_TXT:
        vh = is_emscripten() ? (disp_size.y < disp_size.x ? 1.95f : 2.05f) : 1.70f;
        break;
    case FONT_MENUBAR: 
        vh = is_emscripten() ? (disp_size.y < disp_size.x ? 1.85f : 2.05f) : 1.6f; 
        break;
    case FONT_HDR: 
        vh = is_emscripten() ? 2.51f : 2.1; 
        break;

    default: SDL_assert(false);
    }
    return vh_to_px(vh, disp_size);
}

ImFont* emu_gui::get_font(gui_font_type type, SDL_Point disp_size) const
{
    return get_font_px(get_font_px_size(type, disp_size));
}

static bool touch_supported()
{
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT(return Module.touchType != 0);
#else
    return false;
#endif
}

emu_gui::emu_gui(
    const fs::path& assetdir,
    SDL_Window* window, 
    SDL_Renderer* renderer, 
    emu_interface emu
) :
    m_emu(emu),
    m_renderer(renderer),
    m_cur_view(VIEW_GAME),
    m_lastkeypress(SDL_SCANCODE_UNKNOWN),
    m_touchenabled(touch_supported()),
#ifdef __EMSCRIPTEN__
    m_playerselinp_idx(0),
    m_playersel(PLAYER_SELECT_NONE),
    m_ctrls_showing(CTRLS_PLAYER_SELECT),
#endif    
    m_anykeypress(false),
    m_drawingframe(false),
    m_ok(false)
{
    //demo_window();

#ifdef __EMSCRIPTEN__
    ACTIVE_GUI = this;
#endif

    logMESSAGE("Initializing GUI");

    std::fill_n(m_fonts, NUM_FONT_TYPES, nullptr);
    std::fill_n(m_inputkey_focused, NUM_INPUTS, false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer) ||
        !ImGui_ImplSDLRenderer2_Init(renderer)) {
        logERROR("Failed to initialize ImGui with SDL backend");
        return;
    }
    if (init_fontatlas((assetdir / "CascadiaMono.ttf").string().c_str()) != 0) {
        return;
    }

    if (m_touchenabled) {
        logMESSAGE("Enabled touch controls");

#ifdef __EMSCRIPTEN__
        char* touch_str = (char*)EM_ASM_PTR(return Module.touchTypeString());
        logMESSAGE("Touch control type: %s", touch_str);
        std::free(touch_str);

        EM_ASM(Module.showPlayerSelectControls());
#endif
    }
    m_ok = true;
}

emu_gui::~emu_gui()
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void emu_gui::log_dbginfo()
{
    logMESSAGE("ImGui version: %s", ImGui::GetVersion());
}

bool emu_gui::process_event(const SDL_Event* e, gui_captureinfo& out_ci)
{
    if (e->type == SDL_KEYUP) {
        m_lastkeypress = e->key.keysym.scancode;
        m_anykeypress = true;
    }
    bool ret = ImGui_ImplSDL2_ProcessEvent(e); 

    auto& io = ImGui::GetIO();
    out_ci.capture_keyboard = io.WantCaptureKeyboard;
    out_ci.capture_mouse = io.WantCaptureMouse;

    return ret;
}

#ifdef __EMSCRIPTEN__

extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_fire(bool pressed)
{
    ACTIVE_GUI->m_emu.send_input(INPUT_P1_FIRE, pressed);
    ACTIVE_GUI->m_emu.send_input(INPUT_P2_FIRE, pressed);
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_left(bool pressed)
{
    ACTIVE_GUI->m_emu.send_input(INPUT_P1_LEFT, pressed);
    ACTIVE_GUI->m_emu.send_input(INPUT_P2_LEFT, pressed);
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_right(bool pressed)
{
    ACTIVE_GUI->m_emu.send_input(INPUT_P1_RIGHT, pressed);
    ACTIVE_GUI->m_emu.send_input(INPUT_P2_RIGHT, pressed);
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_click_1p(void)
{
    if (ACTIVE_GUI->m_playersel == PLAYER_SELECT_NONE) {
        ACTIVE_GUI->m_playersel = PLAYER_SELECT_1P;
    }
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_click_2p(void)
{
    if (ACTIVE_GUI->m_playersel == PLAYER_SELECT_NONE) {
        ACTIVE_GUI->m_playersel = PLAYER_SELECT_2P;
    }
}

using gui_inputvec = std::vector<std::pair<input, bool>>;

// inputs to start 1P or 2P, frame by frame
static const std::vector<gui_inputvec> player_select_inputs[2] = {
    { // 1p
        { { INPUT_CREDIT, true } },
        { { INPUT_CREDIT, false }, { INPUT_1P_START, true } }
    },
    { // 2p
        { { INPUT_CREDIT, true } },
        { { INPUT_CREDIT, false } },
        { { INPUT_CREDIT, true } },
        { { INPUT_CREDIT, false }, { INPUT_2P_START, true } }
    }
};

void emu_gui::handle_touchctrls_state()
{
    SDL_assert(m_touchenabled);

    if (m_emu.in_demo_mode()) {
        if (m_ctrls_showing != CTRLS_PLAYER_SELECT) {
            EM_ASM(Module.showPlayerSelectControls());
            m_ctrls_showing = CTRLS_PLAYER_SELECT;
        }
    }
    else if (m_ctrls_showing != CTRLS_GAME) {
        EM_ASM(Module.showGameControls());
        m_ctrls_showing = CTRLS_GAME;
    }

    if (m_playersel != PLAYER_SELECT_NONE)
    {
        if (m_cur_view == VIEW_GAME)
        {
            auto& inputs = player_select_inputs[m_playersel - 1];
            if (m_playerselinp_idx < inputs.size())
            {
                auto& frame_inputs = inputs[m_playerselinp_idx];
                for (auto& inp : frame_inputs) {
                    m_emu.send_input(inp.first, inp.second);
                }
                m_playerselinp_idx++;
            }
            else {
                m_playersel = PLAYER_SELECT_NONE;
                m_playerselinp_idx = 0;
            }
        }
        else { m_cur_view = VIEW_GAME; } // next frame
    }
}
#endif 

static void setposX_right_align(const char* text)
{
    // get start pos for right alignment
    float txtsizeX = ImGui::CalcTextSize(text).x;
    float txtpos = ImGui::GetWindowSize().x - txtsizeX - ImGui::GetStyle().WindowPadding.x;
    ImGui::SetCursorPosX(txtpos);
}

static void setposX_center_align(const char* text)
{
    ImVec2 txt_size = ImGui::CalcTextSize(text);
    float txt_pos = (ImGui::GetWindowSize().x - txt_size.x) / 2;
    ImGui::SetCursorPosX(txt_pos);
}

template <typename ...Args>
static void draw_rtalign_text(const char* fmt, Args&&... args)
{
    const char* ptxt;
    char txtbuf[128]; // more than enough for anybody :)
    if (sizeof...(args) == 0) {
        ptxt = fmt;
    } else {
        std::snprintf(txtbuf, 128, fmt, args...);
        ptxt = txtbuf;
    }

    setposX_right_align(ptxt);
    ImGui::TextUnformatted(ptxt);
}

static float get_scrollbar_width()
{
    return ImGui::GetScrollMaxY() > 0.0f ? ImGui::GetStyle().ScrollbarSize : 0.0f;
}

static void draw_closebutton(const char* id, bool* p_closed, bool is_touchscreen)
{
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpadding = ImGui::GetStyle().WindowPadding;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    constexpr float btnmarginX = 4.f, btnmarginY = 4.f;
    float btn_ypos = dpos.y - wndpadding.y + btnmarginY;
    float btn_xpos_rt = dpos.x + wndsize.x - (get_scrollbar_width() + wndpadding.x + btnmarginX);
    
    bool clicked;
    
    ImGui::PushStyleColor(ImGuiCol_Button, PRIMARY_COLOR.alpha(0.3).to_imcolor());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PRIMARY_COLOR.alpha(0.5).to_imcolor());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, PRIMARY_COLOR.alpha(0.7).to_imcolor());
    {       
        if (is_touchscreen) { 
            // touchscreens have a hard time with small buttons so draw it larger 
            const char* btn_text = "Back";
            constexpr float btnpadding = 11.f;
            float btnwidth = ImGui::CalcTextSize(btn_text).x + 2 * btnpadding;
            ImVec2 btnpos = ImVec2(btn_xpos_rt - btnwidth, btn_ypos);

            ImGui::PushID(id);
            ImGui::PushStyleColor(ImGuiCol_Text, PRIMARY_COLOR.to_imcolor());
            {
                ImGui::SetCursorScreenPos(btnpos);
                clicked = ImGui::Button(btn_text, ImVec2(btnwidth, ImGui::GetFrameHeight()));
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        else {
            constexpr float cross_size = 12.f;
            constexpr float btnpadding = 3.f;
            constexpr float btnsize = cross_size + 2 * btnpadding;

            ImVec2 btnpos = ImVec2(btn_xpos_rt - btnsize, btn_ypos);
            ImVec2 crosspos = ImVec2(btnpos.x + btnpadding, btnpos.y + btnpadding);

            ImGui::PushID(id);
            ImGui::SetCursorScreenPos(btnpos);
            clicked = ImGui::Button("##wndclose", ImVec2(btnsize, btnsize));
            ImGui::PopID();

            color cross_basecol = PRIMARY_COLOR;
            ImU32 crosscol = (ImGui::IsItemHovered() ? cross_basecol.brighter(50) : cross_basecol).to_imcolor();

            ImVec2 p2 = ImVec2(crosspos.x + cross_size, crosspos.y + cross_size);
            ImVec2 p3 = ImVec2(crosspos.x, crosspos.y + cross_size);
            ImVec2 p4 = ImVec2(crosspos.x + cross_size, crosspos.y);
            draw_list->AddLine(crosspos, p2, crosscol, 2.0f);
            draw_list->AddLine(p3, p4, crosscol, 2.0f);
        }
    }
    ImGui::PopStyleColor(3);

    *p_closed = clicked;
    ImGui::SetCursorScreenPos(dpos);
}

static void draw_header(const char* title, gui_align align, ImFont* font)
{
    ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, PRIMARY_COLOR.to_imcolor());
    {
        switch (align)
        {
        case ALIGN_RIGHT:  setposX_right_align(title); break;
        case ALIGN_CENTER: setposX_center_align(title); break;
        default: break;
        }
        ImGui::TextUnformatted(title);
    }
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void emu_gui::draw_header(const char* title, gui_align align)
{
    ::draw_header(title, align, m_fonts[FONT_HDR]);
}

static std::string format_escape(const char* str)
{
    std::string ret;
    for (const char* p = str; *p; ++p) {
        if (*p == '%') {
            ret += "%%";
        } else {
            ret += *p;
        }
    }
    return ret;
}

static void draw_url(const char* text, const char* url, int id_seed = 0)
{
    const ImU32 color = PRIMARY_COLOR.alpha(0.6).to_imcolor();

    float posX = ImGui::GetCursorPosX();
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    ImVec2 txtsize = ImGui::CalcTextSize(text);

    ImGui::PushID(id_seed);
    if (ImGui::InvisibleButton(url, txtsize)) {
        if (SDL_OpenURL(url) != 0) {
            logERROR("Could not open URL %s", url);
        }
    }
    ImGui::PopID();
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::SetTooltip(format_escape(url).c_str());
        ImGui::PopStyleVar();
    }
    ImGui::SameLine();

    ImGui::SetCursorPosX(posX);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();

    // Draw underline
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(dpos.x, dpos.y + txtsize.y + 1),
        ImVec2(dpos.x + txtsize.x, dpos.y + txtsize.y + 1),
        color, 1.0f);
}

static bool draw_dip_switch(int index, bool value, float width = -1)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    width = width < 0 ? ImGui::GetFrameHeight() * 2 : width;
    float height = width * 1.55f;

    ImGui::PushID(index);
    if (ImGui::InvisibleButton("DIP", ImVec2(width, height))) {
        value = !value;
    }
    ImGui::PopID();

    ImVec2 slide_dpos_min = value ? dpos : ImVec2(dpos.x, dpos.y + height / 2);
    ImVec2 slide_dpos_max = slide_dpos_min + ImVec2(width, height / 2);

    draw_list->AddRectFilled(slide_dpos_min, slide_dpos_max, 
        PRIMARY_COLOR.alpha(value ? 1 : 0.75).to_imcolor(), 4.0f);

    draw_list->AddRect(dpos, ImVec2(dpos.x + width, dpos.y + height),
        PRIMARY_COLOR.alpha(ImGui::IsItemHovered() ? 0.7 : 0.33).to_imcolor(), 4.f);

    // glow effect
    if (value) {
        ImVec2 hilight_off(width / 6, 0.134f * height);
        draw_list->AddRectFilled(slide_dpos_min + hilight_off,
            slide_dpos_max - hilight_off, IM_COL32(255, 255, 255, 0.2f * 255), 4.0f);
    }

    return value;
}

struct inputkey_stateptrs
{
    bool* focused;
    SDL_Scancode* key;
};

static const char* scancode_string(SDL_Scancode sc)
{
    if (sc == SDL_SCANCODE_RETURN) {
        return "Enter";
    }  
    else {
        const char* name = scancode_symbol(sc);
        if (!name) {
            name = SDL_GetScancodeName(sc);
        }
        return name;
    }
}

static void draw_inputkey(int id, inputkey_stateptrs state, 
    SDL_Scancode keyvalue, gui_align align = ALIGN_LEFT, float min_inputwidth = 80)
{
    SDL_assert(align == ALIGN_LEFT || align == ALIGN_RIGHT);

    bool& focused = *state.focused;
    SDL_Scancode& key = *state.key;

    ImGuiStyle style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpadding = style.WindowPadding;

    static const char* enterkey_prompt = "Press key...";
    const char* keystr = scancode_string(key);

    float textsize = ImGui::CalcTextSize(focused ? enterkey_prompt : keystr).x;
    float inputwidth = std::max(min_inputwidth, textsize + 4 * style.FramePadding.x);
    float inputposX = align == ALIGN_LEFT ? wndpadding.x : wndsize.x - wndpadding.x - inputwidth;

    ImVec2 inputpos(inputposX, ImGui::GetCursorPosY() - style.FramePadding.y);
    ImVec2 inputsize(inputwidth, ImGui::GetFontSize() + style.FramePadding.y * 2);

    ImGui::SetCursorPos(inputpos);
    ImVec2 dpos = ImGui::GetCursorScreenPos();

    // selectable to determine focus
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    ImGui::Selectable("", &focused, 0, inputsize);
    ImGui::PopStyleColor(3);
    ImGui::PopID();

    bool is_hovered = ImGui::IsItemHovered();
    bool next_frame_unfocused = focused && !is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (focused) {
        draw_list->AddRectFilled(dpos, dpos + inputsize, PRIMARY_COLOR.alpha(0.3).to_imcolor());
        draw_list->AddRect(dpos, dpos + inputsize, PRIMARY_COLOR.to_imcolor());

        ImVec2 frameoff = ImVec2(style.FramePadding.x, style.FramePadding.y / 2);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), 
            dpos + frameoff, PRIMARY_COLOR.to_imcolor(), enterkey_prompt);
    } 
    else {
        draw_list->AddRect(dpos, dpos + inputsize, PRIMARY_COLOR.alpha(is_hovered ? 0.6 : 0.33).to_imcolor());

        ImVec2 frameoff = ImVec2((inputwidth - ImGui::CalcTextSize(keystr).x) / 2, style.FramePadding.y / 2);
        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), 
            dpos + frameoff, IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), keystr);
    }

    if (focused && keyvalue != SDL_SCANCODE_UNKNOWN) {
        key = keyvalue;
        next_frame_unfocused = true;
    }

    // update for next frame
    if (focused) {
        if (next_frame_unfocused) {
            focused = false;
            ImGui::SetNextFrameWantCaptureKeyboard(false);
        }
        else ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
}

void emu_gui::draw_ctrlpanel(const char* id, const char* title, 
    const std::pair<const char*, input> inputs[], int num_inputs, float panelsizeX)
{
    auto& style = ImGui::GetStyle();

    ImGui::PushStyleColor(ImGuiCol_Border, PRIMARY_COLOR.alpha(0.33).to_imcolor());
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 7));
    {
        static constexpr float btm_padding = 8;

        // attempt to calculate the panel size, this seems close enough
        int num_lines = title ? num_inputs + 1 : num_inputs;
        float panelsizeY =
            num_lines * (ImGui::GetFontSize() + style.ItemSpacing.y + style.FramePadding.y) +
            (2 * style.WindowPadding.y + style.FramePadding.y) + btm_padding;

        static constexpr ImGuiWindowFlags wnd_flags =
            WND_DEFAULT_FLAGS |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
            
        ImGui::BeginChild(id, ImVec2(panelsizeX, panelsizeY), ImGuiChildFlags_Border, wnd_flags);
        {
            ImGui::Spacing();

            if (title) {
                ::draw_header(title, ALIGN_LEFT, m_fonts[FONT_TXT]);
                ImGui::Spacing();
            }

            for (int i = 0; i < num_inputs; ++i)
            {
                auto& inp = inputs[i];

                ImGui::TextUnformatted(inp.first);
                ImGui::SameLine();

                inputkey_stateptrs stateptrs = {
                    .focused = &m_inputkey_focused[inp.second],
                    .key = &m_emu.input2keymap()[inp.second]
                };
                draw_inputkey(i, stateptrs, m_lastkeypress, ALIGN_RIGHT);
            }

            ImGui::Dummy(ImVec2(0, btm_padding));
        }
        ImGui::EndChild();
    };
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor();
}

static int draw_volume_slider(int volume)
{    
    auto& style = ImGui::GetStyle();
    auto* draw_list = ImGui::GetWindowDrawList();

    float icon_width = ImGui::GetFrameHeight();
    float max_label_width = ImGui::CalcTextSize("100").x;

    // Draw speaker icon
    {
        auto pos = ImGui::GetCursorScreenPos() + style.FramePadding;

        constexpr float thickness = 2.0f;
        float scale = icon_width / 30.0f;
        ImU32 color = IM_COL32(255, 255, 255, 255);

        ImVec2 speaker[] = {
            ImVec2(pos.x + 11.0f * scale, pos.y + 5.0f * scale),
            ImVec2(pos.x + 6.0f * scale, pos.y + 9.0f * scale),
            ImVec2(pos.x + 2.0f * scale, pos.y + 9.0f * scale),
            ImVec2(pos.x + 2.0f * scale, pos.y + 15.0f * scale),
            ImVec2(pos.x + 6.0f * scale, pos.y + 15.0f * scale),
            ImVec2(pos.x + 11.0f * scale, pos.y + 19.0f * scale)
        };
        draw_list->AddPolyline(speaker, 6, color, true, thickness);

        if (volume == 0)
        {
            float cross_size = 9.0f * scale;
            ImVec2 center = ImVec2(pos.x + 20.0f * scale, pos.y + 12.0f * scale);

            draw_list->AddLine(
                ImVec2(center.x - cross_size * 0.5f, center.y - cross_size * 0.5f),
                ImVec2(center.x + cross_size * 0.5f, center.y + cross_size * 0.5f),
                color, thickness);

            draw_list->AddLine(
                ImVec2(center.x + cross_size * 0.5f, center.y - cross_size * 0.5f),
                ImVec2(center.x - cross_size * 0.5f, center.y + cross_size * 0.5f),
                color, thickness);
        }
        else {
            // Inner wave
            draw_list->PathLineTo(ImVec2(pos.x + 14.0f * scale, pos.y + 10.0f * scale));
            draw_list->PathBezierCubicCurveTo(
                ImVec2(pos.x + 16.0f * scale, pos.y + 7.0f * scale),
                ImVec2(pos.x + 16.0f * scale, pos.y + 17.0f * scale),
                ImVec2(pos.x + 14.0f * scale, pos.y + 14.0f * scale));
            draw_list->PathStroke(color, false, thickness);

            if (volume >= 33)
            {
                // Middle wave
                draw_list->PathLineTo(ImVec2(pos.x + 17.0f * scale, pos.y + 7.0f * scale));
                draw_list->PathBezierCubicCurveTo(
                    ImVec2(pos.x + 20.0f * scale, pos.y + 4.0f * scale),
                    ImVec2(pos.x + 20.0f * scale, pos.y + 20.0f * scale),
                    ImVec2(pos.x + 17.0f * scale, pos.y + 17.0f * scale));
                draw_list->PathStroke(color, false, thickness);
            }

            if (volume >= 66)
            {
                // Outer wave
                draw_list->PathLineTo(ImVec2(pos.x + 20.0f * scale, pos.y + 4.0f * scale));
                draw_list->PathBezierCubicCurveTo(
                    ImVec2(pos.x + 23.5f * scale, pos.y + 2.0f * scale),
                    ImVec2(pos.x + 23.5f * scale, pos.y + 22.0f * scale),
                    ImVec2(pos.x + 20.0f * scale, pos.y + 20.0f * scale));
                draw_list->PathStroke(color, false, thickness);
            }
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + icon_width);
    }

    // Margin after icon and before volume text
    float slider_margin_left = style.ItemSpacing.x * 2.5;
    float slider_margin_right = style.ItemSpacing.x / 2;

    // Draw volume slider
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slider_margin_left);

        float btnheight = ImGui::GetFrameHeight();
        float slider_radius = 0.8 * btnheight / 2;

        float btnwidth = ImGui::GetWindowSize().x -
            (slider_margin_left + slider_margin_right + slider_radius +
                icon_width + max_label_width + 2 * style.WindowPadding.x + style.FramePadding.x);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - slider_radius);
        ImGui::InvisibleButton("##volume", ImVec2(btnwidth + 2 * slider_radius, btnheight));

        float sliderbg_height = 0.35f * btnheight;
        float sliderbg_heightoff = (btnheight - sliderbg_height) / 2;

        ImVec2 sliderbg_min = ImGui::GetItemRectMin() + ImVec2(slider_radius, 0);
        ImVec2 sliderbg_max = ImGui::GetItemRectMax() - ImVec2(slider_radius, 0);
        ImVec2 sliderbg_startpos = sliderbg_min + ImVec2(0, sliderbg_heightoff);
        ImU32 sliderbg_col = ImGui::IsItemHovered() || ImGui::IsItemActive() ? 
            IM_COL32(95, 95, 95, 255) : IM_COL32(70, 70, 70, 255);

        float slider_posX = sliderbg_min.x + btnwidth * volume / 100;

        draw_list->AddRectFilled(sliderbg_startpos,
            sliderbg_max - ImVec2(0, sliderbg_heightoff), sliderbg_col, 10);

        draw_list->AddRectFilled(sliderbg_startpos,
            ImVec2(slider_posX, sliderbg_max.y - sliderbg_heightoff),
            IM_COL32(255, 255, 255, 255), 10);

        ImVec2 slider_pos = ImVec2(slider_posX, sliderbg_min.y + ImGui::GetItemRectSize().y / 2);

        draw_list->AddCircleFilled(slider_pos, slider_radius, PRIMARY_COLOR.to_imcolor());
        draw_list->AddCircle(slider_pos, slider_radius, IM_COL32(0, 0, 0, 255), 0, 2.0f);

        if (ImGui::IsItemActive())
        {
            float new_volume = (ImGui::GetMousePos().x - sliderbg_min.x) / btnwidth * 100;
            volume = std::clamp(int(new_volume), 0, 100);
        }

        char volumestr[4] = { 0 };
        std::to_chars(volumestr, volumestr + 4, volume);

        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetCursorPosX() + slider_margin_right + 
                max_label_width - ImGui::CalcTextSize(volumestr).x - style.ItemSpacing.x / 2,
            ImGui::GetCursorPosY() + style.FramePadding.y / 1.5f
        ));
    }

    ImGui::Text("%d", volume);

    return volume;
}

void emu_gui::draw_settings_content()
{
    auto& style = ImGui::GetStyle();

    // DIP Switches section
    {
        draw_header("DIP Switches");
        ImGui::Dummy(ImVec2(0, 10));

        constexpr float sw_spacingX = 20;

        float sw_width = 2 * ImGui::GetFrameHeight();
        float all_sw_width = 5 * sw_width + 4 * sw_spacingX;
        float sw_startposX = (ImGui::GetWindowSize().x - all_sw_width) / 2;
        
        ImGui::SetCursorPosX(sw_startposX);

        ImGui::PushFont(get_font_px(int(sw_width / 2.25)));
        {
            float sw_txtpos[5];
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(sw_spacingX, 0));
            for (int i = 7; i >= 3; --i)
            {
                char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
                sw_txtpos[i - 3] = ImGui::GetCursorPosX() + 
                    (sw_width - ImGui::CalcTextSize(sw_name).x) / 2;

                bool cur_val = m_emu.get_switch(i);
                bool new_val = draw_dip_switch(i, cur_val, sw_width);
                m_emu.set_switch(i, new_val);

                if (i != 3) {
                    ImGui::SameLine();
                }
            }
            ImGui::PopStyleVar();

            for (int i = 7; i >= 3; --i)
            {
                char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
                ImGui::SetCursorPosX(sw_txtpos[i - 3]);
                ImGui::TextUnformatted(sw_name);
                ImGui::SameLine();
            }
        }
        ImGui::PopFont();

        ImGui::NewLine();
        ImGui::NewLine();

        int num_ships = 0;
        set_bit(&num_ships, 0, m_emu.get_switch(3));
        set_bit(&num_ships, 1, m_emu.get_switch(5));
        num_ships += 3;

        ImGui::Text("Number of ships: %d", num_ships);
        ImGui::Text("Extra ship at: %d points", m_emu.get_switch(6) ? 1000 : 1500);
        ImGui::Text("Diagnostics at startup: %s", m_emu.get_switch(4) ? "Enabled" : "Disabled");
        ImGui::Text("Coins in demo screen: %s", m_emu.get_switch(7) ? "No" : "Yes");

        ImGui::NewLine();
    }
    
    // Controls section
    if (!m_touchenabled || m_anykeypress)
    {
        draw_header("Controls");
        ImGui::Dummy(ImVec2(0, 10));

        static constexpr std::pair<const char*, input> inputs[9]
        {
            { "Left ", INPUT_P1_LEFT },
            { "Right", INPUT_P1_RIGHT },
            { "Fire", INPUT_P1_FIRE },
            { "Left", INPUT_P2_LEFT },
            { "Right", INPUT_P2_RIGHT },
            { "Fire", INPUT_P2_FIRE },
            { "1P Start", INPUT_1P_START },
            { "2P Start", INPUT_2P_START },
            { "Insert coin", INPUT_CREDIT }
        };
        
        float panelmargin = 8;
        float panelsizeX = (ImGui::GetWindowSize().x - panelmargin) / 2 - style.WindowPadding.x;

        draw_ctrlpanel("inputkeys1", "Player 1", inputs, 3, panelsizeX); 

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.ItemSpacing.x + panelmargin);

        draw_ctrlpanel("inputkeys2", "Player 2", inputs + 3, 3, panelsizeX);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.ItemSpacing.y + panelmargin);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.WindowPadding.x + (ImGui::GetWindowSize().x - panelsizeX) / 2);

        draw_ctrlpanel("inputkeys3", nullptr, inputs + 6, 3, panelsizeX);

        ImGui::NewLine();
    }

    // Audio section
    {
        draw_header("Audio");
        ImGui::Dummy(ImVec2(0, 10));

        int new_volume = draw_volume_slider(m_emu.get_volume());
        m_emu.set_volume(new_volume);

        ImGui::NewLine();
    }
}

void emu_gui::draw_about_content()
{
    draw_header("About");
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::PushTextWrapPos(ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x);
    {
        if (build_num()) {
            ImGui::Text("Space Invaders Emulator (build %s)", build_num());
        } else {
            ImGui::TextUnformatted("Space Invaders Emulator");
        }

        ImGui::TextUnformatted("Maya Warrier");
        draw_url("mayawarrier.github.io", "https://mayawarrier.github.io/");
        ImGui::NewLine();

        ImGui::TextUnformatted("Source code available at"); ImGui::SameLine();
        draw_url("GitHub", "https://github.com/mayawarrier/space_invaders_emulator/");
        ImGui::TextUnformatted("under the MIT license.\n\n");

        ImGui::NewLine();

        draw_header("How it works");
        ImGui::Dummy(ImVec2(0, 10));

        const char* content =
            "This emulator runs the original Space Invaders arcade game from 1978!\n\n"
            "It recreates the hardware environment the game expects - simulating the CPU, memory, "
            "and I/O devices so the game behaves just like it would on a real arcade machine.\n\n"
            "At the core is a full emulation of the Intel 8080 processor and other essential "
            "chips on the original motherboard.\n\n";

        ImGui::TextUnformatted(content);

        static constexpr std::pair<const char*, const char*> links[] = {
            { "Computer Archeology website", "https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html" },
            { "RadioShack Intel 8080 Manual", "https://archive.org/details/8080-8085_Assembly_Language_Programming_1977_Intel" },
            { "Intel 8080 Datasheet", "https://deramp.com/downloads/intel/8080%20Data%20Sheet.pdf" }
        };

        ImGui::TextUnformatted("Learn more about the hardware:");
        for (auto& link : links) {
            draw_url(link.first, link.second);
        }

        ImGui::NewLine();
    }
    ImGui::PopTextWrapPos();
}

static const char* view_name(int view)
{
    switch (view)
    {
    case VIEW_SETTINGS: return "Settings";
    case VIEW_ABOUT: return "About";
    case VIEW_GAME: return "Game";
    default: return "";
    }
}

static constexpr int MENUBAR_PADDING = 3;

gui_sizeinfo emu_gui::get_sizeinfo(SDL_Point disp_size) const
{
    SDL_assert(!m_drawingframe);

    int menu_height = get_font_px_size(FONT_MENUBAR, disp_size) + // text
        int(ImGui::GetStyle().FramePadding.y * 2.0f) + 2 * MENUBAR_PADDING;  // padding

    float resv_outwnd_ypct = is_emscripten() ? (m_touchenabled ? 0.25f : 0.20f) : 0.1f;
    int resv_outwndY = int(std::lroundf(resv_outwnd_ypct * disp_size.y));

    return {
        .vp_offset = {.x = 0, .y = menu_height },
        .resv_inwnd_size = {.x = 0, .y = menu_height },
        .resv_outwnd_size = {.x = 0, .y = resv_outwndY }
    };
}

gui_view emu_gui::draw_menubar(const SDL_Rect& viewport)
{
    auto& style = ImGui::GetStyle();

    int new_view = m_cur_view;
    ImGui::PushFont(m_fonts[FONT_MENUBAR]);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, MENUBAR_PADDING));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(13, style.FramePadding.y));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 0));
        {
            if (ImGui::BeginChild("menubar", ImVec2(viewport.w, viewport.y), 
                ImGuiChildFlags_AlwaysUseWindowPadding,
                WND_DEFAULT_FLAGS | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                const ImU32 btncol = PRIMARY_COLOR.alpha(0.1).to_imcolor();
                const ImU32 btnhoveredcol = PRIMARY_COLOR.alpha(0.05).to_imcolor();
                
                for (int i = VIEW_SETTINGS; i < NUM_VIEWS; ++i)
                {
                    ImU32 mybtncol = (m_cur_view == i) ? btncol : 0;
                    ImU32 mybtnhoveredcol = (m_cur_view == i) ? btncol : btnhoveredcol;
                    ImU32 mytxtcol = (m_cur_view == i) ? PRIMARY_COLOR.to_imcolor() : IM_COL32(255, 255, 255,255);
                
                    ImGui::PushStyleColor(ImGuiCol_Button, mybtncol);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, mybtnhoveredcol);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, mybtncol);
                    ImGui::PushStyleColor(ImGuiCol_Text, mytxtcol);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
                    {
                        if (ImGui::Button(view_name(i))) {
                            new_view = (m_cur_view == i) ? VIEW_GAME : i;
                        }
                    }
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(4);
                
                    ImGui::SameLine();
                }
                
                int fps = int(std::lroundf(1.f / m_emu.delta_t()));
                draw_rtalign_text("FPS: %d", fps);
                ImGui::SameLine();
                
                // bottom border
                auto* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddLine(ImVec2(0, ImGui::GetWindowSize().y - 1),
                    ImVec2(viewport.w, ImGui::GetWindowSize().y), PRIMARY_COLOR.alpha(0.7).to_imcolor(), 2.0f);

                ImGui::EndChild();
            }
        }
        ImGui::PopStyleVar(5);
    }
    ImGui::PopFont();
    return gui_view(new_view);
}

void emu_gui::draw_view(gui_view view, const SDL_Rect& viewport, bool* p_wndclosed)
{
    auto& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, style.WindowPadding.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, WND_BGCOLOR_IM32);
    {
        if (ImGui::BeginChild(view_name(view), ImVec2(viewport.w, viewport.h),
            ImGuiChildFlags_AlwaysUseWindowPadding, WND_DEFAULT_FLAGS))
        {
            draw_closebutton(view_name(view), p_wndclosed, m_touchenabled);

            ImGui::Dummy(ImVec2(0, 15));

            switch (view)
            {
            case VIEW_SETTINGS: draw_settings_content(); break;
            case VIEW_ABOUT:    draw_about_content();    break;
            default: break;
            }
        }
        ImGui::EndChild();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void emu_gui::run(SDL_Point disp_size, const SDL_Rect& viewport)
{
    m_drawingframe = true;

    m_fonts[FONT_TXT] = get_font(FONT_TXT, disp_size);
    m_fonts[FONT_HDR] = get_font(FONT_HDR, disp_size);
    // assume screen is small if touch is enabled
    m_fonts[FONT_MENUBAR] = get_font(m_touchenabled ? FONT_TXT : FONT_MENUBAR, disp_size); 
     
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    float gui_wnd_height = viewport.y + (m_cur_view == VIEW_GAME ? 0 : viewport.h);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(viewport.w, gui_wnd_height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    {
        ImGui::Begin("GUI", nullptr, 
            WND_DEFAULT_FLAGS | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            m_cur_view = draw_menubar(viewport);

            if (m_cur_view != VIEW_GAME) {
                ImGui::PushFont(m_fonts[FONT_TXT]);
                {
                    bool view_closed = false;
                    draw_view(m_cur_view, viewport, &view_closed);
                    if (view_closed) {
                        m_cur_view = VIEW_GAME;
                    }
                }
                ImGui::PopFont();
            }
        }
        ImGui::End();
    }
    ImGui::PopStyleVar(3);

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);

#ifdef __EMSCRIPTEN__
    if (m_touchenabled) {
        handle_touchctrls_state();
    }
#endif

    m_lastkeypress = SDL_SCANCODE_UNKNOWN;
    m_drawingframe = false;
}


#ifndef IMGUI_DISABLE_DEMO_WINDOWS
int demo_window()
{
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr) {
        logERROR("Demo SDL_CreateWindow(): %s", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        logERROR("Demo SDL_CreateRenderer(): %s", SDL_GetError());
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsLight();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    
    bool running = true;
    bool show_demo_window = true;
    color clrcol(115, 140, 153, 255);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, clrcol.r, clrcol.g, clrcol.b, clrcol.a);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}
#endif

POP_WARNINGS

