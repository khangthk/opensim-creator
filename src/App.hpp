#pragma once

#include "src/Assertions.hpp"
#include "src/RecentFile.hpp"
#include "src/Screen.hpp"

#include <SDL_events.h>
#include <glm/vec2.hpp>

#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace osc {
    struct Config;
}

namespace osc {

    class App final {
        // set when App is constructed for the first time
        static App* g_Current;

    public:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;

        [[nodiscard]] static App& cur() noexcept {
            OSC_ASSERT(g_Current && "App is not initialized: have you constructed a (singleton) instance of App?");
            return *g_Current;
        }

        [[nodiscard]] static Config const& config() noexcept {
            return cur().getConfig();
        }

        [[nodiscard]] static std::filesystem::path resource(std::string_view s) {
            return cur().getResource(s);
        }

        // init app by loading config from default location
        App();
        App(App const&) = delete;
        App(App&&) noexcept;
        ~App() noexcept;

        App& operator=(App const&) = delete;
        App& operator=(App&&) noexcept;

        // start showing the supplied screen
        void show(std::unique_ptr<Screen>);

        // construct `TScreen` with `Args` and start showing it
        template<typename TScreen, typename... Args>
        void show(Args&&... args) {
            show(std::make_unique<TScreen>(std::forward<Args>(args)...));
        }

        // request the app transitions to a new sreen
        //
        // this is a *request* that `App` will fulfill at a later time. App will
        // first call `on_unmount` on the current screen, fully destroy the current
        // screen, then call `on_mount` on the new screen and make the new screen
        // the current screen
        void requestTransition(std::unique_ptr<Screen>);

        // construct `TScreen` with `Args` then request the app transitions to it
        template<typename TScreen, typename... Args>
        void requestTransition(Args&&... args) {
            requestTransition(std::make_unique<TScreen>(std::forward<Args>(args)...));
        }

        // request the app quits as soon as it can (usually after it's finished with a
        // screen method)
        void requestQuit();

        // returns current window dimensions (integer)
        [[nodiscard]] glm::ivec2 idims() const noexcept;

        // returns current window dimensions (float)
        [[nodiscard]] glm::vec2 dims() const noexcept;

        // returns current window aspect ratio
        [[nodiscard]] float aspectRatio() const noexcept;

        // hides mouse in screen and makes it operate relative per-frame
        void setRelativeMouseMode() noexcept;

        // makes the application window fullscreen
        void makeFullscreen();

        // makes the application window windowed (as opposed to fullscreen)
        void makeWindowed();

        // returns number of MSXAA samples multisampled rendererers should use
        [[nodiscard]] int getSamples() const noexcept;

        // sets the number of MSXAA samples multisampled renderered should use
        //
        // throws if arg > max_samples()
        void setSamples(int);

        // returns the maximum number of MSXAA samples the backend supports
        [[nodiscard]] int maxSamples() const noexcept;

        // returns true if the application is rendering in debug mode
        //
        // screen/tab/widget implementations should use this to decide whether
        // to draw extra debug elements
        [[nodiscard]] bool isInDebugMode() const noexcept;
        void enableDebugMode();
        void disableDebugMode();

        [[nodiscard]] bool isVsyncEnabled() const noexcept;
        void enableVsync();
        void disableVsync();

        [[nodiscard]] Config const& getConfig() const noexcept;

        // get full path to runtime resource in `resources/` dir
        [[nodiscard]] std::filesystem::path getResource(std::string_view) const noexcept;

        // returns the contents of a resource in a string
        [[nodiscard]] std::string slurpResource(std::string_view) const;

        // returns all files that were recently opened by the user in the app
        //
        // the list is persisted between app boots
        [[nodiscard]] std::vector<RecentFile> getRecentFiles() const;

        // add a file to the recently opened files list
        //
        // this addition is persisted between app boots
        void addRecentFile(std::filesystem::path const&);
    };

    // ImGui support
    //
    // these methods are specialized for OSC (config, fonts, themeing, etc.)
    //
    // these methods should be called by each `Screen` implementation. The reason they aren't
    // automatically integrated into App/Screen is because some screens might want very tight
    // control over ImGui (e.g. recycling contexts, aggro-resetting contexts)

    void ImGuiInit();  // init ImGui context (/w osc settings)
    void ImGuiShutdown();  // shutdown ImGui context
    bool ImGuiOnEvent(SDL_Event const&);  // returns true if ImGui has handled the event
    void ImGuiNewFrame();  // should be called at the start of `draw()`
    void ImGuiRender();  // should be called at the end of `draw()`
}
