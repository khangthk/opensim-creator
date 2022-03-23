#include "UiModel.hpp"

#include "src/Maths/AABB.hpp"
#include "src/Maths/Geometry.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/RenderableScene.hpp"
#include "src/OpenSimBindings/StateModifications.hpp"
#include "src/Platform/App.hpp"
#include "src/Platform/Log.hpp"
#include "src/Platform/os.hpp"
#include "src/Utils/Perf.hpp"

#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentPath.h>

#include <memory>
#include <vector>

using namespace osc;

static std::unique_ptr<OpenSim::Model> makeNewModel()
{
    auto rv = std::make_unique<OpenSim::Model>();
    rv->updDisplayHints().set_show_frames(true);
    return rv;
}

class osc::UiModel::Impl final {
public:

    Impl() :
        Impl{makeNewModel()}
    {
    }

    Impl(std::string const& osim) :
        Impl{std::make_unique<OpenSim::Model>(osim)}
    {
    }

    Impl(std::unique_ptr<OpenSim::Model> _model) :
        m_StateModifications{},
        m_Model{std::move(_model)},
        m_Decorations{},
        m_SceneBVH{},
        m_FixupScaleFactor{1.0f},
        m_MaybeSelected{},
        m_MaybeHovered{},
        m_MaybeIsolated{},
        m_UpdatedModelVersion{},
        m_CurrentModelVersion{},
        m_UpdatedStateVersion{},
        m_CurrentStateVersion{},
        m_UpdatedDecorationsVersion{},
        m_CurrentDecorationsVersion{}
    {
    }

    Impl(Impl const& other) :
        m_StateModifications{other.m_StateModifications},
        m_Model{std::make_unique<OpenSim::Model>(*other.m_Model)},
        m_Decorations{},
        m_SceneBVH{},
        m_FixupScaleFactor{other.m_FixupScaleFactor},
        m_MaybeSelected{other.m_MaybeSelected},
        m_MaybeHovered{other.m_MaybeHovered},
        m_MaybeIsolated{other.m_MaybeIsolated},
        m_UpdatedModelVersion{},
        m_CurrentModelVersion{other.m_CurrentModelVersion},
        m_UpdatedStateVersion{},
        m_CurrentStateVersion{other.m_CurrentStateVersion},
        m_UpdatedDecorationsVersion{},
        m_CurrentDecorationsVersion{other.m_CurrentDecorationsVersion}
    {
    }

    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl const&) = delete;
    Impl& operator=(Impl&&) noexcept = default;
    ~Impl() noexcept = default;

    std::unique_ptr<Impl> clone()
    {
        return std::make_unique<Impl>(*this);
    }

    OpenSim::Model const& getModel() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return *m_Model;
    }

    OpenSim::Model& updModel()
    {
        const_cast<Impl&>(*this).updateIfDirty();
        m_CurrentModelVersion = UID{};
        return *m_Model;
    }

    OpenSim::Model& peekModelADVANCED()
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return *m_Model;
    }

    void markModelAsModified()
    {
        m_CurrentModelVersion = UID{};
    }

    void setModel(std::unique_ptr<OpenSim::Model> m)
    {
        m_Model = std::move(m);
        m_CurrentModelVersion = UID{};
    }

    UID getModelVersion() const
    {
        return m_CurrentModelVersion;
    }

    SimTK::State const& getState() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return m_Model->getWorkingState();
    }

    UID getStateVersion() const
    {
        return m_CurrentStateVersion;
    }

    void pushCoordinateEdit(OpenSim::Coordinate const& c, CoordinateEdit const& ce)
    {
        m_StateModifications.pushCoordinateEdit(c, ce);
        m_CurrentStateVersion = UID{};
    }

    bool removeCoordinateEdit(OpenSim::Coordinate const& c)
    {
        if (m_StateModifications.removeCoordinateEdit(c))
        {
            m_CurrentStateVersion = UID{};
            return true;
        }
        else
        {
            return false;
        }
    }

    nonstd::span<ComponentDecoration const> getSceneDecorations() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return m_Decorations;
    }

    BVH const& getSceneBVH() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return m_SceneBVH;
    }

    float getFixupScaleFactor() const
    {
        return m_FixupScaleFactor;
    }

    void setFixupScaleFactor(float sf)
    {
        m_FixupScaleFactor = sf;
        m_CurrentDecorationsVersion = UID{};
    }

    AABB getSceneAABB() const
    {
        auto const& bvh = getSceneBVH();
        if (!bvh.nodes.empty())
        {
            return bvh.nodes[0].bounds;
        }
        else
        {
            return AABB{};
        }
    }

    glm::vec3 getSceneDimensions() const
    {
        return Dimensions(getSceneAABB());
    }

    float getSceneLongestDimension() const
    {
        return LongestDim(getSceneAABB());
    }

    float getRecommendedScaleFactor() const
    {
        // generate decorations as if they were empty-sized and union their
        // AABBs to get an idea of what the "true" scale of the model probably
        // is (without the model containing oversized frames, etc.)
        std::vector<ComponentDecoration> ses;
        GenerateModelDecorations(getModel(), getState(), 0.0f, ses, getSelected(), getHovered());

        if (ses.empty())
        {
            return 1.0f;
        }

        AABB aabb = ses[0].worldspaceAABB;
        for (size_t i = 1; i < ses.size(); ++i)
        {
            aabb = Union(aabb, ses[i].worldspaceAABB);
        }

        float longest = LongestDim(aabb);
        float rv = 1.0f;

        while (longest < 0.1)
        {
            longest *= 10.0f;
            rv /= 10.0f;
        }

        return rv;
    }

    bool isDirty() const
    {
        return m_CurrentModelVersion != m_UpdatedModelVersion ||
               m_CurrentStateVersion != m_UpdatedStateVersion ||
               m_CurrentDecorationsVersion != m_UpdatedDecorationsVersion;
    }

    void setDirty(bool v)
    {
        if (v)
        {
            m_CurrentModelVersion = UID{};
            m_CurrentStateVersion = UID{};
            m_CurrentDecorationsVersion = UID{};
        }
        else
        {
            m_UpdatedModelVersion = m_CurrentModelVersion;
            m_UpdatedStateVersion = m_CurrentStateVersion;
            m_UpdatedDecorationsVersion = m_CurrentDecorationsVersion;
        }
    }

    void updateIfDirty()
    {
        if (m_CurrentModelVersion != m_UpdatedModelVersion)
        {
            // a model update always induces a state + decorations update also
            if (m_CurrentStateVersion == m_UpdatedStateVersion)
            {
                m_CurrentStateVersion = UID{};
            }
            if (m_CurrentDecorationsVersion == m_UpdatedDecorationsVersion)
            {
                m_CurrentDecorationsVersion = UID{};
            }
        }
        else if (m_CurrentStateVersion != m_UpdatedStateVersion)
        {
            // a state update always induces a decorations update also
            if (m_CurrentDecorationsVersion == m_UpdatedDecorationsVersion)
            {
                m_CurrentDecorationsVersion = UID{};
            }
        }

        if (m_CurrentModelVersion != m_UpdatedModelVersion)
        {
            OSC_PERF("model update");

            m_Model->buildSystem();
            m_Model->initializeState();

            m_UpdatedModelVersion = m_CurrentModelVersion;  // reset flag
        }

        if (m_CurrentStateVersion != m_UpdatedStateVersion)
        {
            OSC_PERF("state update");

            {
                OSC_PERF("apply state modifications");
                m_StateModifications.applyToState(*m_Model, m_Model->updWorkingState());
            }

            {
                OSC_PERF("equilibriate muscles");
                m_Model->equilibrateMuscles(m_Model->updWorkingState());
            }

            {
                OSC_PERF("realize velocity");
                m_Model->realizeVelocity(m_Model->updWorkingState());
            }

            m_UpdatedStateVersion = m_CurrentStateVersion;  // reset flag
        }

        if (m_CurrentDecorationsVersion != m_UpdatedDecorationsVersion)
        {
            OSC_PERF("decoration update");

            {
                OSC_PERF("generate decorations");
                GenerateModelDecorations(*m_Model,
                                         m_Model->updWorkingState(),
                                         m_FixupScaleFactor,
                                         m_Decorations,
                                         FindComponent(*m_Model, m_MaybeSelected),
                                         FindComponent(*m_Model, m_MaybeHovered));
            }

            {
                OSC_PERF("generate BVH");
                UpdateSceneBVH(m_Decorations, m_SceneBVH);
            }

            m_UpdatedDecorationsVersion = m_CurrentDecorationsVersion;
        }
    }

    bool hasSelected() const
    {
        return FindComponent(*m_Model, m_MaybeSelected);
    }

    OpenSim::Component const* getSelected() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return FindComponent(*m_Model, m_MaybeSelected);
    }

    OpenSim::Component* updSelected()
    {
        const_cast<Impl&>(*this).updateIfDirty();
        OpenSim::Component* c = FindComponentMut(*m_Model, m_MaybeSelected);
        if (c)
        {
            m_CurrentModelVersion = {};
        }
        return c;
    }

    void setSelected(OpenSim::Component const* c)
    {
        auto oldSelection = m_MaybeSelected;

        if (c)
        {
            m_MaybeSelected = c->getAbsolutePath();
        }
        else
        {
            m_MaybeSelected = {};
        }

        if (m_MaybeSelected != oldSelection)
        {
            m_CurrentDecorationsVersion = UID{};
        }
    }

    bool selectionHasTypeHashCode(size_t v) const
    {
        OpenSim::Component const* selected = getSelected();
        return selected && typeid(*selected).hash_code() == v;
    }

    bool hasHovered() const
    {
        return FindComponent(*m_Model, m_MaybeHovered);
    }

    OpenSim::Component const* getHovered() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return FindComponent(*m_Model, m_MaybeHovered);
    }

    OpenSim::Component* updHovered()
    {
        const_cast<Impl&>(*this).updateIfDirty();
        OpenSim::Component* c = FindComponentMut(*m_Model, m_MaybeHovered);
        if (c)
        {
            m_CurrentModelVersion = UID{};
        }
        return c;
    }

    void setHovered(OpenSim::Component const* c)
    {
        auto oldHovered = m_MaybeHovered;

        if (c)
        {
            m_MaybeHovered = c->getAbsolutePath();
        }
        else
        {
            m_MaybeHovered = {};
        }

        if (m_MaybeHovered != oldHovered)
        {
            m_CurrentDecorationsVersion = UID{};
        }
    }

    OpenSim::Component const* getIsolated() const
    {
        const_cast<Impl&>(*this).updateIfDirty();
        return FindComponent(*m_Model, m_MaybeIsolated);
    }

    OpenSim::Component* updIsolated()
    {
        const_cast<Impl&>(*this).updateIfDirty();
        OpenSim::Component* c = FindComponentMut(*m_Model, m_MaybeIsolated);
        if (c)
        {
            m_CurrentModelVersion = UID{};
        }
        return c;
    }

    void setIsolated(OpenSim::Component const* c)
    {
        auto oldIsolated = m_MaybeIsolated;

        if (c)
        {
            m_MaybeIsolated = c->getAbsolutePath();
        }
        else
        {
            m_MaybeIsolated = {};
        }

        if (m_MaybeIsolated != oldIsolated)
        {
            m_CurrentDecorationsVersion = UID{};
        }
    }

    void setSelectedHoveredAndIsolatedFrom(Impl const& other)
    {
        m_MaybeSelected = other.m_MaybeSelected;
        m_MaybeHovered = other.m_MaybeHovered;
        m_MaybeIsolated = other.m_MaybeIsolated;
    }

private:
    // user-enacted state modifications (e.g. coordinate edits)
    StateModifications m_StateModifications;

    // the model, finalized from its properties
    std::unique_ptr<OpenSim::Model> m_Model;

    // decorations, generated from model's display properties etc.
    std::vector<ComponentDecoration> m_Decorations;

    // scene-level BVH of decoration AABBs
    BVH m_SceneBVH;

    // fixup scale factor of the model
    //
    // this scales up/down the decorations of the model - used for extremely
    // undersized models (e.g. fly leg)
    float m_FixupScaleFactor;

    // (maybe) absolute path to the current selection (empty otherwise)
    OpenSim::ComponentPath m_MaybeSelected;

    // (maybe) absolute path to the current hover (empty otherwise)
    OpenSim::ComponentPath m_MaybeHovered;

    // (maybe) absolute path to the current isolation (empty otherwise)
    OpenSim::ComponentPath m_MaybeIsolated;

    // these version IDs are used to figure out if/when the model/state/decorations
    // need to be updated
    UID m_UpdatedModelVersion;
    UID m_CurrentModelVersion;
    UID m_UpdatedStateVersion;
    UID m_CurrentStateVersion;
    UID m_UpdatedDecorationsVersion;
    UID m_CurrentDecorationsVersion;
};

osc::UiModel::UiModel() :
    m_Impl{new Impl{}}
{
}

osc::UiModel::UiModel(std::string const& osim) :
    m_Impl{new Impl{osim}}
{
}

osc::UiModel::UiModel(std::unique_ptr<OpenSim::Model> _model) :
    m_Impl{new Impl{std::move(_model)}}
{
}

osc::UiModel::UiModel(UiModel const& other) :
    m_Impl{new Impl{*other.m_Impl}}
{
}

osc::UiModel::UiModel(UiModel&&) noexcept = default;
UiModel& osc::UiModel::operator=(UiModel const& other) = default;
osc::UiModel& osc::UiModel::operator=(UiModel&&) noexcept = default;
osc::UiModel::~UiModel() noexcept = default;

OpenSim::Model const& osc::UiModel::getModel() const
{
    return m_Impl->getModel();
}

OpenSim::Model& osc::UiModel::updModel()
{
    return m_Impl->updModel();
}

OpenSim::Model& osc::UiModel::peekModelADVANCED()
{
    return m_Impl->peekModelADVANCED();
}

void osc::UiModel::markModelAsModified()
{
    m_Impl->markModelAsModified();
}

osc::UID osc::UiModel::getModelVersion() const
{
    return m_Impl->getModelVersion();
}

void osc::UiModel::setModel(std::unique_ptr<OpenSim::Model> m)
{
    m_Impl->setModel(std::move(m));
}

SimTK::State const& osc::UiModel::getState() const
{
    return m_Impl->getState();
}

UID osc::UiModel::getStateVersion() const
{
    return m_Impl->getStateVersion();
}

void osc::UiModel::pushCoordinateEdit(OpenSim::Coordinate const& c, CoordinateEdit const& ce)
{
    m_Impl->pushCoordinateEdit(c, ce);
}

bool osc::UiModel::removeCoordinateEdit(OpenSim::Coordinate const& c)
{
    return m_Impl->removeCoordinateEdit(c);
}

nonstd::span<ComponentDecoration const> osc::UiModel::getSceneDecorations() const
{
    return m_Impl->getSceneDecorations();
}

osc::BVH const& osc::UiModel::getSceneBVH() const
{
    return m_Impl->getSceneBVH();
}

float osc::UiModel::getFixupScaleFactor() const
{
    return m_Impl->getFixupScaleFactor();
}

void osc::UiModel::setFixupScaleFactor(float sf)
{
    m_Impl->setFixupScaleFactor(std::move(sf));
}

AABB osc::UiModel::getSceneAABB() const
{
    return m_Impl->getSceneAABB();
}

glm::vec3 osc::UiModel::getSceneDimensions() const
{
    return m_Impl->getSceneDimensions();
}

float osc::UiModel::getSceneLongestDimension() const
{
    return m_Impl->getSceneLongestDimension();
}

float osc::UiModel::getRecommendedScaleFactor() const
{
    return m_Impl->getRecommendedScaleFactor();
}

bool osc::UiModel::isDirty() const
{
    return m_Impl->isDirty();
}

void osc::UiModel::setDirty(bool v)
{
    m_Impl->setDirty(std::move(v));
}

void osc::UiModel::updateIfDirty()
{
    m_Impl->updateIfDirty();
}

bool osc::UiModel::hasSelected() const
{
    return m_Impl->hasSelected();
}

OpenSim::Component const* osc::UiModel::getSelected() const
{
    return m_Impl->getSelected();
}

OpenSim::Component* osc::UiModel::updSelected()
{
    return m_Impl->updSelected();
}

void osc::UiModel::setSelected(OpenSim::Component const* c)
{
    m_Impl->setSelected(std::move(c));
}

bool osc::UiModel::selectionHasTypeHashCode(size_t v) const
{
    return m_Impl->selectionHasTypeHashCode(std::move(v));
}

bool osc::UiModel::hasHovered() const
{
    return m_Impl->hasHovered();
}

OpenSim::Component const* osc::UiModel::getHovered() const
{
    return m_Impl->getHovered();
}

OpenSim::Component* osc::UiModel::updHovered()
{
    return m_Impl->updHovered();
}

void osc::UiModel::setHovered(OpenSim::Component const* c)
{
    m_Impl->setHovered(std::move(c));
}

OpenSim::Component const* osc::UiModel::getIsolated() const
{
    return m_Impl->getIsolated();
}

OpenSim::Component* osc::UiModel::updIsolated()
{
    return m_Impl->updIsolated();
}

void osc::UiModel::setIsolated(OpenSim::Component const* c)
{
    m_Impl->setIsolated(std::move(c));
}

void osc::UiModel::setSelectedHoveredAndIsolatedFrom(UiModel const& uim)
{
    m_Impl->setSelectedHoveredAndIsolatedFrom(*uim.m_Impl);
}
