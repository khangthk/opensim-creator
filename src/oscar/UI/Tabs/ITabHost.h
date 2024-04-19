#pragma once

#include <oscar/UI/Tabs/ITab.h>
#include <oscar/Utils/UID.h>

#include <concepts>
#include <memory>
#include <utility>

namespace osc
{
    // a virtual interface to something that can host multiple UI tabs
    class ITabHost {
    protected:
        ITabHost() = default;
        ITabHost(const ITabHost&) = default;
        ITabHost(ITabHost&&) noexcept = default;
        ITabHost& operator=(const ITabHost&) = default;
        ITabHost& operator=(ITabHost&&) noexcept = default;
    public:
        virtual ~ITabHost() noexcept = default;

        template<std::derived_from<ITab> T, typename... Args>
        requires std::constructible_from<T, Args&&...>
        UID addTab(Args&&... args)
        {
            return addTab(std::make_unique<T>(std::forward<Args>(args)...));
        }

        UID addTab(std::unique_ptr<ITab> tab)
        {
            return implAddTab(std::move(tab));
        }

        void selectTab(UID tabID)
        {
            implSelectTab(tabID);
        }

        void closeTab(UID tabID)
        {
            implCloseTab(tabID);
        }

        void resetImgui()
        {
            implResetImgui();
        }

        template<std::derived_from<ITab> T, typename... Args>
        requires std::constructible_from<T, Args&&...>
        void addAndSelectTab(Args&&... args)
        {
            const UID tabID = addTab<T>(std::forward<Args>(args)...);
            selectTab(tabID);
        }

    private:
        virtual UID implAddTab(std::unique_ptr<ITab>) = 0;
        virtual void implSelectTab(UID) = 0;
        virtual void implCloseTab(UID) = 0;
        virtual void implResetImgui() {}
    };
}
