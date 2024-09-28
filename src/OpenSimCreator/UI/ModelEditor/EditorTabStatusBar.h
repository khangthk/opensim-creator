#pragma once

#include <memory>

namespace osc { class IEditorAPI; }
namespace osc { class IModelStatePair; }
namespace osc { class MainUIScreen; }

namespace osc
{
    class EditorTabStatusBar final {
    public:
        EditorTabStatusBar(
            MainUIScreen&,
            IEditorAPI*,
            std::shared_ptr<IModelStatePair>
        );
        EditorTabStatusBar(const EditorTabStatusBar&) = delete;
        EditorTabStatusBar(EditorTabStatusBar&&) noexcept;
        EditorTabStatusBar& operator=(const EditorTabStatusBar&) = delete;
        EditorTabStatusBar& operator=(EditorTabStatusBar&&) noexcept;
        ~EditorTabStatusBar() noexcept;

        void onDraw();

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
