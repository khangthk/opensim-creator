#pragma once

#include <memory>

namespace osc { class UndoRedoBase; }

namespace osc
{
    class RedoButton final {
    public:
        explicit RedoButton(std::shared_ptr<UndoRedoBase>);
        RedoButton(const RedoButton&) = delete;
        RedoButton(RedoButton&&) noexcept = default;
        RedoButton& operator=(const RedoButton&) = delete;
        RedoButton& operator=(RedoButton&&) noexcept = default;
        ~RedoButton() noexcept;

        void onDraw();
    private:
        std::shared_ptr<UndoRedoBase> m_UndoRedo;
    };
}
