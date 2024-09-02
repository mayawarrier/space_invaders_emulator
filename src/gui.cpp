
#include "emu.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

PUSH_WARNINGS
IGNORE_WFORMAT_SECURITY

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
// good for learning Imgui features
int demo_window();
#endif

struct color
{
    uint8_t r, g, b, a;

    color() = default;

    constexpr color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) :
        r(r), g(g), b(b), a(a)
    {}
    // todo: saturate
    constexpr color brighter(uint8_t amt) const { 
        return color(r + amt, g + amt, b + amt, 255);
    }
    constexpr color darker(uint8_t amt) const {
        return color(r - amt, g - amt, b - amt, 255);
    }
    constexpr ImU32 to_imcolor() const {
        return IM_COL32(r, g, b, a);
    }
};

enum panel_type
{
    PANEL_NONE,
    PANEL_HELP,
    PANEL_SETTINGS
};

int emu_gui::init(emu* emu, SDL_Rect* screen_rect)
{
    // demo_window();

    m_emu = emu;
    m_cur_panel = PANEL_NONE;
    m_fps = -1;
    m_deltat_min = FLT_MAX;
    m_deltat_max = FLT_MIN;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForSDLRenderer(emu->m_window, emu->m_renderer) ||
        !ImGui_ImplSDLRenderer2_Init(emu->m_renderer)) {
        return ERROR("Failed to initialize ImGui with SDL backend");
    }

    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig dft_fontcfg, txt_fontcfg, hdr_fontcfg;
    dft_fontcfg.SizePixels = 15.f;
    txt_fontcfg.SizePixels = 18.f;
    hdr_fontcfg.SizePixels = 25.f;

    io.Fonts->AddFontDefault(&dft_fontcfg);
    m_txt_font = io.Fonts->AddFontDefault(&txt_fontcfg);
    m_hdr_font = io.Fonts->AddFontDefault(&hdr_fontcfg);
    io.Fonts->Build();

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

        m_menubar_height = int(ImGui::GetFrameHeight());
        ImGui::EndFrame();

        *screen_rect = {
            .x = 0,
            .y = m_menubar_height,
            .w = int(emu->m_scresX),
            .h = int(emu->m_scresY)
        };
    }
    return 0;
}

void emu_gui::shutdown()
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void emu_gui::print_dbginfo()
{
    MESSAGE("ImGui version: %s", ImGui::GetVersion());
}

bool emu_gui::process_event(SDL_Event* e) {
    return ImGui_ImplSDL2_ProcessEvent(e); 
}
bool emu_gui::want_keyboard()  {
    return ImGui::GetIO().WantCaptureKeyboard; 
}
bool emu_gui::want_mouse() { 
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

template <typename ...Args>
static void draw_ralign_text(const char* fmt, Args... args)
{
    const char* ptxt;
    char txtbuf[512]; // more than enough for anybody :)

    if (sizeof...(args) == 0) {
        ptxt = fmt;
    } else {
        std::snprintf(txtbuf, 512, fmt, args...);
        ptxt = txtbuf;
    }

    // get start pos for right alignment
    ImVec2 txt_size = ImGui::CalcTextSize(ptxt);
    float wnd_width = ImGui::GetWindowSize().x;
    float txt_pos = wnd_width - txt_size.x - ImGui::GetStyle().WindowPadding.x;

    ImGui::SetCursorPosX(txt_pos);
    ImGui::Text(ptxt);
}

static void draw_header(const char* title, color colr, bool right_align = false)
{
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    ImVec2 wndsize = ImGui::GetWindowSize();
    ImVec2 wndpos = ImGui::GetWindowPos();
    float wndpadding = ImGui::GetStyle().WindowPadding.y;
    float hdr_size = ImGui::GetFont()->FontSize + wndpadding * 2;

    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(wndpos.x, dpos.y),
        ImVec2(wndpos.x + wndsize.x, dpos.y + hdr_size), colr.to_imcolor());

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + wndpadding);
    if (right_align) {
        draw_ralign_text(title);
    } else {
        ImGui::Text(title);
    }
}

static bool draw_dip_switch(int index, bool value)
{
    ImVec2 dpos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetFrameHeight() * 2;
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

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 slide_dpos_min = value ? dpos : ImVec2(dpos.x, dpos.y + height / 2);
    ImVec2 slide_dpos_max = slide_dpos_min + ImVec2(width, height / 2);

    draw_list->AddRectFilled(dpos, ImVec2(dpos.x + width, dpos.y + height), bgcolor.to_imcolor());
    draw_list->AddRectFilled(slide_dpos_min, slide_dpos_max, IM_COL32(255, 255, 255, 255));

    return value;
}

static constexpr int PANEL_SIZE = 350;
static constexpr color PANEL_BGCOLOR(36, 36, 36, 255);
static constexpr color HEADER_BGCOLOR(48, 82, 121, 255);
static constexpr color SUBHDR_BGCOLOR = HEADER_BGCOLOR.brighter(50);


void emu_gui::draw_help_content()
{}

void emu_gui::draw_settings_content()
{
    ImGui::PushFont(m_txt_font);
    draw_header("Cabinet DIP switches", SUBHDR_BGCOLOR, true);
    ImGui::PopFont();
    ImGui::NewLine();

    float sw_txtpos[5];

    ImGui::SetCursorPosX(ImGui::GetFrameHeight() * 2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20, 0));
    for (int i = 7; i >= 3; --i)
    {
        sw_txtpos[i - 3] = ImGui::GetCursorPosX();

        bool cur_val = m_emu->get_switch(i);
        bool upd_val = draw_dip_switch(i, cur_val);
        m_emu->set_switch(i, upd_val);

        if (i != 3) {
            ImGui::SameLine();
        }
    }
    ImGui::PopStyleVar();

    ImGui::PushFont(m_txt_font);
    for (int i = 7; i >= 3; --i)
    {
        char sw_name[] = { 'D', 'I', 'P', char(0x30 + i), '\0' };
        ImGui::SetCursorPosX(sw_txtpos[i - 3]);
        ImGui::Text(sw_name);
        ImGui::SameLine();
    }
    ImGui::PopFont();

    ImGui::NewLine();
    ImGui::NewLine();

    draw_ralign_text("See README for how these");
    draw_ralign_text("affect game behavior.");
    ImGui::NewLine();

    ImGui::PushFont(m_txt_font);
    draw_header("Audio", SUBHDR_BGCOLOR, true);
    ImGui::PopFont();
    ImGui::NewLine();
    
    int volume = m_emu->get_volume();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
    ImGui::SliderInt("", &volume, 0, MAX_VOLUME);
    ImGui::PopStyleVar();
    m_emu->set_volume(volume);

    //ImGui::Text("Some long text testsgs");
    //ImGui::Text("Some long text testsgs");
    //ImGui::Text("Some long text testsgs");
    //ImGui::Text("Some long text testsgs");
    //ImGui::Text("Some long text testsgs");
}

void emu_gui::draw_panel(const char* title, void(emu_gui::*content_cb)())
{
    static constexpr ImGuiWindowFlags PANEL_FLAGS =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove;

    const ImVec2 wndpos(m_emu->m_scresX, m_menubar_height);
    const ImVec2 wndsize(PANEL_SIZE, m_emu->m_scresY);

    ImGui::SetNextWindowPos(wndpos);
    ImGui::SetNextWindowSize(wndsize);

    if (ImGui::Begin(title, NULL, PANEL_FLAGS))
    {
        // background
        ImGui::GetWindowDrawList()->AddRectFilled(wndpos,
            ImVec2(wndpos.x + wndsize.x, wndpos.y + wndsize.y),
            PANEL_BGCOLOR.to_imcolor());

        ImGui::PushFont(m_hdr_font);
        ImGui::SetCursorPosY(0); // remove padding
        draw_header(title, HEADER_BGCOLOR, true);
        ImGui::PopFont();

        ImGui::NewLine();

        (this->*content_cb)();

        ImGui::End();
    }
}

void emu_gui::draw()
{
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int new_panel = m_cur_panel;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button("Settings")) {
            new_panel = (m_cur_panel == PANEL_SETTINGS) ?
                PANEL_NONE : PANEL_SETTINGS;
        }
        ImGui::SameLine();

        if (ImGui::Button("Help!")) {
            new_panel = (m_cur_panel == PANEL_HELP) ?
                PANEL_NONE : PANEL_HELP;
        }
        ImGui::SameLine();

        ImGui::Text("FPS: %d", m_fps);
        ImGui::SameLine();
        ImGui::Text("Delta t: %f, min: %f, max: %f", m_deltat, m_deltat_min, m_deltat_max);

        ImGui::EndMainMenuBar();
    }

    if (new_panel != m_cur_panel)
    {
        if (m_cur_panel == PANEL_NONE) {
            set_window_width(m_emu->m_window, m_emu->m_scresX + PANEL_SIZE);
        }
        else if (new_panel == PANEL_NONE) {
            set_window_width(m_emu->m_window, m_emu->m_scresX);
        }
    }
    m_cur_panel = new_panel;

    switch (m_cur_panel)
    {
    case PANEL_HELP:     draw_panel("Help!",    &emu_gui::draw_help_content); break;
    case PANEL_SETTINGS: draw_panel("Settings", &emu_gui::draw_settings_content); break;
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_emu->m_renderer);
}

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
int demo_window()
{
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr) {
        return ERROR("Demo SDL_CreateWindow(): %s", SDL_GetError());
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        return ERROR("Demo SDL_CreateRenderer(): %s", SDL_GetError());
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