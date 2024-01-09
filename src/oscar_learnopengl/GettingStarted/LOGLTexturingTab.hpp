#pragma once

#include <oscar/UI/Tabs/ITab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class ITabHost; }

namespace osc
{
    class LOGLTexturingTab final : public ITab {
    public:
        static CStringView id();

        explicit LOGLTexturingTab(ParentPtr<ITabHost> const&);
        LOGLTexturingTab(LOGLTexturingTab const&) = delete;
        LOGLTexturingTab(LOGLTexturingTab&&) noexcept;
        LOGLTexturingTab& operator=(LOGLTexturingTab const&) = delete;
        LOGLTexturingTab& operator=(LOGLTexturingTab&&) noexcept;
        ~LOGLTexturingTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
