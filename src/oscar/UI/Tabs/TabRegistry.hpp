#pragma once

#include <oscar/UI/Tabs/ITab.hpp>
#include <oscar/UI/Tabs/ITabHost.hpp>
#include <oscar/UI/Tabs/TabRegistryEntry.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/ParentPtr.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

namespace osc
{
    template<typename T>
    concept StandardRegisterableTab =
        std::derived_from<T, ITab> &&
        std::constructible_from<T, ParentPtr<ITabHost> const&> &&
        requires (T) {
            { T::id() } -> std::same_as<CStringView>;
        };

    // container for alphabetically-sorted tab entries
    class TabRegistry final {
    public:
        TabRegistry();
        TabRegistry(TabRegistry const&) = delete;
        TabRegistry(TabRegistry&&) noexcept;
        TabRegistry& operator=(TabRegistry const&) = delete;
        TabRegistry& operator=(TabRegistry&&) noexcept;
        ~TabRegistry() noexcept;

        void registerTab(TabRegistryEntry const&);

        template<StandardRegisterableTab T>
        void registerTab()
        {
            registerTab(TabRegistryEntry{
                T::id(),
                [](ParentPtr<ITabHost> const& h) { return std::make_unique<T>(h); },
            });
        }

        size_t size() const;
        TabRegistryEntry operator[](size_t) const;
        std::optional<TabRegistryEntry> getByName(std::string_view) const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
