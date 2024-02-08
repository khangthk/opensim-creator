#include "ui_context.hpp"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <ImGuizmo.h>  // care: must come after imgui.h
#include <implot.h>
#include <oscar/Platform/App.hpp>
#include <oscar/Platform/ResourceLoader.hpp>
#include <oscar/Platform/ResourcePath.hpp>
#include <oscar/UI/ImGuiHelpers.hpp>
#include <oscar/UI/ui_graphics_backend.hpp>
#include <oscar/Utils/Perf.hpp>
#include <SDL_events.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <string>

using namespace osc;
namespace ranges = std::ranges;

namespace
{
    constexpr auto c_IconRanges = std::to_array<ImWchar>({ ICON_MIN_FA, ICON_MAX_FA, 0 });

    template<ranges::contiguous_range Container>
    std::unique_ptr<typename Container::value_type[]> ToOwned(Container const& c)
    {
        using value_type = typename Container::value_type;
        using std::size;
        using std::begin;
        using std::end;

        auto rv = std::make_unique<value_type[]>(size(c));
        std::copy(begin(c), end(c), rv.get());
        return rv;
    }

    void AddResourceAsFont(
        ImFontConfig const& config,
        ImFontAtlas& atlas,
        ResourcePath const& path,
        ImWchar const* glyphRanges = nullptr)
    {
        std::string baseFontData = App::slurp(path);
        atlas.AddFontFromMemoryTTF(
            ToOwned(baseFontData).release(),  // ImGui takes ownership
            static_cast<int>(baseFontData.size()) + 1,  // +1 for NUL
            config.SizePixels,
            &config,
            glyphRanges
        );
    }
}

void osc::ui::context::Init()
{
    // init ImGui top-level context
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // make it so that windows can only ever be moved from the title bar
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    // load application-level ImGui config, then the user one,
    // so that the user config takes precedence
    {
        std::string const defaultINIData = App::slurp("imgui_base_config.ini");
        ImGui::LoadIniSettingsFromMemory(defaultINIData.data(), defaultINIData.size());

        // CARE: the reason this filepath is `static` is because ImGui requires that
        // the string outlives the ImGui context
        static std::string const s_UserImguiIniFilePath = (App::get().getUserDataDirPath() / "imgui.ini").string();

        ImGui::LoadIniSettingsFromDisk(s_UserImguiIniFilePath.c_str());
        io.IniFilename = s_UserImguiIniFilePath.c_str();
    }

    ImFontConfig baseConfig;
    baseConfig.SizePixels = 15.0f;
    baseConfig.PixelSnapH = true;
    baseConfig.OversampleH = 2;
    baseConfig.OversampleV = 2;
    AddResourceAsFont(baseConfig, *io.Fonts, "oscar/fonts/Ruda-Bold.ttf");

    // add FontAwesome icon support
    {
        ImFontConfig config = baseConfig;
        config.MergeMode = true;
        config.GlyphMinAdvanceX = std::floor(1.5f * config.SizePixels);
        config.GlyphMaxAdvanceX = std::floor(1.5f * config.SizePixels);
        AddResourceAsFont(config, *io.Fonts, "oscar/fonts/fa-solid-900.ttf", c_IconRanges.data());
    }

    // init ImGui for SDL2 /w OpenGL
    ImGui_ImplSDL2_InitForOpenGL(
        App::upd().updUndleryingWindow(),
        App::upd().updUnderlyingOpenGLContext()
    );

    // init ImGui for OpenGL
    graphics_backend::Init();

    ImGuiApplyDarkTheme();

    // init extra parts (plotting, gizmos, etc.)
    ImPlot::CreateContext();
}

void osc::ui::context::Shutdown()
{
    ImPlot::DestroyContext();

    graphics_backend::Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

bool osc::ui::context::OnEvent(SDL_Event const& e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);

    ImGuiIO const& io  = ImGui::GetIO();

    bool handledByImgui = false;

    if (io.WantCaptureKeyboard && (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP))
    {
        handledByImgui = true;
    }

    if (io.WantCaptureMouse && (e.type == SDL_MOUSEWHEEL || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEBUTTONDOWN))
    {
        handledByImgui = true;
    }

    return handledByImgui;
}

void osc::ui::context::NewFrame()
{
    graphics_backend::NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // extra parts
    ImGuizmo::BeginFrame();
}

void osc::ui::context::Render()
{
    {
        OSC_PERF("ImGuiRender/Render");
        ImGui::Render();
    }

    {
        OSC_PERF("ImGuiRender/ImGui_ImplOscarGfx_RenderDrawData");
        graphics_backend::RenderDrawData(ImGui::GetDrawData());
    }
}
