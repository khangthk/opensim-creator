#pragma once

#include <oscar/UI/Tabs/ITab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>
#include <SDL_events.h>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class ITabHost; }

namespace osc
{
    class LOGLParallaxMappingTab final : public ITab {
    public:
        static CStringView id();

        explicit LOGLParallaxMappingTab(ParentPtr<ITabHost> const&);
        LOGLParallaxMappingTab(LOGLParallaxMappingTab const&) = delete;
        LOGLParallaxMappingTab(LOGLParallaxMappingTab&&) noexcept;
        LOGLParallaxMappingTab& operator=(LOGLParallaxMappingTab const&) = delete;
        LOGLParallaxMappingTab& operator=(LOGLParallaxMappingTab&&) noexcept;
        ~LOGLParallaxMappingTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnMount() final;
        void implOnUnmount() final;
        bool implOnEvent(SDL_Event const&) final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
