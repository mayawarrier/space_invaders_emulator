
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

static constexpr ImU32 WND_BGCOLOR = IM_COL32(0, 0, 0, 255);
static constexpr ImU32 HDR_FONTCOLOR = IM_COL32(0x1E, 0xFE, 0x1E, 0xFF);
static constexpr ImU32 BORDER_COLOR = IM_COL32(0x1E, 0xFE, 0x1E, 0x33);

static constexpr ImGuiWindowFlags WND_DEFAULT_FLAGS = 
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoNavInputs;

static constexpr int MIN_FONT_SIZE = 5;
static constexpr int MAX_FONT_SIZE = 50;

// For emscripten there can only be one window
#ifdef __EMSCRIPTEN__
static emu_gui* GUI = nullptr;
#endif

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
// good for learning Imgui features
int demo_window();
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

int emu_gui::init_fontatlas()
{
    logMESSAGE("Building font atlas");

    ImGuiIO& io = ImGui::GetIO();

    for (int i = MIN_FONT_SIZE; i <= MAX_FONT_SIZE; ++i)
    {
        ImFontConfig cfg; cfg.SizePixels = i;
        ImFont* font = io.Fonts->AddFontFromFileTTF("data/SourceCodePro-Semibold.ttf", i, &cfg);
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
        vh = is_emscripten() ?
            disp_size.y < disp_size.x ? 1.85f : 1.95f :
            1.70f;//1.57f;
        break;
    case FONT_HDR: vh = is_emscripten() ? 2.41f : 2.1; break;//1.944f; break;

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
    SDL_Window* window, 
    SDL_Renderer* renderer, 
    emu_interface emu
) :
    m_emu(emu),
    m_renderer(renderer),
    m_fonts({ nullptr, nullptr }),
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
    GUI = this;
#endif

    logMESSAGE("Initializing GUI");

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
    if (init_fontatlas() != 0) {
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

static void draw_header(const char* title, gui_align align = ALIGN_LEFT)
{
    bool* p_closed = nullptr;
    ImGui::PushStyleColor(ImGuiCol_Text, HDR_FONTCOLOR);
    {
        ImVec2 dpos = ImGui::GetCursorScreenPos();
        ImVec2 wndsize = ImGui::GetWindowSize();
        ImVec2 wndpos = ImGui::GetWindowPos();
        ImVec2 wndpadding = ImGui::GetStyle().WindowPadding;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        float hdr_size = ImGui::GetFont()->FontSize + wndpadding.y;

        if (p_closed)
        {
            constexpr float cross_size = 12.f;
            constexpr float btnmargin = 4.f, btnpadding = 3.f;
            constexpr float btnsize = cross_size + 2 * btnpadding;

            float btn_xoffset = wndsize.x - (cross_size + get_scrollbar_width() + wndpadding.x + 2 * btnpadding + btnmargin);
            ImVec2 btnpos = ImVec2(dpos.x + btn_xoffset, dpos.y + btnmargin);
            ImVec2 crosspos = ImVec2(btnpos.x + btnpadding, btnpos.y + btnpadding);

            ImGui::PushID(title);
            ImGui::SetCursorScreenPos(btnpos);
            bool clicked = ImGui::Button("##wndclose", ImVec2(btnsize, btnsize));
            ImGui::PopID();

            static constexpr color btn_basecol(200, 200, 200, 255);
            ImU32 btncol = (ImGui::IsItemHovered() ? btn_basecol.brighter(50) : btn_basecol).to_imcolor();

            ImVec2 p2 = ImVec2(crosspos.x + cross_size, crosspos.y + cross_size);
            ImVec2 p3 = ImVec2(crosspos.x, crosspos.y + cross_size);
            ImVec2 p4 = ImVec2(crosspos.x + cross_size, crosspos.y);
            draw_list->AddLine(crosspos, p2, btncol, 2.0f);
            draw_list->AddLine(p3, p4, btncol, 2.0f);

            *p_closed = clicked;
            ImGui::SetCursorScreenPos(dpos);
        }

        switch (align)
        {
        case ALIGN_RIGHT:  setposX_right_align(title); break;
        case ALIGN_CENTER: setposX_center_align(title); break;
        default: break;
        }

        ImGui::TextUnformatted(title);
    }
    ImGui::PopStyleColor();
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
    const ImU32 color = IM_COL32(62, 166, 255, 255);

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
        ImGui::SetTooltip(format_escape(url).c_str());
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

static bool draw_dip_switch(int index, bool value, float draw_width = -1)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    float width = draw_width < 0 ? ImGui::GetFrameHeight() * 2 : draw_width;
    float height = width * 1.55f;

    ImGui::PushID(index); // unique button id
    if (ImGui::InvisibleButton("DIP", ImVec2(width, height))) {
        value = !value;
    }
    ImGui::PopID();

    static constexpr color on_bgcolor(145, 211, 68, 255);
    static constexpr color off_bgcolor(150, 150, 150, 255);

    color bgcolor;
    if (value) {
        bgcolor = ImGui::IsItemHovered() ? 
            on_bgcolor.brighter(20) : on_bgcolor;
    } else {
        bgcolor = ImGui::IsItemHovered() ? 
            off_bgcolor.darker(20) : off_bgcolor;
    }

    ImVec2 slide_dpos_min = value ? dpos : ImVec2(dpos.x, dpos.y + height / 2);
    ImVec2 slide_dpos_max = slide_dpos_min + ImVec2(width, height / 2);

    draw_list->AddRectFilled(dpos, ImVec2(dpos.x + width, dpos.y + height), bgcolor.to_imcolor());
    draw_list->AddRectFilled(slide_dpos_min, slide_dpos_max, IM_COL32(255, 255, 255, 255));

    return value;
}

struct inputkey_stateptrs
{
    bool* focused;
    SDL_Scancode* key;
};

static void draw_inputkey(int id, inputkey_stateptrs state, 
    SDL_Scancode keyvalue, gui_align align = ALIGN_LEFT)
{
    SDL_assert(align == ALIGN_LEFT || align == ALIGN_RIGHT);

    bool& focused = *state.focused;
    SDL_Scancode& key = *state.key;

    ImGuiStyle style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpadding = style.WindowPadding;

    float inputwidth = wndsize.x / 3;
    float inputposX = align == ALIGN_LEFT ?
        wndpadding.x : wndsize.x - wndpadding.x - inputwidth;

    ImVec2 inputpos(inputposX, ImGui::GetCursorPosY() - style.FramePadding.y);
    ImVec2 inputsize(inputwidth, ImGui::GetFontSize() + style.FramePadding.y * 2);

    ImGui::SetCursorPos(inputpos);
    ImVec2 dpos = ImGui::GetCursorScreenPos();

    // invisible selectable to determine focus
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
    ImGui::Selectable("", &focused, 0, inputsize);
    ImGui::PopStyleColor(3);
    ImGui::PopID();

    if (focused) {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }
    bool next_frame_unfocused = focused &&
        !ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImU32 bgcolor = focused ?
        ImGui::GetColorU32(ImGuiCol_FrameBgActive) :
        ImGui::GetColorU32(ImGuiCol_FrameBg);

    // Render a textbox-like widget
    draw_list->AddRectFilled(dpos, dpos + inputsize, bgcolor, style.FrameRounding);
    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), dpos + style.FramePadding,
        ImGui::GetColorU32(ImGuiCol_Text), SDL_GetScancodeName(key));

    if (focused && keyvalue != SDL_SCANCODE_UNKNOWN) {
        key = keyvalue;
    }

    // update for next frame
    if (next_frame_unfocused) {
        focused = false;
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }
}

void emu_gui::draw_ctrlpanel(const char* id, const char* title, 
    const std::pair<const char*, input> inputs[], int num_inputs, float panelsizeX)
{
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_COLOR);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 0));
    {
        static constexpr float top_padding = 10;
        static constexpr float bot_padding = 8;
        static constexpr float title_botpadding = 10;

        int num_lines = title ? num_inputs + 1 : num_inputs;

        // best attempt at calculating the panel size. 
        // this is slightly off so add a bit extra
        float panelsizeY =
            num_lines * (
                ImGui::GetFontSize() +
                ImGui::GetStyle().ItemSpacing.y +
                ImGui::GetStyle().FramePadding.y
                ) + 
            (2 * ImGui::GetStyle().WindowPadding.y) +
            top_padding + bot_padding + (title ? title_botpadding : 0) + 5;

        static constexpr ImGuiWindowFlags wnd_flags =
            WND_DEFAULT_FLAGS |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;
            
        ImGui::BeginChild(id, ImVec2(panelsizeX, panelsizeY), ImGuiChildFlags_Border, wnd_flags);
        {
            ImGui::Dummy(ImVec2(0, top_padding));

            if (title) {
                draw_header(title, ALIGN_LEFT);
                ImGui::Dummy(ImVec2(0, title_botpadding));
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

            ImGui::Dummy(ImVec2(0, bot_padding));
        }
        ImGui::EndChild();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

static int draw_volume_slider(const char* label, int volume, gui_align align = ALIGN_LEFT)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7, 7));

    float wndpaddingX = ImGui::GetStyle().WindowPadding.x;
    float textsizeX = ImGui::CalcTextSize(label).x;

    float sliderposX;
    switch (align)
    {
    case ALIGN_LEFT:
        sliderposX = textsizeX + 2 * wndpaddingX;
        break;
    case ALIGN_RIGHT: 
        sliderposX = ImGui::GetWindowSize().x - wndpaddingX - ImGui::CalcItemWidth();
        break;
    case ALIGN_CENTER:
        sliderposX = (ImGui::GetWindowSize().x - ImGui::CalcItemWidth() + textsizeX + wndpaddingX) / 2;
        break;
    default:
        SDL_assert(false);
        return volume;
    }
    ImVec2 txtpos(sliderposX - textsizeX - wndpaddingX, ImGui::GetCursorPosY());

    ImGui::SetCursorPosX(sliderposX);
    ImGui::SliderInt("##volume", &volume, 0, 100);
    ImGui::PopStyleVar();

    ImGui::SameLine();

    ImGui::SetCursorPos(txtpos);
    ImGui::TextUnformatted(label);
    
    return volume;
}

void emu_gui::draw_about_content()
{
    ImGui::PushFont(m_fonts.hdr_font);
    draw_header("About");
    ImGui::PopFont();
    ImGui::NewLine();

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

        ImGui::PushFont(m_fonts.hdr_font);
        draw_header("How it works");
        ImGui::PopFont();
        ImGui::NewLine();

        const char* content =
            "This emulator runs the original Space Invaders game from 1978!\n\n"
            "It simulates the CPU, hardware, and I/O devices the game requires, "
            "making the game think it's running on an actual arcade machine.\n\n"
            "This involves a complete simulation of the Intel 8080 CPU, as well as "
            "several other chips on the Space Invaders motherboard.\n\n";

        
        ImGui::TextUnformatted(content);

        static constexpr std::pair<const char*, const char*> links[] = {
            { "Computer Archeology website", "https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html" },
            { "RadioShack Intel 8080 Manual", "https://archive.org/details/8080-8085_Assembly_Language_Programming_1977_Intel" },
            { "Intel 8080 Datasheet", "https://deramp.com/downloads/intel/8080%20Data%20Sheet.pdf" }
        };

        ImGui::TextUnformatted("Read about the Space Invaders hardware:");
        for (auto& link : links) {
            
            draw_url(link.first, link.second);
        }

        ImGui::NewLine();
    }
    ImGui::PopTextWrapPos();
}

void emu_gui::draw_settings_content()
{
    auto& style = ImGui::GetStyle();

    // DIP Switches section
    {
        ImGui::PushFont(m_fonts.hdr_font);
        draw_header("DIP Switches");
        ImGui::PopFont();

        ImGui::NewLine();

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
        ImGui::PushFont(m_fonts.hdr_font);
        draw_header("Controls");
        ImGui::PopFont();

        ImGui::NewLine();

        static constexpr std::pair<const char*, input> inputs[9]
        {
            { "Left ", INPUT_P1_LEFT },
            { "Right", INPUT_P1_RIGHT },
            { "Fire ", INPUT_P1_FIRE },
            { "Left ", INPUT_P2_LEFT },
            { "Right", INPUT_P2_RIGHT },
            { "Fire ", INPUT_P2_FIRE },
            { "1P Start", INPUT_1P_START },
            { "2P Start", INPUT_2P_START },
            { "Insert coin", INPUT_CREDIT }
        };
        
        float panelmargin = 8;
        float panelsizeX = (ImGui::GetWindowSize().x - panelmargin) / 2 - style.WindowPadding.x;

        draw_ctrlpanel("inputkeys1", "Player 1", inputs, 3, panelsizeX); ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.ItemSpacing.x + panelmargin);

        draw_ctrlpanel("inputkeys2", "Player 2", inputs + 3, 3, panelsizeX);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.ItemSpacing.y + panelmargin);

        draw_ctrlpanel("inputkeys3", nullptr, inputs + 6, 3, ImGui::GetWindowSize().x - 2 * style.WindowPadding.x);
    
        ImGui::NewLine();
    }

    // Audio section
    {
        ImGui::PushFont(m_fonts.hdr_font);
        draw_header("Audio");
        ImGui::PopFont();
        
        ImGui::NewLine();
        int new_volume = draw_volume_slider("Volume", m_emu.get_volume(), ALIGN_CENTER);
        m_emu.set_volume(new_volume);

        ImGui::NewLine();
    }
}

void emu_gui::draw_view(const char* title, 
    const SDL_Rect& viewport, void(emu_gui::*draw_content)(), bool* p_wndclosed)
{
    auto& style = ImGui::GetStyle();

    ImGui::SetNextWindowPos(ImVec2(viewport.x, viewport.y));
    ImGui::SetNextWindowSize(ImVec2(viewport.w, viewport.h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, style.WindowPadding.y));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, WND_BGCOLOR);
    {
        if (ImGui::Begin(title, NULL, WND_DEFAULT_FLAGS))
        {
            (this->*draw_content)();
            ImGui::End();
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

gui_sizeinfo emu_gui::get_sizeinfo(SDL_Point disp_size) const
{
    SDL_assert(!m_drawingframe);

    int menu_height = get_font_px_size(FONT_TXT, disp_size) + // text
        int(ImGui::GetStyle().FramePadding.y * 2.0f);  // padding

    float resv_outwnd_ypct = is_emscripten() && m_touchenabled ? 0.25f : 0.1f;
    int resv_outwndY = int(std::lroundf(resv_outwnd_ypct * disp_size.y));

    return {
        .vp_offset = {.x = 0, .y = menu_height },
        .resv_inwnd_size = {.x = 0, .y = menu_height },
        .resv_outwnd_size = {.x = 0, .y = resv_outwndY }
    };
}

#ifdef __EMSCRIPTEN__

extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_fire(bool pressed) 
{
    GUI->m_emu.send_input(INPUT_P1_FIRE, pressed);
    GUI->m_emu.send_input(INPUT_P2_FIRE, pressed);
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_left(bool pressed) 
{
    GUI->m_emu.send_input(INPUT_P1_LEFT, pressed);
    GUI->m_emu.send_input(INPUT_P2_LEFT, pressed);
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_touch_right(bool pressed) 
{
    GUI->m_emu.send_input(INPUT_P1_RIGHT, pressed);
    GUI->m_emu.send_input(INPUT_P2_RIGHT, pressed);
}

extern "C" EMSCRIPTEN_KEEPALIVE void web_click_1p(void)
{
    if (GUI->m_playersel == PLAYER_SELECT_NONE) {
        GUI->m_playersel = PLAYER_SELECT_1P;
    }
}
extern "C" EMSCRIPTEN_KEEPALIVE void web_click_2p(void) 
{
    if (GUI->m_playersel == PLAYER_SELECT_NONE) {
        GUI->m_playersel = PLAYER_SELECT_2P;
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

void emu_gui::run(SDL_Point disp_size, const SDL_Rect& viewport)
{
    m_drawingframe = true;

    m_fonts.txt_font = get_font(FONT_TXT, disp_size);
    m_fonts.hdr_font = get_font(FONT_HDR, disp_size);
     
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (m_touchenabled) { // disable hovering
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_Button]);
    }
    ImGui::PushFont(m_fonts.txt_font);
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::Button("Settings")) {
                m_cur_view = (m_cur_view == VIEW_SETTINGS) ? VIEW_GAME : VIEW_SETTINGS;
            }
            ImGui::SameLine();

            if (ImGui::Button("About")) {
                m_cur_view = (m_cur_view == VIEW_ABOUT) ? VIEW_GAME : VIEW_ABOUT;
            }
            ImGui::SameLine();

            int fps = int(std::lroundf(1.f / m_emu.delta_t()));
            draw_rtalign_text("FPS: %d", fps);

            ImGui::EndMainMenuBar();
        }

        bool wndclosed = false;
        switch (m_cur_view)
        {
        case VIEW_SETTINGS: draw_view("Settings", viewport, &emu_gui::draw_settings_content, &wndclosed); break;
        case VIEW_ABOUT:    draw_view("About",    viewport, &emu_gui::draw_about_content, &wndclosed); break;
        default: break;
        }

        if (wndclosed) {
            m_cur_view = VIEW_GAME;
        }
    }
    ImGui::PopFont();
    if (m_touchenabled) {
        ImGui::PopStyleColor();
    }

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
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), 
            (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
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

