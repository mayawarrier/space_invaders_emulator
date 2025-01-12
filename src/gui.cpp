
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

#define PADX() ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x)

emu_gui::emu_gui(emu_interface emu) :
    m_emu(emu),
    m_hdr_font(nullptr),
    m_subhdr_font(nullptr),
    m_menu_height(0),
    m_cur_panel(PANEL_NONE),
    m_fps(-1),
    m_lastkeypress(SDL_SCANCODE_UNKNOWN),
    m_ok(false)
{
    //demo_window();

    std::fill_n(m_inputkey_focused, INPUT_NUM_INPUTS, false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    SDL_assert(emu.window() && emu.renderer());

    if (!ImGui_ImplSDL2_InitForSDLRenderer(emu.window(), emu.renderer()) ||
        !ImGui_ImplSDLRenderer2_Init(emu.renderer())) {
        logERROR("Failed to initialize ImGui with SDL backend");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig text_fontcfg, subhdr_fontcfg, hdr_fontcfg;
    text_fontcfg.SizePixels = 15.f;
    subhdr_fontcfg.SizePixels = 19.f;
    hdr_fontcfg.SizePixels = 25.f;

    io.Fonts->AddFontDefault(&text_fontcfg);
    m_subhdr_font = io.Fonts->AddFontDefault(&subhdr_fontcfg);
    m_hdr_font = io.Fonts->AddFontDefault(&hdr_fontcfg);
    io.Fonts->Build();

    io.IniFilename = nullptr;

    // nasty workaround to determine menubar height
    // https://github.com/ocornut/imgui/issues/252
    {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Dummy", NULL, ImGuiWindowFlags_NoTitleBar);
        ImGui::BeginMainMenuBar();
        ImGui::EndMainMenuBar();
        ImGui::End();

        m_menu_height = int(ImGui::GetFrameHeight());
        ImGui::EndFrame();
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

bool emu_gui::process_event(const SDL_Event* e) 
{
    if (e->type == SDL_KEYUP) {
        m_lastkeypress = e->key.keysym.scancode;
    }
    return ImGui_ImplSDL2_ProcessEvent(e); 
}

bool emu_gui::want_keyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard; 
}

bool emu_gui::want_mouse() const { 
    return ImGui::GetIO().WantCaptureMouse; 
}

static void set_window_width(SDL_Window* window, int new_width)
{
    int width, height, xpos, ypos;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowPosition(window, &xpos, &ypos);

    SDL_SetWindowSize(window, new_width, height);
    SDL_SetWindowPosition(window, xpos - (new_width - width) / 2, ypos);
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

static void draw_header(const char* title, color colr, gui_align_t align = ALIGN_LEFT)
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

static void draw_subheader(const char* title, color colr, gui_align_t align = ALIGN_LEFT)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, ImGui::GetStyle().WindowPadding.y));
    draw_header(title, colr, align);
    ImGui::PopStyleVar();
}

static void draw_url(const char* text, const char* url)
{
    const ImU32 color = IM_COL32(0, 0, 238, 255);

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

void emu_gui::draw_inputkey(const char* label, inputtype inptype, gui_align_t align, float labelsizeX)
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

    if (focused && m_lastkeypress != SDL_SCANCODE_UNKNOWN) {
        key = m_lastkeypress;
        m_lastkeypress = SDL_SCANCODE_UNKNOWN; // consume
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

static int draw_volume_slider(const char* label, int volume, gui_align_t align = ALIGN_LEFT)
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
        ImGui::PushFont(m_subhdr_font);
        draw_subheader("DIP Switches", SUBHDR_BGCOLOR);
        ImGui::PopFont();

        ImGui::NewLine();

        constexpr float sw_spacingX = 20;

        float sw_width = 2 * ImGui::GetFrameHeight();
        float all_sw_width = 5 * sw_width + 4 * sw_spacingX;
        float sw_startposX = (ImGui::GetWindowSize().x - all_sw_width) / 2;
        ImGui::SetCursorPosX(sw_startposX);

        float sw_txtpos[5];

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(sw_spacingX, 0));
        for (int i = 7; i >= 3; --i)
        {
            sw_txtpos[i - 3] = ImGui::GetCursorPosX();

            bool cur_val = m_emu.get_switch(i);
            bool new_val = draw_dip_switch(i, cur_val, sw_width);
            m_emu.set_switch(i, new_val);

            if (i != 3) {
                ImGui::SameLine();
            }
        }
        ImGui::PopStyleVar();

        ImGui::PushFont(m_subhdr_font);
        for (int i = 7; i >= 3; --i)
        {
            char sw_name[] = { 'D', 'I', 'P', char('0' + i), '\0' };
            ImGui::SetCursorPosX(sw_txtpos[i - 3]);
            ImGui::TextUnformatted(sw_name);
            ImGui::SameLine();
        }
        ImGui::PopFont();

        ImGui::NewLine();
        ImGui::NewLine();

        // Draw URL
        {
            //setposX_right_align("See README to learn how this works.");

            PADX();
            ImGui::TextUnformatted("See");
            ImGui::SameLine();
            draw_url("README", "https://github.com/mayawarrier/space_invaders_emulator/blob/main/README.md");
            ImGui::SameLine();
            ImGui::TextUnformatted("to learn how this works.");
        }
        
        ImGui::NewLine();

        int num_ships = 0;
        set_bit(&num_ships, 0, m_emu.get_switch(3));
        set_bit(&num_ships, 1, m_emu.get_switch(5));
        num_ships += 3;

        PADX(); ImGui::Text("Number of ships: %d", num_ships);
        PADX(); ImGui::Text("Extra ship at: %d points", m_emu.get_switch(6) ? 1000 : 1500);
        PADX(); ImGui::Text("Diagnostics at startup: %s", m_emu.get_switch(4) ? "Enabled" : "Disabled");
        PADX(); ImGui::Text("Coins in demo screen: %s", m_emu.get_switch(7) ? "No" : "Yes");

        ImGui::NewLine();
    }

    // Controls section
    {
        ImGui::PushFont(m_subhdr_font);
        draw_subheader("Controls", SUBHDR_BGCOLOR);
        ImGui::PopFont();

        ImGui::NewLine();

        constexpr std::pair<const char*, inputtype> inputs[INPUT_NUM_INPUTS]
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

        for (int i = 1; i <= INPUT_NUM_INPUTS; ++i)
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
        ImGui::PushFont(m_subhdr_font);
        draw_subheader("Audio", SUBHDR_BGCOLOR);
        ImGui::PopFont();

        ImGui::NewLine();
        int new_volume = draw_volume_slider("Volume", m_emu.get_volume(), ALIGN_CENTER);
        m_emu.set_volume(new_volume);
    }
}

void emu_gui::draw_panel(const char* title, void(emu_gui::*draw_content)())
{
    static constexpr ImGuiWindowFlags PANEL_FLAGS = 
        ImGuiWindowFlags_NoTitleBar |
        //ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNavInputs;

    const ImVec2 wndpos(m_emu.viewport().x, m_emu.viewport().y);
    const ImVec2 wndsize(m_emu.viewport().w, m_emu.viewport().h);

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
        ImGui::PushFont(m_hdr_font);
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

void emu_gui::draw_frame()
{
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int& cur_panel = m_cur_panel;
    int new_panel = cur_panel;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Settings")) {
            new_panel = 
                (cur_panel == PANEL_SETTINGS) ?
                PANEL_NONE : PANEL_SETTINGS;
        }
        ImGui::SameLine();

        if (ImGui::Button("Help!")) {
            new_panel = 
                (cur_panel == PANEL_HELP) ?
                PANEL_NONE : PANEL_HELP;
        }
        ImGui::SameLine();

        draw_rtalign_text("FPS: %d", m_fps);

        ImGui::EndMainMenuBar();
    }
    cur_panel = new_panel;

    switch (cur_panel)
    {
    case PANEL_SETTINGS: draw_panel("Settings", &emu_gui::draw_settings_content); break;
    case PANEL_HELP:     draw_panel("Help!",    &emu_gui::draw_help_content); break;
    default: break;
    }

    m_lastkeypress = SDL_SCANCODE_UNKNOWN;

    // todo: recompute font sizes for next frame, based on window size?

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_emu.renderer());
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
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
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