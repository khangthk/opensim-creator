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
    class LOGLBlendingTab final : public ITab {
    public:
        static CStringView id();

        explicit LOGLBlendingTab(ParentPtr<ITabHost> const&);
        LOGLBlendingTab(LOGLBlendingTab const&) = delete;
        LOGLBlendingTab(LOGLBlendingTab&&) noexcept;
        LOGLBlendingTab& operator=(LOGLBlendingTab const&) = delete;
        LOGLBlendingTab& operator=(LOGLBlendingTab&&) noexcept;
        ~LOGLBlendingTab() noexcept override;

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
