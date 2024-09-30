#pragma once

#include <OpenSimCreator/Documents/Model/ObjectPropertyEdit.h>

#include <functional>
#include <memory>
#include <optional>

namespace OpenSim { class Object; }
namespace osc { class IModelStatePair; }
namespace osc { class Widget; }

namespace osc
{
    class ObjectPropertiesEditor final {
    public:
        ObjectPropertiesEditor(
            Widget&,
            std::shared_ptr<const IModelStatePair> targetModel,
            std::function<const OpenSim::Object*()> objectGetter
        );
        ObjectPropertiesEditor(const ObjectPropertiesEditor&) = delete;
        ObjectPropertiesEditor(ObjectPropertiesEditor&&) noexcept;
        ObjectPropertiesEditor& operator=(const ObjectPropertiesEditor&) = delete;
        ObjectPropertiesEditor& operator=(ObjectPropertiesEditor&&) noexcept;
        ~ObjectPropertiesEditor() noexcept;

        // does not actually apply any property changes - the caller should check+apply the return value
        std::optional<ObjectPropertyEdit> onDraw();

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
