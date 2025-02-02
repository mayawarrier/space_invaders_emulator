
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "emu.hpp"
#include "gui.hpp"

PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
// good for learning Imgui features
int demo_window();
#endif

static constexpr color PANEL_BGCOLOR(36, 36, 36, 255);
static constexpr color HEADER_BGCOLOR(48, 82, 121, 255);
static constexpr color SUBHDR_BGCOLOR = HEADER_BGCOLOR.brighter(50);

#define MIN_FONT_SIZE 5
#define MAX_FONT_SIZE 50

#if __EMSCRIPTEN__
// larger because reported display size is smaller
#define TXT_FONT_VH 1.85
#define SUBHDR_FONT_VH 2.41
#define HDR_FONT_VH 2.96
#else
#define TXT_FONT_VH 1.57
#define SUBHDR_FONT_VH 1.944
#define HDR_FONT_VH 2.5
#endif

// For emscripten there can only be one window
#ifdef __EMSCRIPTEN__
static emu_gui* GUI = nullptr;
#endif

int emu_gui::init_fontatlas()
{
    logMESSAGE("Building font atlas");

    ImGuiIO& io = ImGui::GetIO();

    for (int i = MIN_FONT_SIZE; i <= MAX_FONT_SIZE; ++i)
    {
        ImFontConfig cfg; cfg.SizePixels = i;
        ImFont* font = io.Fonts->AddFontDefault(&cfg);
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

static int get_font_vh_size(float vh, SDL_Point disp_size) {
    return int(std::lroundf(vh * disp_size.y / 100.f));
}

ImFont* emu_gui::get_font_vh(float vh, SDL_Point disp_size) const {
    return get_font_px(get_font_vh_size(vh, disp_size));
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
    m_fonts({ nullptr, nullptr, nullptr }),
    m_cur_view(VIEW_GAME),
    m_frame_lastkeypress(SDL_SCANCODE_UNKNOWN),
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
        m_frame_lastkeypress = e->key.keysym.scancode;
        m_anykeypress = true;
    }
    bool ret = ImGui_ImplSDL2_ProcessEvent(e); 

    auto& io = ImGui::GetIO();
    out_ci.capture_keyboard = io.WantCaptureKeyboard;
    out_ci.capture_mouse = io.WantCaptureMouse;

    return ret;
}

static void WND_PADX() {
    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
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

static void draw_header(const char* title, color colr, gui_align align = ALIGN_LEFT)
{
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpos = ImGui::GetWindowPos();
    ImVec2 wndpadding = ImGui::GetStyle().WindowPadding;
    float hdr_size = ImGui::GetFont()->FontSize + wndpadding.y * 2;

    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(wndpos.x, dpos.y),
        ImVec2(wndpos.x + wndsize.x, dpos.y + hdr_size), colr.to_imcolor());

    ImGui::SetCursorPos(ImVec2(wndpadding.x, ImGui::GetCursorPosY() + wndpadding.y));

    switch (align)
    {
    case ALIGN_RIGHT:
        setposX_right_align(title);
        break;
    case ALIGN_CENTER:
        setposX_center_align(title);
        break;
    default:
        break;
    }
    ImGui::TextUnformatted(title);
}

static void draw_subheader(const char* title, color colr, gui_align align = ALIGN_LEFT)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, ImGui::GetStyle().WindowPadding.y));
    draw_header(title, colr, align);
    ImGui::PopStyleVar();
}

static void draw_url(const char* text, const char* url)
{
    const ImU32 color = IM_COL32(62, 166, 255, 255);

    float posX = ImGui::GetCursorPosX();
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    ImVec2 txtsize = ImGui::CalcTextSize(text);

    if (ImGui::InvisibleButton(text, txtsize)) {
        if (SDL_OpenURL(url) != 0) {
            logERROR("Could not open URL %s", url);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
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

void emu_gui::draw_inputkey(const char* label, input inptype, gui_align align, float labelsizeX)
{
    SDL_assert(align == ALIGN_LEFT || align == ALIGN_RIGHT);

    bool& focused = m_inputkey_focused[inptype];
    SDL_Scancode& key = m_emu.input2keymap()[inptype];

    ImGuiStyle style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpadding = style.WindowPadding;

    float inputwidth = wndsize.x / 3;
    float textsizeX = labelsizeX < 0 ? ImGui::CalcTextSize(label).x : labelsizeX;
    float inputposX = align == ALIGN_LEFT ? 
        textsizeX + 2 * wndpadding.x : wndsize.x - wndpadding.x - inputwidth;
    
    ImVec2 inputpos(inputposX, ImGui::GetCursorPosY() - style.FramePadding.y);
    ImVec2 inputsize(inputwidth, ImGui::GetFontSize() + style.FramePadding.y * 2);
    ImVec2 txtpos(inputposX - textsizeX - wndpadding.x, ImGui::GetCursorPosY());
    
    ImGui::SetCursorPos(inputpos);
    ImVec2 dpos = ImGui::GetCursorScreenPos();

    // invisible selectable to determine focus
    ImGui::PushID(label);
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

    if (focused && m_frame_lastkeypress != SDL_SCANCODE_UNKNOWN) {
        key = m_frame_lastkeypress;
        m_frame_lastkeypress = SDL_SCANCODE_UNKNOWN; // consume
    } 

    // update for next frame
    if (next_frame_unfocused) {
        focused = false;
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }

    ImGui::SameLine();
    ImGui::SetCursorPos(txtpos);
    ImGui::TextUnformatted(label);
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

void emu_gui::draw_help_content()
{

}

void emu_gui::draw_settings_content()
{
    // DIP Switches section
    {
        ImGui::PushFont(m_fonts.subhdr_font);
        draw_subheader("DIP Switches", SUBHDR_BGCOLOR);
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

        // Draw URL message
        //{
        //    WND_PADX();
        //    ImGui::TextUnformatted("See");
        //    ImGui::SameLine();
        //    draw_url("README", "https://github.com/mayawarrier/space_invaders_emulator/blob/main/README.md");
        //    ImGui::SameLine();
        //    ImGui::TextUnformatted("to learn how this works.");
        //}
        
        ImGui::NewLine();

        int num_ships = 0;
        set_bit(&num_ships, 0, m_emu.get_switch(3));
        set_bit(&num_ships, 1, m_emu.get_switch(5));
        num_ships += 3;

        WND_PADX(); ImGui::Text("Number of ships: %d", num_ships);
        WND_PADX(); ImGui::Text("Extra ship at: %d points", m_emu.get_switch(6) ? 1000 : 1500);
        WND_PADX(); ImGui::Text("Diagnostics at startup: %s", m_emu.get_switch(4) ? "Enabled" : "Disabled");
        WND_PADX(); ImGui::Text("Coins in demo screen: %s", m_emu.get_switch(7) ? "No" : "Yes");

        ImGui::NewLine();
    }
    
    // Controls section
    if (!m_touchenabled || m_anykeypress)
    {
        ImGui::PushFont(m_fonts.subhdr_font);
        draw_subheader("Controls", SUBHDR_BGCOLOR);
        ImGui::PopFont();

        ImGui::NewLine();

        constexpr std::pair<const char*, input> inputs[NUM_INPUTS]
        {
            { "Player 1 Left ", INPUT_P1_LEFT },
            { "Player 1 Right", INPUT_P1_RIGHT },
            { "Player 1 Fire ", INPUT_P1_FIRE },
            { "Player 2 Left ", INPUT_P2_LEFT },
            { "Player 2 Right", INPUT_P2_RIGHT },
            { "Player 2 Fire ", INPUT_P2_FIRE },
            { "1P Start", INPUT_1P_START },
            { "2P Start", INPUT_2P_START },
            { "Insert coin", INPUT_CREDIT }
        };

        float labelsizeX = std::numeric_limits<float>::min();
        for (auto& inp : inputs) {
            labelsizeX = std::max(labelsizeX, ImGui::CalcTextSize(inp.first).x);
        }

        for (int i = 1; i <= NUM_INPUTS; ++i)
        {
            auto& inp = inputs[i - 1];
            draw_inputkey(inp.first, inp.second, ALIGN_LEFT, labelsizeX);
            if ((i % 3) == 0) {
                ImGui::NewLine();
            }
        }
    }

    // Audio section
    {
        ImGui::PushFont(m_fonts.subhdr_font);
        draw_subheader("Audio", SUBHDR_BGCOLOR);
        ImGui::PopFont();
        
        ImGui::NewLine();
        int new_volume = draw_volume_slider("Volume", m_emu.get_volume(), ALIGN_CENTER);
        m_emu.set_volume(new_volume);

        ImGui::NewLine();
    }
}

void emu_gui::draw_panel(const char* title, const SDL_Rect& viewport, void(emu_gui::*draw_content)())
{
    static constexpr ImGuiWindowFlags PANEL_FLAGS = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNavInputs;

    const ImVec2 wndpos(viewport.x, viewport.y);
    const ImVec2 wndsize(viewport.w, viewport.h);

    ImGui::SetNextWindowPos(wndpos);
    ImGui::SetNextWindowSize(wndsize);

    if (ImGui::Begin(title, NULL, PANEL_FLAGS))
    {
        auto& style = ImGui::GetStyle();

        // background
        ImGui::GetWindowDrawList()->AddRectFilled(wndpos,
            ImVec2(wndpos.x + wndsize.x, wndpos.y + wndsize.y),
            PANEL_BGCOLOR.to_imcolor());

        // header
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.WindowPadding.x, 12));
        ImGui::PushFont(m_fonts.hdr_font);
        ImGui::SetCursorPosY(0); // remove spacing
        draw_header(title, HEADER_BGCOLOR, ALIGN_CENTER);
        ImGui::PopFont();
        ImGui::PopStyleVar();

        ImGui::NewLine();

        // content
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25, style.WindowPadding.y));
        (this->*draw_content)();
        ImGui::PopStyleVar();

        ImGui::End();
    } 
}

gui_sizeinfo emu_gui::sizeinfo(SDL_Point disp_size) const
{
    SDL_assert(!m_drawingframe);

    int menu_height = get_font_vh_size(TXT_FONT_VH, disp_size) + // text
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
        if (m_ctrls_showing != CTRLS_PLAYER_SELECT) 
        {
            EM_ASM(Module.showPlayerSelectControls());
            m_ctrls_showing = CTRLS_PLAYER_SELECT;
            m_playersel = PLAYER_SELECT_NONE;
            m_playerselinp_idx = 0;
        }
    } 
    else if (m_ctrls_showing != CTRLS_GAME) {
        EM_ASM(Module.showGameControls());
        m_ctrls_showing = CTRLS_GAME;
    }

    if (m_playersel != PLAYER_SELECT_NONE)
    {
        auto& inputs = player_select_inputs[m_playersel - 1];
        if (m_playerselinp_idx < inputs.size())
        {
            const auto& frame_inputs = inputs[m_playerselinp_idx];
            for (const auto& inp : frame_inputs) {
                m_emu.send_input(inp.first, inp.second);
            }
            m_playerselinp_idx++;
        }
    }
}
#endif 

void emu_gui::run(SDL_Point disp_size, const SDL_Rect& viewport)
{
    m_drawingframe = true;

    m_fonts.txt_font = get_font_vh(TXT_FONT_VH, disp_size);
    m_fonts.subhdr_font = get_font_vh(SUBHDR_FONT_VH, disp_size);
    m_fonts.hdr_font = get_font_vh(HDR_FONT_VH, disp_size);
     
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::PushFont(m_fonts.txt_font);

    int new_view = m_cur_view;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Settings")) {
            new_view = 
                (m_cur_view == VIEW_SETTINGS) ?
                VIEW_GAME : VIEW_SETTINGS;
        }
        ImGui::SameLine();

        if (ImGui::Button("About")) {
            new_view = 
                (m_cur_view == VIEW_ABOUT) ?
                VIEW_GAME : VIEW_ABOUT;
        }
        ImGui::SameLine();

        int fps = int(std::lroundf(1.f / m_emu.delta_t()));
        draw_rtalign_text("FPS: %d", fps);

        ImGui::EndMainMenuBar();
    }
    m_cur_view = new_view;

    switch (m_cur_view)
    {
    case VIEW_SETTINGS: draw_panel("Settings", viewport, &emu_gui::draw_settings_content); break;
    case VIEW_ABOUT:    draw_panel("About",    viewport, &emu_gui::draw_help_content); break;
    default: break;
    }

    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);

#ifdef __EMSCRIPTEN__
    if (m_touchenabled) {
        handle_touchctrls_state();
    }
#endif

    m_frame_lastkeypress = SDL_SCANCODE_UNKNOWN;
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

