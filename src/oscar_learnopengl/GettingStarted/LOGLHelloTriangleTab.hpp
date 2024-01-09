#pragma once

#include <oscar/UI/Tabs/ITab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class ITabHost; }

namespace osc
{
    class LOGLHelloTriangleTab final : public ITab {
    public:
        static CStringView id();

        explicit LOGLHelloTriangleTab(ParentPtr<ITabHost> const&);
        LOGLHelloTriangleTab(LOGLHelloTriangleTab const&) = delete;
        LOGLHelloTriangleTab(LOGLHelloTriangleTab&&) noexcept;
        LOGLHelloTriangleTab& operator=(LOGLHelloTriangleTab const&) = delete;
        LOGLHelloTriangleTab& operator=(LOGLHelloTriangleTab&&) noexcept;
        ~LOGLHelloTriangleTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
