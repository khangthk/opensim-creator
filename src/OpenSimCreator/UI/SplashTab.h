#pragma once

#include <oscar/UI/Tabs/Tab.h>

namespace osc { class IMainUIStateAPI; }
namespace osc { template<typename T> class ParentPtr; }

namespace osc
{
    class SplashTab final : public Tab {
    public:
        explicit SplashTab(const ParentPtr<IMainUIStateAPI>&);

    private:
        void impl_on_mount() final;
        void impl_on_unmount() final;
        bool impl_on_event(Event&) final;
        void impl_on_draw_main_menu() final;
        void impl_on_draw() final;

        class Impl;
        OSC_WIDGET_DATA_GETTERS(Impl);
    };
}
